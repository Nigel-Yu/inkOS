#include "EPD.h"
#include "EPaperDrive.h"

#define POWER_PIN 7
#define MS_PER_SEC 60000

void setup() {
  Serial.begin(115200);

  // Display settings
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  EPD_GPIOInit();
}

void loop() {
  //PAINT_Clear(WHITE)
  EPD_FastMode1Init(); // initialize screen
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H(); // clear cache

  // draw
  EPD_DrawLine(240, 0, 240, 270, BLACK);
  EPD_DrawLine(540, 0, 540, 0, BLACK);

  EPD_PartUpdate(); // partial update
  EPD_DeepSleep();

  delay(10 * MS_PER_SEC) // runs every 10 mins
}