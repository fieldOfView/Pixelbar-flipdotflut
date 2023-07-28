#include <bitset>
#include <array>

#include "WiFi.h"
#include "AsyncUDP.h"
#include <ESPmDNS.h>

#define COLUMNS 112
#define ROWS 16

#define MAX_PACKAGE_LENGTH 1440

const char* ssid = "fieldOfView";
const char* pass = "tarantino";
const unsigned int port = 1337;

AsyncUDP udp;

std::bitset<COLUMNS * ROWS> current_state;            // last polarity sent to controller for each dot
std::bitset<COLUMNS * ROWS> flip;                     // flag to flip polarity for each dot
std::array<SemaphoreHandle_t, COLUMNS * ROWS> lock;   // lock to access flip and current_state on one core at a time

void setup() {
  // Serial is for debug output over the USB serial connection
  Serial.begin(115200);
  Serial.println("ESP32 flipdotflut");

  // Serial2 is for communicating with the flipdot controller
  Serial2.begin(74880);

  unsigned int dot_index;
  for (unsigned int row = 0; row < ROWS; row++)
  {
    for (unsigned int column = 0; column < COLUMNS; column++)
    {
      // force all pixels to be flipped to "off" on the next display loop
      dot_index = column + COLUMNS * row;
      current_state[dot_index] = true;
      flip[dot_index] = true;
      lock[dot_index] = xSemaphoreCreateMutex();
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
  Serial.print("Listening on ");
  Serial.print(WiFi.localIP());
  Serial.print(" UDP port ");
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
  Serial.println("Waiting for flipdotflut messages");
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
  unsigned int dot_index;
  bool new_state;

  int packet_length = packet.length();
  if(packet_length % 2 != 0 || packet_length > MAX_PACKAGE_LENGTH)
  {
    Serial.println("Received package with invalid length");
    return;
  }

  for(int offset=0; offset<packet_length; offset += 2)
  {
    cmdh = packet.data()[offset];
    cmdl = packet.data()[offset + 1];

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

    unsigned int dot_index = column + COLUMNS * row;

    // acquire a lock on this pixel so it is not accessed by display loop
    // until we are done changing it
    if (xSemaphoreTake(lock[dot_index], portMAX_DELAY))
    {
      flip[dot_index] = (new_state != current_state[dot_index]);
      xSemaphoreGive(lock[dot_index]);
    }
  }
}

void loop() {
  unsigned int dot_index;
  for (unsigned int row = 0; row < ROWS; row++)
  {
    for (unsigned int column = 0; column < COLUMNS; column++)
    {
      // acquire a lock on this pixel so it is not accessed by UDP messages
      // until we are done looking at it
      if (!xSemaphoreTake(lock[dot_index], portMAX_DELAY))
      {
        continue;
      }

      if (!flip[dot_index])
      {
        xSemaphoreGive(lock[dot_index]);
        continue;
      }

      bool new_state = !current_state[dot_index];
      current_state[dot_index] = new_state;
      flip[dot_index] = false;

      // release the lock because we don't need the UDP messages to wait
      // until we are done sending the flipped dot
      xSemaphoreGive(lock[dot_index]);

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