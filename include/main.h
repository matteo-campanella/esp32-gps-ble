#include <WifiUdp.h>
#include <TinyGPS++.h>

#include "logging.h"
#include "ble.h"
#include "gpsdata.h"
#include "wifi_credentials.h"
#include "ota.h"


#define RED_LED 23
#define GREEN_LED 22
#define GPS_TICK_INTERVAL 500
#define GPS_LOG_INTERVAL 1000
#define TIMESTAMP_FORMAT "%04d-%02d-%02dT%02d:%02d:%02dZ"
#define TIMESTAMP_ARGS gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second()
