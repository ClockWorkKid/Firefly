
/*
 * redLed_Digital(); // 0 - 1 yada false - true 
 * blueLed_Digital(); // 0 - 1 yada false - true 
 * greenLed_Digital(); // 0 - 1 yada false - true 
 * 
 * redLed_Analog(); // 0 - 1023
 * blueLed_Analog(); // 0 - 1023
 * greenLed_Analog(); // 0 - 1023
 * 
 * motorFL_Analog(); // 0 - 200 
 * motorFR_Analog(); // 0 - 200 
 * motorRL_Analog(); // 0 - 200 
 * motorRR_Analog(); // 0 - 200 
 * 
 * takeOff(altitude,total time);
 * land();
 * goforward(time);
 * goBack(time);
 * goLeft(time);
 * goRight(time);
 * setAltitude(Altitude);
 * targetOto = 0 - 1000;
 * 
 *  Trim_Roll = 0 // -1750 : 1750
 *  Trim_Pitch = 0
 *  Trim_Yaw =0
 *  
 * armControl = 1 or 0;
 * 
 * 
 * flyMode_1 = 1 or 0;
 * flyMode_2 = 1 or 0;
 * flyMode_3 = 1 or 0;
 * 
 * DistanceX; // multi-ranger
 * DistanceY;
 * 
 *  SetPointOpt[0] -15 && 15 
 *   SetPointOpt[1] -15 && 15 
 */

  
#define REMOTE_XY_REMOTE

#include "remotexy.h"
#include <espcopter.h>

// ************************************************************************************************************************************



void setup() {
  mainSetup();
  Trim_Roll = 0; // -1750, 1750
  Trim_Pitch = 0; // -1750, 1750
  Trim_Yaw = 100;  // -1750, 1750
}

void loop() {
   mainLoop();  // main flying loop
   if(RemoteXY.switch_1 == 1){ // if switch_1 is on do here
   armControl =1;     
   takeOff(500, 16000); //Take off 500 altitude for 25 seconds 
   delay_(7500);
   goForward(1000);
   delay_(7500);
   land();//land 
   }else{ // if switch_1 is off do here
   armControl = 0; // close the motors
}
} 
