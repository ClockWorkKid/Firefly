/*
 12-3-09
 Nathan Seidle
 SparkFun Electronics
 
 OpenLog hardware and firmware are released under the Creative Commons Share Alike v3.0 license.
 http://creativecommons.org/licenses/by-sa/3.0/
 Feel free to use, distribute, and sell varients of OpenLog. All we ask is that you include attribution of 'Based on OpenLog by SparkFun'.
 
 OpenLog is a simple serial logger based on the ATmega328 running at 16MHz. The ATmega328
 is able to talk to high capacity (larger than 2GB) SD cards. The whole purpose of this
 logger was to create a logger that just powered up and worked. OpenLog ships with an Arduino/Optiboot 
 115200bps serial bootloader running at 16MHz so you can load new firmware with a simple serial
 connection.
 
 This version has the command line interface stripped out in order to simplify the overall program and increase 
 the receive buffer (RAM). You can still configure some of the options via the config.txt file.

 For even faster logging, see OpenLog_Minimal.
 
 */
#define __PROG_TYPES_COMPAT__ //Needed to get SerialPort.h to work in Arduino 1.6.x

#include <SPI.h>
#include <SdFat.h> //We do not use the built-in SD.h file because it calls Serial.print
#include <SerialPort.h> //This is a new/beta library written by Bill Greiman. You rock Bill!
#include <EEPROM.h>
#include <FreeStack.h> //Allows us to print the available stack/RAM size

SerialPort<0, 850, 0> NewSerial;
//This is a very important buffer declaration. This sets the <port #, rx size, tx size>. We set
//the TX buffer to zero because we will be spending most of our time needing to buffer the incoming (RX) characters.

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers

#define SD_CHIP_SELECT 10 //On OpenLog this is pin 10

//Debug turns on (1) or off (0) a bunch of verbose debug statements. Normally use (0)
//#define DEBUG  1
#define DEBUG  0

//The bigger the receive buffer, the less likely we are to drop characters at high speed. However, the ATmega has a limited amount of
//RAM. This debug mode allows us to view available RAM at various stages of the program
//#define RAM_TESTING  1 //On
#define RAM_TESTING  0 //Off

#define CFG_FILENAME "config.txt" //This is the name of the file that contains the unit settings

#define MAX_CFG "115200,103,214,0,1,1,0\0" //= 115200 bps, escape char of ASCII(103), 214 times, new log mode, verbose on, echo on, ignore RX false. 
#define CFG_LENGTH (strlen(MAX_CFG) + 1) //Length of text found in config file. strlen ignores \0 so we have to add it back 
#define SEQ_FILENAME "SEQLOG00.TXT" //This is the name for the file when you're in sequential modeu

//Internal EEPROM locations for the user settings
#define LOCATION_BAUD_SETTING		0x01
#define LOCATION_SYSTEM_SETTING		0x02
#define LOCATION_FILE_NUMBER_LSB	0x03
#define LOCATION_FILE_NUMBER_MSB	0x04
#define LOCATION_ESCAPE_CHAR		0x05
#define LOCATION_MAX_ESCAPE_CHAR	0x06
#define LOCATION_VERBOSE                0x07
#define LOCATION_ECHO                   0x08
#define LOCATION_BAUD_SETTING_HIGH	0x09
#define LOCATION_BAUD_SETTING_MID	0x0A
#define LOCATION_BAUD_SETTING_LOW	0x0B
#define LOCATION_IGNORE_RX		0x0C

#define BAUD_MIN  300
#define BAUD_MAX  1000000

#define MODE_NEWLOG	0
#define MODE_SEQLOG     1
#define MODE_COMMAND    2

//STAT1 is a general LED and indicates serial traffic
#define STAT1  5 //On PORTD
#define STAT1_PORT  PORTD
#define STAT2  5 //On PORTB
#define STAT2_PORT  PORTB
const byte statled1 = 5;  //This is the normal status LED
const byte statled2 = 13; //This is the SPI LED, indicating SD traffic

