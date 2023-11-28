#include <TinyGPS++.h>
#include <string>
#include "gpsdata.h"

void ble_setup();
void ble_update(GPSData *,TinyGPSPlus *);
void ble_uart_send(const char *);
String ble_uart_receive();
void ble_stop();