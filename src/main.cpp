#include "main.h"

TinyGPSPlus gps;
Logger logger;
GPSData data;
bool gps_debug = false;

int no_gps_lock_counter = 0;
int no_location_update_counter = 0;
double old_lat,old_lon;

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

    Logger::printfln("Connecting to WiFi: %s ...", found_ssid);
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
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command == "g" || command == "gps") {
            gps_debug = !gps_debug;
            logger.printfln("Toggling GPS debug %s...", gps_debug ? "on" : "off");
        } else if (command == "r" || command == "reset") {
            logger.println("Restarting ESP...");
            ESP.restart();
        } else if (command == "u" || command == "upload") {
            logger.println("Uploading gpsLog...");
            //upload_gps_file();
        }
    }
}

TaskHandle_t redLedTask,greenLedTask,gpsTask;

void manageRedLed(void * pvParameters){
  for(;;){    digitalWrite(RED_LED, HIGH);
    delay(500);
    digitalWrite(RED_LED, LOW);
    delay(500);
  } 
}

void manageGreenLed(void * pvParameters){
  for(;;){
    digitalWrite(GREEN_LED, HIGH);
    delay(1000);
    digitalWrite(GREEN_LED, LOW);
    delay(1000);
  } 
}

void manageGPS(void * pvParameters) {
    unsigned long last = millis();
    unsigned long last1 = last;
    for(;;) {
        unsigned long now = millis();
        if ((now-last) < GPS_TICK_INTERVAL) continue;
        last=now;
        bool isLocationValid = gps.location.isValid();
        bool isLocationUpdated = gps.location.isUpdated();

        if (gps.speed.isValid()) {
            data.speed = gps.speed.kmph();
            data.min_speed=data.speed<data.min_speed?data.speed:data.min_speed;
            data.max_speed=data.speed>data.max_speed?data.speed:data.max_speed;
        }
        if (gps.altitude.isValid()) {
            data.alt = gps.altitude.meters();
            data.min_alt=data.alt<data.min_alt?data.alt:data.min_alt;
            data.max_alt=data.alt>data.max_alt?data.alt:data.max_alt;
        }

        if (isLocationUpdated && isLocationValid && gps.speed.isValid() && gps.altitude.isValid()) ble_update(data);
        
        if ((now-last1) > GPS_LOG_INTERVAL) { //do checks every 1 second
            last1 = now;
            if (!isLocationValid) {
                if (no_gps_lock_counter++ % int(60000 / GPS_LOG_INTERVAL) == 0) {
                    logger.printfln(TIMESTAMP_FORMAT ": No GPS lock: %u satellites.",
                                TIMESTAMP_ARGS,
                                gps.satellites.value());
                }
                continue;
            }
            no_gps_lock_counter = 0;

            if (!isLocationUpdated) {
                if (no_location_update_counter++ % int(60000 / GPS_LOG_INTERVAL) == 0) {
                    logger.printfln(TIMESTAMP_FORMAT ": Last GPS location update was %.0f seconds ago.",
                                TIMESTAMP_ARGS,
                                gps.location.age() / 1000.0);
                }
                continue;
            }
            no_location_update_counter = 0;

            data.lat = gps.location.lat();
            data.lon = gps.location.lng();
            double distance_travelled = gps.distanceBetween(old_lat, old_lon, data.lat, data.lon);
            if (distance_travelled < 5.0) {
                logger.printfln(TIMESTAMP_FORMAT ": Only travelled %.2f meters since last update - skipping.",
                            TIMESTAMP_ARGS,
                            distance_travelled);
                continue;
            } else {
                old_lat = data.lat;
                old_lon = data.lon;
            }

            char record[80];
            snprintf(record, sizeof(record), TIMESTAMP_FORMAT ";%.6f;%.6f;%.2f;%.2f;%d",
                    TIMESTAMP_ARGS,
                    data.lat, data.lon, data.alt, data.speed, data.satellites);
            logger.println(record);
        }
    }
}

void led_setup() {
    pinMode(RED_LED,OUTPUT);
    pinMode(GREEN_LED,OUTPUT);
    xTaskCreate(manageRedLed,"redLed",1024,NULL,10,&redLedTask);
    xTaskCreate(manageGreenLed,"greenLed",1024,NULL,10,&greenLedTask);   
}

void data_update() {
    data.max_speed = 100;
}

void gps_setup() {
    Serial2.begin(9600, SERIAL_8N1,17,16);
    xTaskCreate(manageGPS,"GPS",8192,NULL,1,&gpsTask); 
    logger.print("GPS-");
}

void setup() {
    ota_setup();
    led_setup();
    wifi_connect();
    ble_setup();
    gps_setup();
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
                cc=0;
                logger.print(buf);
            }
        }
        gps.encode(b);
    }
}