//Blinking LED error codes
#define ERROR_SD_INIT	  3
#define ERROR_NEW_BAUD	  5
#define ERROR_CARD_INIT   6
#define ERROR_VOLUME_INIT 7
#define ERROR_ROOT_INIT   8
#define ERROR_FILE_OPEN   9

#define OFF   0x00
#define ON    0x01

SdFat sd;

long setting_uart_speed; //This is the baud rate that the system runs at, default is 9600. Can be 300 to 1,000,000
byte setting_system_mode; //This is the mode the system runs in, default is MODE_NEWLOG
byte setting_escape_character; //This is the ASCII character we look for to break logging, default is ctrl+z
byte setting_max_escape_character; //Number of escape chars before break logging, default is 3
byte setting_verbose; //This controls the whether we get extended or simple responses.
byte setting_echo; //This turns on/off echoing at the command prompt
byte setting_ignore_RX; //This flag, when set to 1 will make OpenLog ignore the state of the RX pin when powering up

//Handle errors by printing the error type and blinking LEDs in certain way
//The function will never exit - it loops forever inside blink_error
void systemError(byte error_type)
{
  NewSerial.print(F("Error "));
  switch(error_type)
  {
  case ERROR_CARD_INIT:
    NewSerial.print(F("card.init")); 
    blink_error(ERROR_SD_INIT);
    break;
  case ERROR_VOLUME_INIT:
    NewSerial.print(F("volume.init")); 
    blink_error(ERROR_SD_INIT);
    break;
  case ERROR_ROOT_INIT:
    NewSerial.print(F("root.init")); 
    blink_error(ERROR_SD_INIT);
    break;
  case ERROR_FILE_OPEN:
    NewSerial.print(F("file.open")); 
    blink_error(ERROR_SD_INIT);
    break;
  }
}

void setup(void)
{
  pinMode(statled1, OUTPUT);

  //Power down various bits of hardware to lower power usage  
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();

  //Shut off TWI, Timer2, Timer1, ADC
  ADCSRA &= ~(1<<ADEN); //Disable ADC
  ACSR = (1<<ACD); //Disable the analog comparator
  DIDR0 = 0x3F; //Disable digital input buffers on all ADC0-ADC5 pins
  DIDR1 = (1<<AIN1D)|(1<<AIN0D); //Disable digital input buffer on AIN1/0

  power_twi_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_adc_disable();

  read_system_settings(); //Load all system settings from EEPROM

  //Setup UART
  NewSerial.begin(setting_uart_speed);
  if (setting_uart_speed < 500)      // check for slow baud rates
  {
    //There is an error in the Serial library for lower than 500bps. 
    //This fixes it. See issue 163: https://github.com/sparkfun/OpenLog/issues/163
    // redo USART baud rate configuration
    UBRR0 = (F_CPU / (16UL * setting_uart_speed)) - 1;
    UCSR0A &= ~_BV(U2X0);
  }
  NewSerial.print(F("1"));

  //Setup SD & FAT
  if (!sd.begin(SD_CHIP_SELECT, SPI_FULL_SPEED)) systemError(ERROR_CARD_INIT);
  if (!sd.chdir()) systemError(ERROR_ROOT_INIT); //Change to root directory. All new file creation will be in root.

  NewSerial.print(F("2"));

  //Search for a config file and load any settings found. This will over-ride previous EEPROM settings if found.
  read_config_file();

  if(setting_ignore_RX == OFF) //If we are NOT ignoring RX, then
    check_emergency_reset(); //Look to see if the RX pin is being pulled low
}

void loop(void)
{
  //If we are in new log mode, find a new file name to write to
  if(setting_system_mode == MODE_NEWLOG)
  {
    //new_file_name = newlog();
    append_file(newlog()); //Append the file name that newlog() returns
  }

  //If we are in sequential log mode, determine if seqlog.txt has been created or not, and then open it for logging
  if(setting_system_mode == MODE_SEQLOG)
    seqlog();

  while(1); //We should never get this far
}

