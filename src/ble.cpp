#include <CircularBuffer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include <TinyGPS++.h>

#include "ble.h"
#include "logging.h"
#include <string>

BLEServer* pServer = NULL;
BLECharacteristic* pLocCharacteristic = NULL;
BLECharacteristic* pSpeedCharacteristic = NULL;
BLECharacteristic* pAltCharacteristic = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
CircularBuffer<byte,512> buffer;

extern Logger logger;

#define UART_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define UART_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define GPS_SERVICE_UUID        "f7cfe716-cf46-440c-b763-2b10f181051e"
#define LOC_CHARACTERISTIC_UUID "0a75ba9e-7cf2-490b-9811-f16af03730db"
#define SPD_CHARACTERISTIC_UUID "f37a98dd-7da6-4578-adae-9dbd481b7034"
#define ALT_CHARACTERISTIC_UUID "0538d525-011e-450b-a9e4-920010a0cf63"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        for (int i = 0; i < rxValue.length(); i++) buffer.push(rxValue[i]);
      }
    }
};

void ble_setup() {
  // Create the BLE Device
  BLEDevice::init("bleGPS");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  //UART Service
  BLEService *pUARTService = pServer->createService(UART_SERVICE_UUID);
  pTxCharacteristic = pUARTService->createCharacteristic(
										UART_CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
  pTxCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic * pRxCharacteristic = pUARTService->createCharacteristic(
											UART_CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);
  pRxCharacteristic->setCallbacks(new MyCallbacks());     
  pUARTService->start();

  //GPS Service
  BLEService *pGpsService = pServer->createService(GPS_SERVICE_UUID);
  pLocCharacteristic = pGpsService->createCharacteristic(
                      LOC_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pLocCharacteristic->addDescriptor(new BLE2902());

  pSpeedCharacteristic = pGpsService->createCharacteristic(
                      SPD_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pSpeedCharacteristic->addDescriptor(new BLE2902());

  pAltCharacteristic = pGpsService->createCharacteristic(
                      SPD_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pAltCharacteristic->addDescriptor(new BLE2902());

  pGpsService->start();
  pServer->getAdvertising()->start();

  logger.print("BLE-");
}

void ble_uart_send(const char *message) {
    if (deviceConnected) {
      pTxCharacteristic->setValue((uint8_t*)message,strlen(message));
      pTxCharacteristic->notify();
		  delay(10); // bluetooth stack will go into congestion, if too many packets are sent
	  }
}

unsigned int ble_uart_receive(char *b) {
  return 0;
}

void ble_update(GPSData *data, TinyGPSPlus *gps) {
    if (deviceConnected) {
      if (gps->location.isValid() && gps->location.isUpdated()) {
        pLocCharacteristic->setValue(std::to_string(gps->location.lat())+","+std::to_string(gps->location.lng()));
        pLocCharacteristic->notify();
      }
      if (gps->speed.isValid() && gps->speed.isUpdated()) {
        pLocCharacteristic->setValue(std::to_string(data->min_speed)+","+std::to_string(data->speed)+","+std::to_string(data->max_speed));
        pLocCharacteristic->notify();        
      }
      if (gps->altitude.isValid() && gps->altitude.isUpdated()) {
        pLocCharacteristic->setValue(std::to_string(data->min_alt)+","+std::to_string(data->alt)+","+std::to_string(data->max_alt));
        pLocCharacteristic->notify(); 
      }
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}

void ble_stop() {
  BLEDevice::deinit();
}