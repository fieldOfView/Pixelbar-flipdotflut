/**
 * SSD1306 Flipdot display simulator
 * (c) 2023 Aldo Hoeben / fieldOfView
 * based on:
 *
 * Flipdot display driver firmware
 * (c) 2023 Koen van Vliet <8by8mail@gmail.com>
 *
 * Serial command format: Two consecutive bytes containing x,y coordinates and dot polarity (on/off.)
 * CMDH = 1CCC CCCC
 * CMDL = 0xxP RRRR
 *
 * C = column address
 * R = row address
 * P = dot polarity (1= on/ 0=off)
 * x = reserved for future use, set to 0 for now
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define WIRE Wire
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &WIRE);

#define BAUD_RATE 74880UL ///< Serial command interface data rate.
//#define SHOW_TESTPATTERN ///< Flash the display for a second of times before starting.

// Display parameters
#define DISPLAY_NOF_ROWS 16 ///< Number of rows of the display
#define DISPLAY_NOF_COLUMNS 28 * 4 ///< Total number of columns of the display

#define MAX_TIME_BETWEEN_UPDATES 100
#define MAX_DOTS_BETWEEN_UPDATES 800

static void flip(uint8_t x, uint8_t y, uint8_t d);
static inline void selectRow(uint8_t row);
static inline void selectColumn(uint8_t column);

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(1000);

  // Clear the buffer.
  display.clearDisplay();
  display.display();

  Serial.begin(BAUD_RATE);
  pinMode(LED_BUILTIN, OUTPUT);
}

// the loop function runs over and over again forever
void loop() {
  uint8_t data, row, col;
  uint8_t cmdl, cmdh;

  unsigned long last_display = millis();
  unsigned long current_time = last_display;
  unsigned long dots = 0;
  bool test = false;

  #ifdef SHOW_TESTPATTERN
  unsigned long test_time = millis();
  while(millis() - test_time < 1000){
   for(data = 0; data <= 1; data++){
     for(row = 0; row < DISPLAY_NOF_ROWS; row++){
       for(col = 0; col < DISPLAY_NOF_COLUMNS; col++){
          flip(col, row, data);

          dots++;
          current_time = millis();
          if(current_time - last_display > MAX_TIME_BETWEEN_UPDATES ||
              dots > MAX_DOTS_BETWEEN_UPDATES) {
            last_display = current_time;
            dots = 0;
            display.display();
          }
        }
      }
    }
  }
  #endif

  cmdl = 0;

  while (1) {
    current_time = millis();
    if(current_time - last_display > MAX_TIME_BETWEEN_UPDATES ||
        dots > MAX_DOTS_BETWEEN_UPDATES) {
      last_display = current_time;
      dots = 0;
      display.display();
    }
    if (Serial.available()) {
      if (cmdl & (1<<7)) {
        cmdh = cmdl;
        cmdl = Serial.read();

        data = (cmdl >> 4) & 0x01;
        row = cmdl & 0x0F;
        col = cmdh & 0x7F;

        flip(col, row, data);
        dots++;

        cmdl = 0;
      } else {
        cmdl = Serial.read();
      }
    }
  }
}

// Private functions

/**
 * Flip a dot at position
 * @param x[in] Dot x-coordinate / column
 * @param y[in] Dot y-coordinate / row
 * @param p[in] Dot polarity. 1=on 0=off
 *
 * display.display() must be called after a certain number of flips or time
 */
static void flip(uint8_t x, uint8_t y, uint8_t p) {
  if (x < DISPLAY_NOF_COLUMNS && y < DISPLAY_NOF_ROWS) {
    display.drawPixel(x, y, p);
  }
}
