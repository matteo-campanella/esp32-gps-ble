#include "leds.h"

TaskHandle_t Leds::redLedTask,Leds::greenLedTask;

unsigned int Leds::redOn = 100;
unsigned int Leds::redOff = 100;
unsigned int Leds::greenOn = 100;
unsigned int Leds::greenOff = 100;

void Leds::manageRedLed(void * pvParameters){
  for(;;){    
    digitalWrite(RED_LED, HIGH);
    delay(redOn);
    digitalWrite(RED_LED, LOW);
    delay(redOff);
  } 
}

void Leds::manageGreenLed(void * pvParameters){
  for(;;){
    digitalWrite(GREEN_LED, HIGH);
    delay(greenOn);
    digitalWrite(GREEN_LED, LOW);
    delay(greenOff);
  }
}
  
void Leds::setup() {
    pinMode(RED_LED,OUTPUT);
    pinMode(GREEN_LED,OUTPUT);
    xTaskCreate(Leds::manageRedLed,"redLed",1024,NULL,10,&redLedTask);
    xTaskCreate(Leds::manageGreenLed,"greenLed",1024,NULL,10,&greenLedTask);   
}

void Leds::setCommStatus(CommStatus status) {
  switch(status) {
    case CommStatus::searching:
      redOn=redOff=100;
      break;
    case CommStatus::connected:
      redOn=redOff=1000;
      break;
    case CommStatus::disconnected:
      redOn=1;
      redOff=999;
      break;
  }
}

void Leds::setGPSStatus(GpsStatus status) {
  switch(status) {
    case GpsStatus::searching:
      greenOn=greenOff=100;
      break;
    case GpsStatus::locked:
      greenOn=greenOff=1000;
  }
}