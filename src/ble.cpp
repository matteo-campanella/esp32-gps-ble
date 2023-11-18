#include "ble.h"
#include "logging.h"
#include <CircularBuffer.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
CircularBuffer<byte,512> buffer;

extern Logger logger;

#define UART_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define UART_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define SERVICE_UUID        "f7cfe716-cf46-440c-b763-2b10f181051e"
#define CHARACTERISTIC_UUID "0a75ba9e-7cf2-490b-9811-f16af03730db"

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

  //GPS Speed and Alt Service
  BLEService *pGpsService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pGpsService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pGpsService->start();

  // BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  // pAdvertising->addServiceUUID(UART_SERVICE_UUID);
  // pAdvertising->setScanResponse(false);
  // pAdvertising->setMinPreferred(0x0);  //set value to 0x00 to not advertise this parameter
  // BLEDevice::startAdvertising();

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

void ble_update(GPSData data) {
    if (deviceConnected) {
        pCharacteristic->setValue(data.max_speed);
        pCharacteristic->notify();
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