//Log to a new file everytime the system boots
//Checks the spots in EEPROM for the next available LOG# file name
//Updates EEPROM and then appends to the new log file.
//Limited to 65535 files but this should not always be the case.
char* newlog(void)
{
  byte msb, lsb;
  uint16_t new_file_number;

  SdFile newFile; //This will contain the file for SD writing

  //Combine two 8-bit EEPROM spots into one 16-bit number
  lsb = EEPROM.read(LOCATION_FILE_NUMBER_LSB);
  msb = EEPROM.read(LOCATION_FILE_NUMBER_MSB);

  new_file_number = msb;
  new_file_number = new_file_number << 8;
  new_file_number |= lsb;

  //If both EEPROM spots are 255 (0xFF), that means they are un-initialized (first time OpenLog has been turned on)
  //Let's init them both to 0
  if((lsb == 255) && (msb == 255))
  {
    new_file_number = 0; //By default, unit will start at file number zero
    EEPROM.write(LOCATION_FILE_NUMBER_LSB, 0x00);
    EEPROM.write(LOCATION_FILE_NUMBER_MSB, 0x00);
  }

  //The above code looks like it will forever loop if we ever create 65535 logs
  //Let's quit if we ever get to 65534
  //65534 logs is quite possible if you have a system with lots of power on/off cycles
  if(new_file_number == 65534)
  {
    //Gracefully drop out to command prompt with some error
    NewSerial.print(F("!Too many logs:1!"));
    return(0); //Bail!
  }

  //If we made it this far, everything looks good - let's start testing to see if our file number is the next available

  //Search for next available log spot
  //char new_file_name[] = "LOG00000.TXT";
  static char new_file_name[13];
  while(1)
  {
    sprintf_P(new_file_name, PSTR("LOG%05d.TXT"), new_file_number); //Splice the new file number into this file name

    //Try to open file, if fail (file doesn't exist), then break
    if (newFile.open(new_file_name, O_CREAT | O_EXCL | O_WRITE)) break;

    //Try to open file and see if it is empty. If so, use it.
    if (newFile.open(new_file_name, O_READ)) 
    {
      if (newFile.fileSize() == 0)
      {
        newFile.close();        // Close this existing file we just opened.
        return(new_file_name);  // Use existing empty file.
      }
      newFile.close(); // Close this existing file we just opened.
    }

    //Try the next number
    new_file_number++;
    if(new_file_number > 65533) //There is a max of 65534 logs
    {
      NewSerial.print(F("!Too many logs:2!"));
      return(0); //Bail!
    }
  }
  newFile.close(); //Close this new file we just opened

  new_file_number++; //Increment so the next power up uses the next file #

  //Record new_file number to EEPROM
  lsb = (byte)(new_file_number & 0x00FF);
  msb = (byte)((new_file_number & 0xFF00) >> 8);

  EEPROM.write(LOCATION_FILE_NUMBER_LSB, lsb); // LSB

  if (EEPROM.read(LOCATION_FILE_NUMBER_MSB) != msb)
    EEPROM.write(LOCATION_FILE_NUMBER_MSB, msb); // MSB

#if DEBUG
  NewSerial.print(F("\nCreated new file: "));
  NewSerial.println(new_file_name);
#endif

  //  append_file(new_file_name);
  return(new_file_name);
}

//Log to the same file every time the system boots, sequentially
//Checks to see if the file SEQLOG.txt is available
//If not, create it
//If yes, append to it
//Return 0 on error
//Return anything else on sucess
void seqlog(void)
{
  SdFile seqFile;

  char sequentialFileName[strlen(SEQ_FILENAME)]; //Limited to 8.3
  strcpy_P(sequentialFileName, PSTR(SEQ_FILENAME)); //This is the name of the config file. 'config.sys' is probably a bad idea.

  //Try to create sequential file
  if (!seqFile.open(sequentialFileName, O_CREAT | O_WRITE))
  {
    NewSerial.print(F("Error creating SEQLOG\n"));
    return;
  }

  seqFile.close(); //Close this new file we just opened

  append_file(sequentialFileName); 
}

