#include <spi.h>
#include <EPD_Init.h>
#include <EPD.h>
#include "BLEDevice.h"               // Include the library for BLE device-related functions
#include "BLEServer.h"               // Include the library for BLE server-related functions
#include "BLEUtils.h"                // Include the library for BLE utility functions
#include "BLE2902.h"                 // Include the library for the BLE2902 descriptor, used for characteristic descriptors
// #include "BluetoothA2DPSink.h"

#define POWER_PIN 7
#define MS_PER_SEC 1000
#define MS_PER_MIN 60000

#define HOME_KEY 2    // Pin for home key 
#define EXIT_KEY 1    // Pin for exit key
#define PRV_KEY 6     // Pin for previous page key
#define NEXT_KEY 4    // Pin for next page key
#define OK_KEY 5      // Pin 

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Receive channel 
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // Transmid channel

// pointers
BLECharacteristic *pCharacteristic;
BLEServer *pServer;
BLEService *pService;
bool deviceConnected = false;

class InkOsServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    deviceConnected = true;
    Serial.println("Bluetooth Connected.");
  }
  void onDisconnect(BLEServer *pServer) override {
    deviceConnected = false;
    Serial.println("Bluetooth Disconnected.");
    pServer->startAdvertising(); // Announces its presence over bluetooth
    Serial.println("Advertising started.");
  }
};

void drawStatusBar(uint16_t x, uint16_t y, uint16_t cur, uint16_t tot) {
  if (cur > tot) cur = tot; 
  uint8_t height = 20, width = 85;
  EPD_DrawRectangle(x, y, x + width, y + height, BLACK, 0);
  EPD_DrawRectangle(x, y, x + ((float) cur / tot) * width, y + height, BLACK, 1);
}

// allocate 1 bit monochrome framebuffer in SRAM???????
uint8_t ImageBW[27200]; // uint8_t used instead of int to save memory

void setup()
{
  Serial.begin(115200);

  // display settings
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  delay(100);

  EPD_GPIOInit();

  // full screen refresh
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_PartUpdate();

  // bluetooth
  // BLEDevice::init("inkOS");
  // pServer = BLEDevice::createServer();
  // pServer->setCallbacks(new InkOsServerCallbacks());
  // pService = pServer->createService(SERVICE_UUID);
  // // 
  // pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, 
  //   BLECharacteristic::PROPERTY_NOTIFY); // NOTIFY property allows for continuous data flow
  // pCharacteristic->addDescriptor(new BLE2902());
  // // TODO: check on custom callbacks
  // // BLE

  // pinMode(HOME_KEY, INPUT);
  // pinMode(EXIT_KEY, INPUT);
  // pinMode(PRV_KEY, INPUT);
  // pinMode(NEXT_KEY, INPUT);
  // pinMode(OK_KEY, INPUT);    

  delay(2 * MS_PER_SEC); // waits for serial to boot
  Serial.println("Setup Complete.");
}

void loop()
{
  //EPD_FastMode1Init(); // wake up from deep sleep

  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE); // gives pointer of array
  Paint_Clear(WHITE);

  // Boundaries
  EPD_DrawLine(240, 0, 240, 270, BLACK);
  EPD_DrawLine(540, 0, 540, 270, BLACK);
  // Headers
  EPD_ShowString(35, 10, "Bluetooth Media Player", 16, BLACK);
  EPD_ShowString(360, 10, "Schedule", 16, BLACK);
  EPD_ShowString(610, 10, "System Monitor", 16, BLACK);
  
  // Hardware Status
  char statBuff[32]; // temporary storage of stats
  snprintf(statBuff, sizeof(statBuff), "Free PSRAM: %u KB", ESP.getFreePsramInKb());
  EPD_ShowString(590, 40, statBuff, 16, BLACK);
  drawStatusBar(630, 65, ESP.getFreePsramInKb(), ESP.getPsramSizeInKb());
  snprintf(statBuff, sizeof(statBuff), "Free Heap: %u KB", ESP.getFreeHeap() / 1024);
  EPD_ShowString(590, 120, statBuff, 16, BLACK);
  drawStatusBar(630, 145, ESP.getFreeHeap(), ESP.getHeapSize());
  snprintf(statBuff, sizeof(statBuff), "Internal Temp: %u C", static_cast<uint16_t>(temperatureRead()));
  EPD_ShowString(590, 200, statBuff, 16, BLACK);
  EPD_DrawCircle(730, 202, 2, BLACK, false);
  
  EPD_Display(ImageBW);
  EPD_PartUpdate(); // TODO: try part & fast update
  //Serial.println("Loop Complete.");
  //EPD_DeepSleep();

  if (digitalRead(HOME_KEY) == 0) {
    delay(100); // anti shake delay
    if (digitalRead(HOME_KEY) == 1) {
      Serial.print("key pressed");
    }
  }
  //delay(30 * MS_PER_MIN); // runs every 30 mins
}