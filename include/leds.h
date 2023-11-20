#include <Arduino.h>

#define RED_LED 23
#define GREEN_LED 22

class Leds{
    private:
        static TaskHandle_t greenLedTask,redLedTask;
        static unsigned int redOn,redOff,greenOff,greenOn;
    public:
        enum class CommStatus {
            searching,
            connected,
            disconnected
        };
        enum class GpsStatus {
            searching,
            locked
        };
        static void setCommStatus(CommStatus status);
        static void setGPSStatus(GpsStatus status);
        static void setup();
        static void manageRedLed(void *);
        static void manageGreenLed(void *);
};