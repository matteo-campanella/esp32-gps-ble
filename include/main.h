#include <WifiUdp.h>
#include <TinyGPS++.h>

#include "logging.h"
#include "ble.h"
#include "gpsdata.h"
#include "wifi_credentials.h"
#include "ota.h"
#include "leds.h"

#define GPS_TICK_INTERVAL 500
#define GPS_LOG_INTERVAL 1000
#define TIMESTAMP_FORMAT "%04d-%02d-%02dT%02d:%02d:%02dZ"
#define TIMESTAMP_ARGS gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second()
#define SLEEP_TRIGGER_SPEED 6
#define SLEEP_TRIGGER_COUNT 10
#define WAKE_TRIGGER_COUNT 10