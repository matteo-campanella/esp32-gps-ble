#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include "gpsdata.h"

void ble_setup();
void ble_update(GPSData);
void ble_uart_send(const char *);