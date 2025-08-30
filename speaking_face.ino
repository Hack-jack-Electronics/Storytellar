#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  

// ESP32 → MAX7219 pins
#define DIN  23
#define CLK  18
#define CS   5

#define MAX_DEVICES 1
MD_MAX72XX mx(HARDWARE_TYPE, DIN, CLK, CS, MAX_DEVICES);

// Frame 1: face with closed mouth
byte face_closed[8] = {
  B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10100101,
  B10011001,
  B01000010,
  B00111100
};

// Frame 2: face with open mouth
byte face_open[8] = {
  B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10111101,
  B10000001,
  B01000010,
  B00111100
};

void drawFace(byte face[8]) {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      bool pixel = bitRead(face[row], 7 - col);
      mx.setPoint(row, col, pixel);
    }
  }
}

void setup() {
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 5);
  mx.clear();
}

void loop() {
  drawFace(face_closed);   // closed mouth
  delay(400);
  drawFace(face_open);     // open mouth
  delay(400);
}
