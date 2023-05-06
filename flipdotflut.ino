#include "WiFi.h"
#include "AsyncUDP.h"
#include <mutex>

#define COLUMNS 112
#define ROWS 16

const char* ssid = "WIFI_NAME";
const char* pass = "WIFI_PASSWORD";
const unsigned int port = 1234;

AsyncUDP udp;

std::mutex lock[COLUMNS][ROWS];
bool flip[COLUMNS][ROWS];
bool current_state[COLUMNS][ROWS];

void setup() {
  // Serial is for debug output over the USB serial connection
  Serial.begin(115200);
  Serial.println("ESP32 flipdotflut");

  // Serial2 is for communicating with the flipdot controller
  Serial2.begin(74880);

  // setup and connect to WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  // setup UDP listener
  if (udp.listen(port)) {
    Serial.print("UDP Listening on: ");
    Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.println(port);
    udp.onPacket(onUDPMessage);
  }
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
  bool state;

  if(packet.lenght() != 2) {
    Serial.println("Received package with invalid length");
    return
  }

  cmdh = packet.data()[0];
  cmdl = packet.data()[1];

  if(cmdh & 0x80 == 0 || cmdl & 0x80 != 0) {
    Serial.println("Received package with invalid structure");
    return
  }

  // parse column, row, state from message
  column = (cmdh & 0x7F);
  row = (cmdl & 0x0F);
  state = (cmdl & 0x10) != 0;

  if(column >= COLUMNS || row >= ROWS) {
    Serial.println("Received package exceeding allowed positions");
    return;
  }

  // acquire a lock on this pixel so it is not accessed by display loop
  // until we are done changing it
  std::lock_guard<std::mutex> lck(lock[column][row]);

  flip[column][row] = (new_state != current_state[column][row]);
}

void loop() {
  for (unsigned int row = 0; row < ROWS; row++)
  {
    for (unsigned int column = 0; column < COLUMNS; column++)
    {
      // acquire a lock on this pixel so it is not accessed by UDP messages
      // until we are done looking at it
      std::unique_lock<std::mutex> lck(lock[column][row]);

      if (flip[column][row])
      {
        bool new_state = !current_state[column][row];
        current_state[column][row] = new_state;

        // release the lock because we don't need the UDP messages to wait
        // until we are done sending the flipped dot
        lck.unlock();

        drawDot(column, row, new_state);
      }
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
  cmdh = (1 << 7) | (col & 0x7F);

  Serial2.write(cmdh);
  Serial2.write(cmdl);
  Serial2.flush();
}