//This is the most important function of the device. These loops have been tweaked as much as possible.
//Modifying this loop may negatively affect how well the device can record at high baud rates.
//Appends a stream of serial data to a given file
//Assumes the currentDirectory variable has been set before entering the routine
//Does not exit until Ctrl+z (ASCII 26) is received
//Returns 0 on error
//Returns 1 on success
byte append_file(char* file_name)
{
  SdFile workingFile;

  // O_CREAT - create the file if it does not exist
  // O_APPEND - seek to the end of the file prior to each write
  // O_WRITE - open for write
  if (!workingFile.open(file_name, O_CREAT | O_APPEND | O_WRITE)) systemError(ERROR_FILE_OPEN);

  if (workingFile.fileSize() == 0) {
    //This is a trick to make sure first cluster is allocated - found in Bill's example/beta code
    workingFile.rewind();
    workingFile.sync();
  }

  //This is the 2nd buffer. It pulls from the larger Serial buffer as quickly as possible.
  //The built-in Arduino serial buffer is 64 bytes: https://www.arduino.cc/en/Serial/Available
  const byte LOCAL_BUFF_SIZE = 128;
  byte localBuffer[LOCAL_BUFF_SIZE];

  const unsigned int MAX_IDLE_TIME_MSEC = 500; //The number of milliseconds before unit goes to sleep
  unsigned long lastSyncTime = millis(); //Keeps track of the last time the file was synced

#if DEBUG
  NewSerial.print(F("FreeStack: "));
  NewSerial.println(FreeStack());
#endif

  NewSerial.print(F("<")); //give a different prompt to indicate no echoing
  digitalWrite(statled1, HIGH); //Turn on indicator LED

  //Start recording incoming characters
  while(1) { //Infinite loop

    byte charsToRecord = NewSerial.read(localBuffer, sizeof(localBuffer)); //Read characters from global buffer into the local buffer
    if (charsToRecord > 0) {
      //Scan the local buffer for esacape characters
      //In the light version of OpenLog, we don't check for escape characters

      workingFile.write(localBuffer, charsToRecord); //Record the buffer to the card

      STAT1_PORT ^= (1<<STAT1); //Toggle the STAT1 LED each time we record the buffer
    }
    //No characters recevied?
    else if( (millis() - lastSyncTime) > MAX_IDLE_TIME_MSEC) { //If we haven't received any characters in 2s, goto sleep
      workingFile.sync(); //Sync the card before we go to sleep

      STAT1_PORT &= ~(1<<STAT1); //Turn off stat LED to save power

      power_timer0_disable(); //Shut down peripherals we don't need
      power_spi_disable();
      sleep_mode(); //Stop everything and go to sleep. Wake up if serial character received

      power_spi_enable(); //After wake up, power up peripherals
      power_timer0_enable();

      lastSyncTime = millis(); //Reset the last sync time to now
    }
  }

  return(1); //Success!
}

//The following are system functions needed for basic operation
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Blinks the status LEDs to indicate a type of error
void blink_error(byte ERROR_TYPE) {
  while(1) {
    for(int x = 0 ; x < ERROR_TYPE ; x++) {
      digitalWrite(statled1, HIGH);
      delay(200);
      digitalWrite(statled1, LOW);
      delay(200);
    }

    delay(2000);
  }
}

