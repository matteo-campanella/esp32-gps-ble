#include <TinyGPS++.h>
#include "gpsdata.h"

void ble_setup();
void ble_update(GPSData *,TinyGPSPlus *);
void ble_uart_send(const char *);
void ble_stop();