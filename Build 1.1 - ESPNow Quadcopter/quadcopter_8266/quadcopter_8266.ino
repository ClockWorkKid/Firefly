//############################################################################//
//                          PPM Declarations                                  //
//############################################################################//

#define CPU_MHZ 80
#define CHANNEL_NUMBER 8  //set the number of chanels
#define CHANNEL_DEFAULT_VALUE 1100  //set the default servo value
#define FRAME_LENGTH 22500  //set the PPM frame length in microseconds (1ms = 1000µs)
#define PULSE_LENGTH 300  //set the pulse length
#define onState 0  //set polarity of the pulses: 1 is positive, 0 is negative
#define sigPin 5 //set PPM signal output pin on the arduino

int ppm[CHANNEL_NUMBER];

#define THR 1
#define ROL 2
#define PIT 3
#define YAW 0
#define AUX1 4
#define AUX2 5
#define AUX3 6
#define AUX4 7

volatile unsigned long next;
volatile unsigned int ppm_running = 1;

void ppmISR(void) {
  static boolean state = true;

  if (state) {  //start pulse
    digitalWrite(sigPin, onState);
    next = next + (PULSE_LENGTH * CPU_MHZ);
    state = false;
  }
  else { //end pulse and calculate when to start the next pulse
    static byte cur_chan_numb;
    static unsigned int calc_rest;

    digitalWrite(sigPin, !onState);
    state = true;

    if (cur_chan_numb >= CHANNEL_NUMBER) {
      cur_chan_numb = 0;
      calc_rest = calc_rest + PULSE_LENGTH;//
      next = next + ((FRAME_LENGTH - calc_rest) * CPU_MHZ);
      calc_rest = 0;
    }
    else {
      next = next + ((ppm[cur_chan_numb] - PULSE_LENGTH) * CPU_MHZ);
      calc_rest = calc_rest + ppm[cur_chan_numb];
      cur_chan_numb++;
    }
  }
  timer0_write(next);
}

void ppm_begin(void) {
  pinMode(sigPin, OUTPUT);
  digitalWrite(sigPin, !onState); //set the PPM signal pin to the default state (off)

  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(ppmISR);
  next = ESP.getCycleCount() + 1000;
  timer0_write(next);
  for (int i = 0; i < CHANNEL_NUMBER; i++) {
    ppm[i] = CHANNEL_DEFAULT_VALUE;
  }
  interrupts();
}

//############################################################################//
//                          ESPNOW Declarations                               //
//############################################################################//

#include <ESP8266WiFi.h>
#include <espnow.h>

#define txInterval 50 //ms
#define timeout 200   //ms

#define verbose_enabled false

uint8_t drone_address[] = {0xCC, 0x50, 0xE3, 0x3C, 0xAD, 0x13};
uint8_t drone1_address[] = {0x5C, 0xCF, 0x7F, 0xB7, 0x22, 0x8E};
uint8_t drone2_address[] = {0x84, 0xF3, 0xEB, 0x5A, 0x73, 0xA2};
uint8_t drone3_address[] = {0xBC, 0xDD, 0xC2, 0x2B, 0x0E, 0x98};
uint8_t station_address[] = {0x80, 0x7D, 0x3A, 0xB7, 0x81, 0x54};


// Send data structure
typedef struct drone_message {
  bool conn_stat;
  float battery;
  uint8_t local_time[2];
} drone_message;


// Receive data structure
typedef struct command_message {
  bool conn_stat;
  int thr;
  int rol;
  int pit;
  int yaw;
  int aux[4];
  uint8_t local_time[2];
} command_message;


bool compare_address(uint8_t* add1, uint8_t* add2)
{
  for (int i = 0; i < 6; i++)
    if (add1[i] != add2[i])
      return false;
  return true;
}

void print_address(uint8_t* add) {
  for (int i = 0; i < 6; i++) {
    Serial.print(add[i], HEX);
    Serial.print(" ");
  }
}

class Station_Handler {
  public:
    uint8_t address[6];  // station MAC Address
    drone_message droneData;
    command_message commandData;
    boolean connection_status = false;

    Station_Handler() {
    }

    void begin(uint8_t* stationAddress) {
      memcpy(address, stationAddress, 6);
      esp_now_add_peer(address, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);
      if (verbose_enabled) {
        print_address(stationAddress);
        Serial.println(" ");
      }
    }

    void transmit() {
      esp_now_send(address, (uint8_t *) &droneData, sizeof(droneData));
    }

    void receive(const unsigned char* incomingData) {
      memcpy(&commandData, incomingData, sizeof(commandData));

    }