//Check to see if we need an emergency UART reset
//Scan the RX pin for 2 seconds
//If it's low the entire time, then return 1
void check_emergency_reset(void)
{
  pinMode(0, INPUT); //Turn the RX pin into an input
  digitalWrite(0, HIGH); //Push a 1 onto RX pin to enable internal pull-up

  //Quick pin check
  if(digitalRead(0) == HIGH) return;

  //Disable SPI so that we control the LEDs
  SPI.end();

  //Wait 2 seconds, blinking LEDs while we wait
  pinMode(statled2, OUTPUT);
  digitalWrite(statled2, HIGH); //Set the STAT2 LED
  for(byte i = 0 ; i < 40 ; i++)
  {
    delay(25);
    toggleLED(statled1); //Blink the stat LEDs

    if(digitalRead(0) == HIGH) return; //Check to see if RX is not low anymore

    delay(25);
    toggleLED(statled2); //Blink the stat LEDs

    if(digitalRead(0) == HIGH) return; //Check to see if RX is not low anymore
  }		

  //If we make it here, then RX pin stayed low the whole time
  set_default_settings(); //Reset baud, escape characters, escape number, system mode

  //Try to setup the SD card so we can record these new settings
  if (!sd.begin(SD_CHIP_SELECT, SPI_HALF_SPEED)) systemError(ERROR_CARD_INIT);
  if (!sd.chdir()) systemError(ERROR_ROOT_INIT); //Change to root directory

  record_config_file(); //Record new config settings

  //Disable SPI so that we control the LEDs
  SPI.end();

  pinMode(statled1, OUTPUT);
  pinMode(statled2, OUTPUT);
  digitalWrite(statled1, HIGH);
  digitalWrite(statled2, HIGH);

  //Now sit in forever loop indicating system is now at 9600bps
  while(1)
  {
    delay(500);
    toggleLED(statled1); //Blink the stat LEDs
    toggleLED(statled2); //Blink the stat LEDs
  }
}

//Resets all the system settings to safe values
void set_default_settings(void)
{
  //Reset UART to 9600bps
  writeBaud(9600);

  //Reset system to new log mode
  EEPROM.write(LOCATION_SYSTEM_SETTING, MODE_NEWLOG);

  //Reset escape character to ctrl+z
  EEPROM.write(LOCATION_ESCAPE_CHAR, 26); 

  //Reset number of escape characters to 3
  EEPROM.write(LOCATION_MAX_ESCAPE_CHAR, 3);

  //Reset verbose responses to on
  EEPROM.write(LOCATION_VERBOSE, ON);

  //Reset echo to on
  EEPROM.write(LOCATION_ECHO, ON);

  //Reset the ignore RX to 'Pay attention to RX!'
  EEPROM.write(LOCATION_IGNORE_RX, OFF);

  //These settings are not recorded to the config file
  //We can't do it here because we are not sure the FAT system is init'd
}

//Reads the current system settings from EEPROM
//If anything looks weird, reset setting to default value
void read_system_settings(void)
{
  //Read what the current UART speed is from EEPROM memory
  //Default is 9600
  setting_uart_speed = readBaud(); //Combine the three bytes
  if(setting_uart_speed < BAUD_MIN || setting_uart_speed > BAUD_MAX) 
  {
    setting_uart_speed = 9600; //Reset UART to 9600 if there is no speed stored
    writeBaud(setting_uart_speed); //Record to EEPROM
  }

  //Determine the system mode we should be in
  //Default is NEWLOG mode
  setting_system_mode = EEPROM.read(LOCATION_SYSTEM_SETTING);
  if(setting_system_mode > 5) 
  {
    setting_system_mode = MODE_NEWLOG; //By default, unit will turn on and go to new file logging
    EEPROM.write(LOCATION_SYSTEM_SETTING, setting_system_mode);
  }

  //Read the escape_character
  //ASCII(26) is ctrl+z
  setting_escape_character = EEPROM.read(LOCATION_ESCAPE_CHAR);
  if(setting_escape_character == 0 || setting_escape_character == 255) 
  {
    setting_escape_character = 26; //Reset escape character to ctrl+z
    EEPROM.write(LOCATION_ESCAPE_CHAR, setting_escape_character);
  }

  //Read the number of escape_characters to look for
  //Default is 3
  setting_max_escape_character = EEPROM.read(LOCATION_MAX_ESCAPE_CHAR);
  if(setting_max_escape_character == 255) 
  {
    setting_max_escape_character = 3; //Reset number of escape characters to 3
    EEPROM.write(LOCATION_MAX_ESCAPE_CHAR, setting_max_escape_character);
  }

  //Read whether we should use verbose responses or not
  //Default is true
  setting_verbose = EEPROM.read(LOCATION_VERBOSE);
  if(setting_verbose != ON && setting_verbose != OFF) 
  {
    setting_verbose = ON; //Reset verbose to true
    EEPROM.write(LOCATION_VERBOSE, setting_verbose);
  }

  //Read whether we should echo characters or not
  //Default is true
  setting_echo = EEPROM.read(LOCATION_ECHO);
  if(setting_echo != ON || setting_echo != OFF) 
  {
    setting_echo = ON; //Reset to echo on
    EEPROM.write(LOCATION_ECHO, setting_echo);
  }

  //Read whether we should ignore RX at power up
  //Some users need OpenLog to ignore the RX pin during power up
  //Default is false or ignore
  setting_ignore_RX = EEPROM.read(LOCATION_IGNORE_RX);
  if(setting_ignore_RX > 1) 
  {
    setting_ignore_RX = OFF; //By default we DO NOT ignore RX
    EEPROM.write(LOCATION_IGNORE_RX, setting_ignore_RX);
  }
}

