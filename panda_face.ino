#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  
#define DIN  23
#define CLK  18
#define CS   5
#define MAX_DEVICES 1

MD_MAX72XX mx(HARDWARE_TYPE, DIN, CLK, CS, MAX_DEVICES);

// Eyes open + mouth
byte face_open[8] = {
  B00000000,
  B01000010,  // curved eyes top
  B10100101,  // curved eyes bottom
  B00011000,  // small nose
  B00011000,  // mouth start
  B00100100,  // mouth curve
  B00000000,
  B00000000
};

// Eyes closed (blink) + mouth
byte face_blink[8] = {
  B00000000,
  B01111110,  // flat eyes (closed)
  B00000000,
  B00011000,  // nose
  B00011000,  // mouth start
  B00100100,  // mouth curve
  B00000000,
  B00000000
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
  randomSeed(analogRead(0));
}

void loop() {
  drawFace(face_open);
  delay(1500 + random(500));  // eyes open for a while

  drawFace(face_blink);
  delay(200);                 // blink fast

  drawFace(face_open);
  delay(1000 + random(1000)); // open again
}
