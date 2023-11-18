#include "logging.h"
#include "ble.h"
#include <Arduino.h>

WiFiUDP Logger::udp;

void Logger::udpBroadcast(const char *message) {
    if (!WiFi.isConnected()) return;
    if (udp.beginPacket(WiFi.broadcastIP(),UDP_BROADCAST_PORT)) {
        udp.print(message);
        udp.endPacket();
    }
}

void Logger::print(const char *message) {
    Serial.print(message);
    udpBroadcast(message);
    //ble_uart_send(message);
}

void Logger::printf(const char *message, ...) {
    va_list argp;
    va_start(argp, message);

    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), message, argp);
    Logger::print(buffer);
}

void Logger::vprintf(const char *message, va_list argp) {
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), message, argp);
    Logger::print(buffer);
}

void Logger::printfln(const char *message, ...) {
    va_list argp;
    va_start(argp, message);
    Logger::vprintf(message, argp);
    Logger::print("\n");
}

void Logger::println(const char *message) {
    Logger::print(message);
    Logger::print("\n");
}
