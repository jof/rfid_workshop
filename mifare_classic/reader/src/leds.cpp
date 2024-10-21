#include <Arduino.h>
#include <Wire.h>
#include <leds.h>

int LED_I2C_ADDR = 0x60;
TwoWire ledWire = TwoWire(0);
float currentRGBA[4] = {0, 0, 0, 0};
float conversionFactor = 0.0;
static int fadeTableSteps = 21;
bool breakLoop = false;
TaskHandle_t animationTask;
QueueHandle_t animationTaskQueue;

void ledWrite(uint8_t reg_addr, uint8_t data)
{
  uint8_t arr[2];
  arr[0] = reg_addr;
  arr[1] = data;
  ledWire.beginTransmission(LED_I2C_ADDR);
  ledWire.write(arr, 2);
  ledWire.endTransmission();
}

void setColor(float r, float g, float b, float a)
{
  currentRGBA[0] = r;
  currentRGBA[1] = g;
  currentRGBA[2] = b;
  currentRGBA[3] = a;

  ledWrite(0x02, b * a);
  ledWrite(0x03, g * a);
  ledWrite(0x04, r * a);
}

void fadeTo(int r, int g, int b, float a, float duration)
{
  LEDFadeInstruction fadeInstruction = {r, g, b, a, duration};
  xQueueSend(animationTaskQueue, &fadeInstruction, 0);
}

void asyncFadeTo(void *param)
{
  while (true)
  {
    LEDFadeInstruction fadeInstruction;
    bool itemAvailable = xQueueReceive(animationTaskQueue, &fadeInstruction, 0);
    if (!itemAvailable)
    {
      vTaskDelay(400 / portTICK_PERIOD_MS);
      continue;
    }

    int r = fadeInstruction.r;
    int g = fadeInstruction.g;
    int b = fadeInstruction.b;
    float a = fadeInstruction.a;
    float duration = fadeInstruction.duration;

    int currentR = (int)currentRGBA[0];
    int currentG = (int)currentRGBA[1];
    int currentB = (int)currentRGBA[2];
    float currentA = currentRGBA[3];

    float stepsR = (r - currentR) / fadeTableSteps;
    float stepsG = (g - currentG) / fadeTableSteps;
    float stepsB = (b - currentB) / fadeTableSteps;
    float stepsA = (a - currentA) / fadeTableSteps;

    float stepDuration = (duration * 1000) / fadeTableSteps;

    for (int i = fadeTableSteps; i >= 0; i -= 1)
    {
      setColor(r - (stepsR * i), g - (stepsG * i), b - (stepsB * i), a - (stepsA * i));
      vTaskDelay(stepDuration / portTICK_PERIOD_MS);
    }
  }
}

void pulse(int r, int g, int b)
{
  fadeTo(r, g, b, 1.0, 0.3);
  fadeTo(r, g, b, 0.1, 0.2);
  fadeTo(r, g, b, 0.9, 0.3);
  fadeTo(r, g, b, 0.3, 0.1);
}

void defaultLED()
{
  Serial.println("defaultLED()");
  fadeTo(100, 200, 200, 0.5, 0.5);
}

void successLED()
{
  Serial.println("successLED()");
  pulse(0, 200, 50);
}

void errorLED()
{
  Serial.println("errorLED()");
  pulse(200, 20, 0);
}

void writeLED()
{
  fadeTo(200, 0, 140, 0.8, 0.5);
}

void offLED()
{
  setColor(0, 0, 0, 0);
}

void adjustLEDFromString(const char *str)
{
  int r = 0.0;
  int g = 0.0;
  int b = 0.0;
  float a = 1.0;
  sscanf(str, "rgba:%i,%i,%i,%f", &r, &g, &b, &a);

  Serial.println(str);

  fadeTo(r, g, b, a, 0.2);
}

void setupLED()
{
  conversionFactor = 31.0 / 255.0;

  ledWire.begin(SDA, SLC);

  ledWrite(0x00, 0x01);
  ledWrite(0x01, 0x01);
  ledWrite(0x08, 0xAA);

  setColor(0, 0, 0, 0);

  animationTaskQueue = xQueueCreate(10, sizeof(struct LEDFadeInstruction));
  xTaskCreate(asyncFadeTo, "LEDFadeTo", 2048, NULL, 1, NULL);
}
