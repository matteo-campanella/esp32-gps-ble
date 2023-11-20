#include "main.h"
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>

TinyGPSPlus gps;
Logger logger;
GPSData data;
Leds leds;
bool gps_debug = false;
bool gps_checks = false;

int no_gps_lock_counter = 0;
int no_location_update_counter = 0;
double old_lat,old_lon;
TaskHandle_t gpsTask;
bool isModemSleepOn = false;

bool wifi_connect() {
    const char *found_ssid = NULL;
    int n = 0;

    for (int i = 0; i < 3; i++) {
        n = WiFi.scanNetworks();
        if (n > 0) {
            break;
        }
        delay(250);
    }

    for (int i = 0; i < n; ++i) {
        int j = 0;
        while (WIFI_CREDENTIALS[j][0] != NULL) {
            if (WiFi.SSID(i) == WIFI_CREDENTIALS[j][0]) {
                found_ssid = WIFI_CREDENTIALS[j][0];
                const char *passphrase = WIFI_CREDENTIALS[j][1];
                WiFi.begin(found_ssid, passphrase);
                break;
            }
            j++;
        }
    }

    if (found_ssid == NULL) {
        logger.println("No known WiFi found.");
        WiFi.mode(WIFI_OFF);
        return false;
    }

    logger.printfln("Connecting to WiFi: %s ...", found_ssid);
    WiFi.mode(WIFI_STA);
    int tries = 50;
    while (WiFi.status() != WL_CONNECTED && tries > 0) {
        delay(250);
        tries--;
    }
    if (tries == 0) {
        logger.println("Failed to connect to WiFi!");
        WiFi.mode(WIFI_OFF);
        return false;
    }
    
    logger.print("IP: ");
    logger.println(WiFi.localIP().toString().c_str());

    logger.print("NET-");
    return true;
}


void check_incoming_commands() {
    String command;
    command = logger.udpReceive();
    if (command.length()>0) {
        command.trim();
    }
    if (Serial.available()>0) {
        command = Serial.readStringUntil('\n');
        command.trim();
    }
    if (command == "g" || command == "gps") {
        gps_debug = !gps_debug;
        logger.printfln("Toggling GPS debug %s...", gps_debug ? "on" : "off");
    } else if (command == "r" || command == "reset") {
        logger.println("Restarting ESP...");
        delay(1000);
        ESP.restart();
    } else if (command == "u" || command == "upload") {
        logger.println("Uploading gpsLog...");
        //upload_gps_file();
    }
}

void manageGPS(void * pvParameters) {
    unsigned long last = millis();
    unsigned long last1 = last;
    unsigned int sleep_speed_counter = 0;
    unsigned int wake_speed_counter = 0;
    for(;;) {
        unsigned long now = millis();
        if ((now-last) < GPS_TICK_INTERVAL) continue;
        last=now;

        if (gps.speed.isValid() && gps.speed.isUpdated()) {
            data.speed=gps.speed.kmph();
            data.min_speed=data.speed<data.min_speed?data.speed:data.min_speed;
            data.max_speed=data.speed>data.max_speed?data.speed:data.max_speed;
            if (data.speed > SLEEP_TRIGGER_SPEED ) {
                sleep_speed_counter++;
                wake_speed_counter=0;
                logger.println("S");
            }
            else {
                wake_speed_counter++;
                sleep_speed_counter=0;
                logger.println("W");
            }
        }
        if (gps.altitude.isValid() && gps.altitude.isUpdated()) {
            data.alt=gps.altitude.meters();
            data.min_alt=data.alt<data.min_alt?data.alt:data.min_alt;
            data.max_alt=data.alt>data.max_alt?data.alt:data.max_alt;
        }

        if ((now-last1) > GPS_LOG_INTERVAL) { //do checks every 1 second
            last1 = now;

            if (gps_checks) {
                if (!gps.location.isValid()) {
                    if (no_gps_lock_counter++ % int(60000 / GPS_LOG_INTERVAL) == 0) {
                        logger.printfln(TIMESTAMP_FORMAT ": No GPS lock: %u satellites.",
                                    TIMESTAMP_ARGS,
                                    gps.satellites.value());
                    }
                    continue;
                }
                no_gps_lock_counter = 0;

                if (!gps.location.isUpdated()) {
                    if (no_location_update_counter++ % int(60000 / GPS_LOG_INTERVAL) == 0) {
                        logger.printfln(TIMESTAMP_FORMAT ": Last GPS location update was %.0f seconds ago.",
                                    TIMESTAMP_ARGS,
                                    gps.location.age() / 1000.0);
                    }
                    continue;
                }
                no_location_update_counter = 0;

                double new_lat = gps.location.lat();
                double new_lon = gps.location.lng();
                double distance_travelled = gps.distanceBetween(old_lat, old_lon, new_lat, new_lon);
                if (distance_travelled < 5.0) {
                    logger.printfln(TIMESTAMP_FORMAT ": Only travelled %.2f meters since last update - skipping.",
                                TIMESTAMP_ARGS,
                                distance_travelled);
                    continue;
                } else {
                    old_lat = new_lat;
                    old_lon = new_lon;
                }
            }

            if (gps.location.isValid() && gps.location.isUpdated()) {
                char record[80];
                snprintf(record, sizeof(record), TIMESTAMP_FORMAT ";%.6f;%.6f;%.2f;%.2f;%d",
                        TIMESTAMP_ARGS,
                        gps.location.lat(), gps.location.lng(), gps.altitude.meters(), gps.speed.kmph(), gps.satellites.value());
                logger.println(record);
            }

            ble_update(&data,&gps);

            if (sleep_speed_counter > SLEEP_TRIGGER_COUNT && !isModemSleepOn) {
                sleep_speed_counter = 0;
                wake_speed_counter = 0;

                logger.println("Entering Modem Sleep Mode");
                //delay(1000);
                // ble_stop();
                // WiFi.disconnect();
                // WiFi.mode(WIFI_OFF);
                // btStop();
                // esp_bt_controller_disable();
                isModemSleepOn = true;
            }
            if (wake_speed_counter > WAKE_TRIGGER_COUNT && isModemSleepOn) {
                sleep_speed_counter = 0;
                wake_speed_counter = 0;  
                logger.println("Exiting Modem Sleep Mode");
            }
        }
    }
}

void gps_setup() {
    Serial2.begin(9600, SERIAL_8N1,17,16);
    xTaskCreate(manageGPS,"GPS",8192,NULL,1,&gpsTask); 
    logger.print("GPS-");
}

void setup() {
    ota_setup();
    leds.setup();
    wifi_connect();
    ble_setup();
    gps_setup();
    //logger.udpListen();
    logger.println("UP");
}


char buf[256];
uint8_t cc = 0;

void loop() {
    if (Serial2.available() > 0) {
        int b = Serial2.read();
        if (gps_debug) {
            buf[cc++]=b;
            if (b=='\n') {
                buf[cc]=0;
                logger.print(buf);
                cc=0;
            }
        }
        gps.encode(b);
    }
    //check_incoming_commands();
}