void read_config_file(void)
{
  SdFile configFile;

  if (!sd.chdir()) systemError(ERROR_ROOT_INIT); // open the root directory

  char configFileName[strlen(CFG_FILENAME)]; //Limited to 8.3
  strcpy_P(configFileName, PSTR(CFG_FILENAME)); //This is the name of the config file. 'config.sys' is probably a bad idea.

  //Check to see if we have a config file
  if (!configFile.open(configFileName, O_READ)) {
    //If we don't have a config file already, then create config file and record the current system settings to the file
#if DEBUG
    NewSerial.println(F("No config found - creating default:"));
#endif
    configFile.close();

      //Record the current eeprom settings to the config file
    record_config_file();
    return;
  }

  //If we found the config file then load settings from file and push them into EEPROM
#if DEBUG
  NewSerial.println(F("Found config file!"));
#endif

  //Read up to 20 characters from the file. There may be a better way of doing this...
  char c;
  int len;
  byte settings_string[CFG_LENGTH]; //"115200,103,14,0,1,1\0" = 115200 bps, escape char of ASCII(103), 14 times, new log mode, verbose on, echo on.
  for(len = 0 ; len < CFG_LENGTH ; len++) {
    if( (c = configFile.read()) < 0) break; //We've reached the end of the file
    if(c == '\0') break; //Bail if we hit the end of this string
    settings_string[len] = c;
  }
  configFile.close();

#if DEBUG
  //Print line for debugging
  NewSerial.print(F("Text Settings: "));
  for(int i = 0; i < len; i++)
    NewSerial.write(settings_string[i]);
  NewSerial.println();
  NewSerial.print(F("Len: "));
  NewSerial.println(len);
#endif

  //Default the system settings in case things go horribly wrong
  long new_system_baud = 9600;
  byte new_system_mode = MODE_NEWLOG;
  byte new_system_escape = 26;
  byte new_system_max_escape = 3;
  byte new_system_verbose = ON;
  byte new_system_echo = ON;
  byte new_system_ignore_RX = OFF;  

  //Parse the settings out
  byte i = 0, j = 0, setting_number = 0;
  char new_setting[8]; //Max length of a setting is 6, the bps setting = '115200' plus '\0'
  byte new_setting_int = 0;

  for(i = 0 ; i < len; i++)
  {
    //Pick out one setting from the line of text
    for(j = 0 ; settings_string[i] != ',' && i < len && j < 6 ; )
    {
      new_setting[j] = settings_string[i];
      i++;
      j++;
    }

    new_setting[j] = '\0'; //Terminate the string for array compare
    new_setting_int = atoi(new_setting); //Convert string to int

    if(setting_number == 0) //Baud rate
    {
      new_system_baud = strtolong(new_setting);

      //Basic error checking
      if(new_system_baud < BAUD_MIN || new_system_baud > BAUD_MAX) new_system_baud = 9600; //Default to 9600
    }
    else if(setting_number == 1) //Escape character
    {
      new_system_escape = new_setting_int;
      if(new_system_escape == 0 || new_system_escape > 127) new_system_escape = 26; //Default is ctrl+z
    }
    else if(setting_number == 2) //Max amount escape character
    {
      new_system_max_escape = new_setting_int;
      if(new_system_max_escape > 254) new_system_max_escape = 3; //Default is 3
    }
    else if(setting_number == 3) //System mode
    {
      new_system_mode = new_setting_int;
      if(new_system_mode == 0 || new_system_mode > MODE_COMMAND) new_system_mode = MODE_NEWLOG; //Default is NEWLOG
    }
    else if(setting_number == 4) //Verbose setting
    {
      new_system_verbose = new_setting_int;
      if(new_system_verbose != ON && new_system_verbose != OFF) new_system_verbose = ON; //Default is on
    }
    else if(setting_number == 5) //Echo setting
    {
      new_system_echo = new_setting_int;
      if(new_system_echo != ON && new_system_echo != OFF) new_system_echo = ON; //Default is on
    }
    else if(setting_number == 6) //Ignore RX setting
    {
      new_system_ignore_RX = new_setting_int;
      if(new_system_ignore_RX != ON && new_system_ignore_RX != OFF) new_system_ignore_RX = OFF; //Default is to listen to RX
    }
    else
      //We're done! Stop looking for settings
      break;

    setting_number++;
  }

  //We now have the settings loaded into the global variables. Now check if they're different from EEPROM settings
  boolean recordNewSettings = false;

  if(new_system_baud != setting_uart_speed) {
    //If the baud rate from the file is different from the current setting,
    //Then update the setting to the file setting
    //And re-init the UART
    writeBaud(new_system_baud); //Write this baudrate to EEPROM
    setting_uart_speed = new_system_baud;
    NewSerial.begin(setting_uart_speed); //Move system to new uart speed

    recordNewSettings = true;
  }

  if(new_system_mode != setting_system_mode) {
    //Goto new system mode
    setting_system_mode = new_system_mode;
    EEPROM.write(LOCATION_SYSTEM_SETTING, setting_system_mode);

    recordNewSettings = true;
  }

  if(new_system_escape != setting_escape_character) {
    //Goto new system escape char
    setting_escape_character = new_system_escape;
    EEPROM.write(LOCATION_ESCAPE_CHAR, setting_escape_character); 

    recordNewSettings = true;
  }

  if(new_system_max_escape != setting_max_escape_character) {
    //Goto new max escape
    setting_max_escape_character = new_system_max_escape;
    EEPROM.write(LOCATION_MAX_ESCAPE_CHAR, setting_max_escape_character);

    recordNewSettings = true;
  }

  if(new_system_verbose != setting_verbose) {
    //Goto new verbose setting
    setting_verbose = new_system_verbose;
    EEPROM.write(LOCATION_VERBOSE, setting_verbose);

    recordNewSettings = true;
  }

  if(new_system_echo != setting_echo) {
    //Goto new echo setting
    setting_echo = new_system_echo;
    EEPROM.write(LOCATION_ECHO, setting_echo);

    recordNewSettings = true;
  }

  if(new_system_ignore_RX != setting_ignore_RX) {
    //Goto new ignore setting
    setting_ignore_RX = new_system_ignore_RX;
    EEPROM.write(LOCATION_IGNORE_RX, setting_ignore_RX);

    recordNewSettings = true;
  }

  //We don't want to constantly record a new config file on each power on. Only record when there is a change.
  if(recordNewSettings == true)
    record_config_file(); //If we corrected some values because the config file was corrupt, then overwrite any corruption
#if DEBUG
  else
    NewSerial.println(F("Config file matches system settings"));
#endif

  //All done! New settings are loaded. System will now operate off new config settings found in file.

  //Set flags for extended mode options  
  /* These settings are not used in light mode
  if (setting_verbose == ON)
    feedback_mode |= EXTENDED_INFO;
  else
    feedback_mode &= ((byte)~EXTENDED_INFO);

  if (setting_echo == ON)
    feedback_mode |= ECHO;
  else
    feedback_mode &= ((byte)~ECHO);
    */
}

