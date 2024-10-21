#include <Arduino.h>
#include <Wire.h>

#define SLC (22)

struct LEDFadeInstruction
{
  int r;
  int g;
  int b;
  float a;
  float duration;
};

void ledWrite(uint8_t reg_addr, uint8_t data);
void setColor(float r, float g, float b, float a);
void fadeTo(int r, int g, int b, float a, float duration);
void asyncFadeTo(void *param);
void pulse(int r, int g, int b);
void defaultLED();
void successLED();
void errorLED();
void writeLED();
void offLED();
void adjustLEDFromString(const char *str);
void setupLED();
;
