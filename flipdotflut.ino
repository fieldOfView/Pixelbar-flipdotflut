#include "WiFi.h"
#include "AsyncUDP.h"
#include <ESPmDNS.h>

#define COLUMNS 112
#define ROWS 16

const char* ssid = "WIFI_NAME";
const char* pass = "WIFI_PASSWORD";
const unsigned int port = 1234;

AsyncUDP udp;

bool current_state[COLUMNS][ROWS];     // last polarity sent to controller for each dot
bool flip[COLUMNS][ROWS];              // flag to flip polarity for each dot
SemaphoreHandle_t lock[COLUMNS][ROWS]; // lock to access flip and current_state on one core at a time

void setup() {
  // Serial is for debug output over the USB serial connection
  Serial.begin(115200);
  Serial.println("ESP32 flipdotflut");

  // Serial2 is for communicating with the flipdot controller
  Serial2.begin(74880);

  for (unsigned int row = 0; row < ROWS; row++)
  {
    for (unsigned int column = 0; column < COLUMNS; column++)
    {
      // force all pixels to be flipped to "off" on the next display loop
      current_state[column][row] = true;
      flip[column][row] = true;
      lock[column][row] = xSemaphoreCreateMutex();
    }
  }

  // setup and connect to WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  // setup UDP listener
  if (!udp.listen(port))
  {
      Serial.println("Error setting up UDP listener!");
      while(1) {
          delay(1000);
      }
  }
  Serial.print("UDP Listening on: ");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(port);
  udp.onPacket(onUDPMessage);

  // setup mDNS responder
  if (!MDNS.begin("flipdotflut")) {
      Serial.println("Error setting up MDNS responder!");
      while(1) {
          delay(1000);
      }
  }
  MDNS.addService("flipdotflut", "udp", port);
  Serial.println("mDNS responder started");
}

void onUDPMessage(AsyncUDPPacket packet) {
  /**
   * The UDP package should closely follow the serial command format:
   * Two consecutive bytes containing x,y coordinates and dot polarity (on/off.)
   * CMDH = 1CCC CCCC CMDL = 0xxP RRRR
   *
   * C = column address
   * R = row address
   * P = dot polarity (1= on/ 0=off)
   * x = reserved for future use, set to 0 for now
   */

  uint8_t cmdl, cmdh;
  unsigned int column, row;
  bool new_state;

  if(packet.length() != 2)
  {
    Serial.println("Received package with invalid length");
    return;
  }

  cmdh = packet.data()[0];
  cmdl = packet.data()[1];


  if((cmdh & 0x80) == 0 || (cmdl & 0x80) != 0)
  {
    Serial.println("Received package with invalid structure");
    return;
  }

  // parse column, row, state from message
  column = (cmdh & 0x7F);
  row = (cmdl & 0x0F);
  new_state = (cmdl & 0x10) != 0;

  if(column >= COLUMNS || row >= ROWS)
  {
    Serial.println("Received package exceeding allowed positions");
    return;
  }

  // acquire a lock on this pixel so it is not accessed by display loop
  // until we are done changing it
  if (xSemaphoreTake(lock[column][row], portMAX_DELAY))
  {
    flip[column][row] = (new_state != current_state[column][row]);
    xSemaphoreGive(lock[column][row]);
  }
}

void loop() {
  for (unsigned int row = 0; row < ROWS; row++)
  {
    for (unsigned int column = 0; column < COLUMNS; column++)
    {
      // acquire a lock on this pixel so it is not accessed by UDP messages
      // until we are done looking at it
      if (!xSemaphoreTake(lock[column][row], portMAX_DELAY))
      {
        continue;
      }

      if (!flip[column][row])
      {
        xSemaphoreGive(lock[column][row]);
        continue;
      }

      bool new_state = !current_state[column][row];
      current_state[column][row] = new_state;
      flip[column][row] = false;

      // release the lock because we don't need the UDP messages to wait
      // until we are done sending the flipped dot
      xSemaphoreGive(lock[column][row]);

      drawDot(column, row, new_state);
    }
  }
}

void drawDot(unsigned int column, unsigned int row, bool pol) {
  /**
   * Serial command format:
   * Two consecutive bytes containing x,y coordinates and dot polarity (on/off.)
   * CMDH = 1CCC CCCC CMDL = 0xxP RRRR
   *
   * C = column address
   * R = row address
   * P = dot polarity (1= on/ 0=off)
   * x = reserved for future use, set to 0 for now
   */

  uint8_t cmdl, cmdh;

  cmdl = (pol ? (1 << 4) : 0) | (row & 0x0F);
  cmdh = (1 << 7) | (column & 0x7F);

  Serial2.write(cmdh);
  Serial2.write(cmdl);
  Serial2.flush();
}