//Records the current EEPROM settings to the config file
//If a config file exists, it is trashed and a new one is created
void record_config_file(void)
{
  SdFile myFile;

  if (!sd.chdir()) systemError(ERROR_ROOT_INIT); // open the root directory

  char configFileName[strlen(CFG_FILENAME)];
  strcpy_P(configFileName, PSTR(CFG_FILENAME)); //This is the name of the config file. 'config.sys' is probably a bad idea.

  //If there is currently a config file, trash it
  if (myFile.open(configFileName, O_WRITE)) {
    if (!myFile.remove()){
      NewSerial.println(F("Remove config failed"));
      myFile.close(); //Close this file
      return;
    }
  }

  //myFile.close(); //Not sure if we need to close the file before we try to reopen it

  //Create config file
  myFile.open(configFileName, O_CREAT | O_APPEND | O_WRITE);

  //Config was successfully created, now record current system settings to the config file

  char settings_string[CFG_LENGTH]; //"115200,103,214,0,1,1\0" = 115200 bps, escape char of ASCII(103), 214 times, new log mode, verbose on, echo on.

  //Before we read the EEPROM values, they've already been tested and defaulted in the read_system_settings function
  long current_system_baud = readBaud();
  byte current_system_escape = EEPROM.read(LOCATION_ESCAPE_CHAR);
  byte current_system_max_escape = EEPROM.read(LOCATION_MAX_ESCAPE_CHAR);
  byte current_system_mode = EEPROM.read(LOCATION_SYSTEM_SETTING);
  byte current_system_verbose = EEPROM.read(LOCATION_VERBOSE);
  byte current_system_echo = EEPROM.read(LOCATION_ECHO);
  byte current_system_ignore_RX = EEPROM.read(LOCATION_IGNORE_RX);

  //Convert system settings to visible ASCII characters
  sprintf_P(settings_string, PSTR("%ld,%d,%d,%d,%d,%d,%d\0"), current_system_baud, current_system_escape, current_system_max_escape, current_system_mode, current_system_verbose, current_system_echo, current_system_ignore_RX);

  //Record current system settings to the config file
  if(myFile.write(settings_string, strlen(settings_string)) != strlen(settings_string))
    NewSerial.println(F("error writing to file"));

  myFile.println(); //Add a break between lines

  //Add a decoder line to the file
  #define HELP_STR "baud,escape,esc#,mode,verb,echo,ignoreRX\0"
  char helperString[strlen(HELP_STR) + 1]; //strlen is preprocessed but returns one less because it ignores the \0
  strcpy_P(helperString, PSTR(HELP_STR));
  myFile.write(helperString); //Add this string to the file

  myFile.sync(); //Sync all newly written data to card
  myFile.close(); //Close this file
  //Now that the new config file has the current system settings, nothing else to do!
}

