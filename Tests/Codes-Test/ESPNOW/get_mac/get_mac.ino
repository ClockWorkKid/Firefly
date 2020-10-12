// Complete Instructions to Get and Change ESP MAC Address: https://RandomNerdTutorials.com/get-change-esp32-esp8266-mac-address-arduino/

#include <ESP8266WiFi.h>
//#include <WiFi.h>


void setup(){
  Serial.begin(115200);

}
 
void loop(){
  Serial.print("ESP8266 Board MAC Address:  "); Serial.println(WiFi.macAddress());
}