    void print_droneData() {
      Serial.print("Drone connection: "); Serial.print(droneData.conn_stat);
      Serial.print(" bat "); Serial.print(droneData.battery);
      Serial.print(" min "); Serial.print(droneData.local_time[0]);
      Serial.print(" sec "); Serial.println(droneData.local_time[1]);
    }

    void print_commandData() {
      Serial.print("Station connection: "); Serial.print(commandData.conn_stat);
      Serial.print(" THR "); Serial.print(commandData.thr);
      Serial.print(" ROL "); Serial.print(commandData.rol);
      Serial.print(" PIT "); Serial.print(commandData.pit);
      Serial.print(" YAW "); Serial.print(commandData.yaw);
      Serial.print(" AU0 "); Serial.print(commandData.aux[0]);
      Serial.print(" AU1 "); Serial.print(commandData.aux[1]);
      Serial.print(" min "); Serial.print(commandData.local_time[0]);
      Serial.print(" sec "); Serial.println(commandData.local_time[1]);
    }
};


Station_Handler st;


unsigned long beginTime;
unsigned long currentTime;
unsigned long lastTime = 0; //transmission timer
unsigned long lastReceived; //successful receive timer
unsigned long lastSent;     //successful transmit timer

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {


  if (sendStatus != 0) {
    if (verbose_enabled) Serial.println("Delivery fail");
  }
  else {
    lastSent = millis();
    if (verbose_enabled) {
      Serial.print("Data successfully sent to :");
      print_address((uint8_t*)mac_addr);
      Serial.println("\n");
    }
  }

}

// Callback function that will be executed when data is received
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {

  bool recognized = false;

  if (verbose_enabled) {
    Serial.print("Msg Source: ");
    print_address(mac);
  }

  if (compare_address(st.address, mac)) {
    lastReceived = millis();
    st.receive(incomingData);

    if (verbose_enabled) Serial.println("Recognized as registered station");

    recognized = true;
  }

  if (!recognized && verbose_enabled) Serial.println("Unrecognized source\n");
}

void espnow8266_begin() {

  WiFi.mode(WIFI_STA);  // Set device as a Wi-Fi Station

  if (esp_now_init() != 0) {
    if (verbose_enabled) Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);  // sendCB to get the status of Trasnmitted packet
  esp_now_register_recv_cb(OnDataRecv);  // recvCB to get recv packer info

  st.begin(station_address);
}

//############################################################################//
//                            Helper Functions                                //
//############################################################################//


void signal_reset() {
  ppm[THR] = 1000;
  ppm[ROL] = 1500;
  ppm[PIT] = 1500;
  ppm[YAW] = 1500;

  ppm[AUX1] = 1000;
  ppm[AUX2] = 1000;
  ppm[AUX3] = 1000;
  ppm[AUX4] = 1000;
}

void signal_update() {
  ppm[THR] = constrain(st.commandData.thr, 1000, 2000);
  ppm[ROL] = constrain(st.commandData.rol, 1000, 2000);
  ppm[PIT] = constrain(st.commandData.pit, 1000, 2000);
  ppm[YAW] = constrain(st.commandData.yaw, 1000, 2000);

  ppm[AUX1] = st.commandData.aux[0] ? 1700 : 1100 ;
  ppm[AUX2] = st.commandData.aux[1] ? 1700 : 1100 ;
  ppm[AUX3] = st.commandData.aux[2] ? 1700 : 1100 ;
  ppm[AUX4] = st.commandData.aux[3] ? 1700 : 1100 ;
}

void drone_routine(bool feedback_enabled) {
  st.droneData.local_time[0] = floor(((currentTime - beginTime) / 1000) / 60.0);
  st.droneData.local_time[1] = ((currentTime - beginTime) / 1000) % 60;

  if (feedback_enabled) {
    Serial.println("\nDrone");
    st.print_commandData();
    st.print_droneData();
  }

  st.transmit();
}

//############################################################################//
//                            Setup and Loop                                  //
//############################################################################//

void setup() {

  beginTime = millis();
  Serial.begin(115200);
  Serial.print("\n\n\n\n\n");

  espnow8266_begin();
  ppm_begin();
}


void loop() {

  currentTime = millis();

  if ((currentTime - lastReceived) > timeout || (currentTime - lastSent) > timeout) {
    st.droneData.conn_stat = false;
    signal_reset();
  }
  else {
    st.droneData.conn_stat = true;
    signal_update();
  }

  if ((currentTime - lastTime) > txInterval) {
    drone_routine(false);
    lastTime = currentTime;
  }
}
