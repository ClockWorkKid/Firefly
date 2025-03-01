
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
 * 
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
 */

#define REMOTE_XY_OWN

 
#include "remotexy.h"
#include <espcopter.h>

// ************************************************************************************************************************************

void setup() {
  mainSetup();
  esp.greenLed_Digital(0);
}

void loop() {
    mainLoop();  // main flying loop
   if(RemoteXY.button_1 == 1){ // if switch_1 is on do here
    esp.redLed_Digital(1);
   }else{ 
    esp.redLed_Digital(0);
   }
   if(RemoteXY.button_2 == 1){ // if switch_1 is on do here
    esp.greenLed_Digital(1);
   }else{ 
    esp.greenLed_Digital(0);
   }
   if(RemoteXY.button_3 == 1){ // if switch_1 is on do here
    esp.blueLed_Digital(1);
   }else{ 
    esp.blueLed_Digital(0);
   }
} 
