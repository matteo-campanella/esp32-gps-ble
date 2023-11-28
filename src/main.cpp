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
TaskHandle_t gpsTask,feedGpsTask;
bool isModemSleepOn = false;
bool isGPSLocked = false;
String command;

void wifi_off() {
    WiFi.disconnect(true,false);
    WiFi.mode(WIFI_OFF);
    leds.wifiStatus=Leds::WIFISTATUS::wifi_off;
    logger.print("NET-");
}

void wifi_setup() {
    wifi_off();
}

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
        wifi_off();
        return false;
    }

    logger.printfln("Connecting to WiFi: %s ...", found_ssid);
    WiFi.mode(WIFI_STA);
    leds.wifiStatus=Leds::WIFISTATUS::wifi_on;
    int tries = 50;
    while (WiFi.status() != WL_CONNECTED && tries > 0) {
        delay(250);
        tries--;
    }
    if (tries == 0) {
        logger.println("Failed to connect to WiFi!");
        wifi_off();
        return false;
    }
    
    leds.wifiStatus=Leds::WIFISTATUS::wifi_connected;
    logger.print("NET+");
    return true;
}

void modem_sleep() {
    ble_stop();
    btStop();
    //WiFi.disconnect();
    //WiFi.setSleep(true);
    //WiFi.mode(WIFI_OFF);
    //setCpuFrequencyMhz(40);
    isModemSleepOn = true;
}

void modem_awake() {
    isModemSleepOn = false;
    //setCpuFrequencyMhz(240);
    btStart();
    ble_setup();
}

void check_incoming_commands() {
    command = ble_uart_receive();
    if (Serial.available()>0) command = Serial.readStringUntil('\n');
    // command = logger.udpReceive();
    if (command.length()==0) return;
    command.trim();
    if (command == "g" || command == "gps") {
        gps_debug = !gps_debug;
        logger.printfln("GPS debug %s...", gps_debug ? "ON" : "OFF");
    } 
    else if (command == "r" || command == "reset") {
        logger.println("Restarting ESP...");
        delay(1000);
        ESP.restart();
    } 
    else if (command == "s" || command == "sleep") {
        logger.println("Entering Modem Sleep...");
        modem_sleep();
    }   
    else if (command == "u" || command == "upload") {
        logger.println("Uploading gpsLog...");
        //upload_gps_file();
    }
}

void manageGPS(void * pvParameters) {
    unsigned long last = millis();
    unsigned long last1 = last;
    unsigned int sleep_speed_counter = 0;
    unsigned int wake_speed_counter = 0;

    bool isLocUpdated,isSpeedUpdated,isAltUpdated,isSatUpdated,isHDOPUpdated;

    for(;;) {
        unsigned long now = millis();
        if ((now-last) < GPS_TICK_INTERVAL) continue;
        last=now;

        isLocUpdated = gps.location.isUpdated();
        isSpeedUpdated = gps.speed.isUpdated();
        isAltUpdated = gps.speed.isUpdated();
        isSatUpdated = gps.satellites.isUpdated();
        isHDOPUpdated = gps.hdop.isUpdated();

        if (gps.hdop.isValid() && gps.hdop.hdop() < MAX_HDOP && gps.hdop.hdop() > 0.1) {
            isGPSLocked=true;
            leds.gpsStatus=Leds::GPSSTATUS::locked;
        }
        else {
            isGPSLocked=false;
            leds.gpsStatus=Leds::GPSSTATUS::searching;
        }

        if (gps.speed.isValid() && isSpeedUpdated) {
            data.speed=gps.speed.kmph();
            data.min_speed=data.speed<data.min_speed?data.speed:data.min_speed;
            data.max_speed=data.speed>data.max_speed?data.speed:data.max_speed;
            if (isGPSLocked) {
                if (data.speed > SLEEP_TRIGGER_SPEED) {
                    sleep_speed_counter++;
                    wake_speed_counter=0;
                }
                else {
                    wake_speed_counter++;
                    sleep_speed_counter=0;
                }
            }
        }
        if (gps.altitude.isValid() && isAltUpdated) {
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

                if (!isLocUpdated) {
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

            if (gps.location.isValid() && isLocUpdated) {
                char record[80];
                snprintf(record, sizeof(record), TIMESTAMP_FORMAT ";%.6f;%.6f;%.2f;%.2f;%.2f;%d",
                        TIMESTAMP_ARGS,
                        gps.location.lat(), gps.location.lng(), gps.altitude.meters(), gps.speed.kmph(), gps.hdop.hdop(), gps.satellites.value());
                logger.println(record);
            }
            else logger.print(".");

            if ((isLocUpdated || 
                isSpeedUpdated || 
                isAltUpdated ||
                isSatUpdated ||
                isHDOPUpdated) && !isModemSleepOn
            ) ble_update(&data,&gps);

            if (sleep_speed_counter > SLEEP_TRIGGER_COUNT) {
                sleep_speed_counter = 0;
                wake_speed_counter = 0;
                if(!isModemSleepOn) {
                    logger.println("Entering Modem Sleep Mode");
                    modem_sleep();
                }
            }
            if (wake_speed_counter > WAKE_TRIGGER_COUNT) {
                sleep_speed_counter = 0;
                wake_speed_counter = 0;
                if (isModemSleepOn) {  
                    logger.println("Exiting Modem Sleep Mode");
                    modem_awake();
                }
            }
        }
    }
}

void feedGPS(void * pvParameters) {
    char buf[256];
    uint8_t cc = 0;

    for (;;) {
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
    }
}

void gps_setup() {
    Serial2.begin(9600, SERIAL_8N1,17,16);
    xTaskCreate(manageGPS,"GPS1",8192,NULL,1,&gpsTask);
    xTaskCreate(feedGPS,"GPS2",8192,NULL,1,&feedGpsTask);
    logger.print("GPS+");
}

void bus_setup() {
    pinMode(BUS_RX,INPUT);
    pinMode(BUS_TX,INPUT);
    logger.print("BUS-");
}

void setup() {
    ota_setup();
    leds.setup();
    bus_setup();
    ble_setup();
    wifi_setup();
    gps_setup();
    //logger.udpListen();
    logger.println("UP");
}

void loop() {
    check_incoming_commands();
}