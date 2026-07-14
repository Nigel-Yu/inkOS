#include <5.79_ble_refresh/spi.h>
#include <5.79_ble_refresh/EPD_Init.h>
#include <5.79_ble_refresh/EPD.h>
#include <WiFi.h>
#include <secrets.h>

#define POWER_PIN 7
#define MS_PER_SEC 1000
#define MS_PER_MIN 60000

#define HOME_KEY 2    // Pin for home key
#define EXIT_KEY 1    // Pin for exit key
#define PRV_KEY 6     // Pin for previous page key
#define NEXT_KEY 4    // Pin for next page key
#define OK_KEY 5      // Pin for confirm key

uint8_t ImageBW[27200]; // allocate 1 bit monochrome framebuffer in SRAM???????

String ssid = WIFI_SSID;
String pass = WIFI_PASSWORD;

void setup()
{
  Serial.begin(115200);
  
  // WiFi.mode()
  WiFi.begin(ssid, pass);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected");
  } else {
        Serial.print("Connection Failed. Status Code: ");
        switch(WiFi.status()) {
            case WL_NO_SSID_AVAIL:
                Serial.println("WL_NO_SSID_AVAIL (Network not found. Check SSID name)");
                break;
            case WL_CONNECT_FAILED:
                Serial.println("WL_CONNECT_FAILED (Password incorrect)");
                break;
            case WL_CONNECTION_LOST:
                Serial.println("WL_CONNECTION_LOST (Signal too weak / dropped)");
                break;
            case WL_DISCONNECTED:
                Serial.println("WL_DISCONNECTED (Module failed to acquire IP / rejected)");
                break;
            default:
                Serial.println(WiFi.status());
                break;
        }
    }

  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.println("Connecting WiFi...");
  // } 
  Serial.println("WiFi Connected!");
  Serial.println("IP address: " + WiFi.localIP().toString());


  // display settings
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  delay(100);

  EPD_GPIOInit();

  // full screen refresh
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_FastUpdate();

  delay(4 * MS_PER_SEC); // waits for serial to boot
  Serial.println("Setup Complete.");
}

void loop()
{
  EPD_FastMode1Init(); // wake up from deep sleep

  Paint_NewImage(ImageBW, EPD_W, EPD_H, 0, WHITE); // gives pointer of array
  Paint_Clear(WHITE);

  // draw
  EPD_DrawLine(240, 0, 240, 270, BLACK);
  EPD_DrawLine(540, 0, 540, 270, BLACK);
  EPD_Display(ImageBW);

  EPD_FastUpdate(); // TODO: try part & fast update
  // EPD_Clear_R26A6H(); // clear cache
  Serial.println("Loop Complete.");
  EPD_DeepSleep();

  delay(10 * MS_PER_MIN); // runs every 10 mins
}