//Given a baud rate (long number = four bytes but we only use three), record to EEPROM
void writeBaud(long uartRate)
{
  EEPROM.write(LOCATION_BAUD_SETTING_HIGH, (byte)((uartRate & 0x00FF0000) >> 16));
  EEPROM.write(LOCATION_BAUD_SETTING_MID, (byte)(uartRate >> 8));
  EEPROM.write(LOCATION_BAUD_SETTING_LOW, (byte)uartRate);
}

//Look up the baud rate. This requires three bytes be combined into one long
long readBaud(void)
{
  byte uartSpeedHigh = EEPROM.read(LOCATION_BAUD_SETTING_HIGH);
  byte uartSpeedMid = EEPROM.read(LOCATION_BAUD_SETTING_MID);
  byte uartSpeedLow = EEPROM.read(LOCATION_BAUD_SETTING_LOW);

  long uartSpeed = 0x00FF0000 & ((long)uartSpeedHigh << 16) | ((long)uartSpeedMid << 8) | uartSpeedLow; //Combine the three bytes

  return(uartSpeed); 
}


//End core system functions
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//A rudimentary way to convert a string to a long 32 bit integer
//Used by the read command, in command shell and baud from the system menu
uint32_t strtolong(const char* str)
{
  uint32_t l = 0;
  while(*str >= '0' && *str <= '9')
    l = l * 10 + (*str++ - '0');

  return l;
}

//Given a pin, it will toggle it from high to low or vice versa
void toggleLED(byte pinNumber)
{
  if (digitalRead(pinNumber)) digitalWrite(pinNumber, LOW);
  else digitalWrite(pinNumber, HIGH);
}

