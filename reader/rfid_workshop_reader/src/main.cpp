#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>

#define SCK (18)
#define MOSI (23)
#define MISO (19)
#define SS (5)
#define RST (4)
#define SDA (21)
#define SLC (22)
#define AUDIO_BUS (25)

#define NOTE_DURATION 50 // duration of each note in milliseconds
#define NOTE_PAUSE 12    // pause between notes in milliseconds

void playAudioNote(note_t note, uint8_t octave, uint16_t duration, uint16_t pause)
{
  ledcWriteNote(0, note, octave);
  delay(duration);
  ledcWriteTone(0, 0);
  delay(pause);
}

void playAudioSuccess()
{
  // playAudioNote(NOTE_C, 4, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_E, 4, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_G, 4, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_C, 5, NOTE_DURATION, NOTE_PAUSE);
  // delay(250);
  // playAudioNote(NOTE_C, 4, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_E, 4, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_G, 4, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_C, 5, NOTE_DURATION, NOTE_PAUSE);
  playAudioNote(NOTE_A, 4, 100, 12);
  playAudioNote(NOTE_D, 4, 100, 12);
  playAudioNote(NOTE_G, 4, 100, 12);
  playAudioNote(NOTE_D, 5, 100, 12);
  playAudioNote(NOTE_A, 5, 300, 12);
}

void playAudioFailure()
{
  // playAudioNote(NOTE_G, 2, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_D, 4, NOTE_DURATION, 0);
  // playAudioNote(NOTE_D, 4, NOTE_DURATION, 0);
  // playAudioNote(NOTE_G, 2, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_D, 4, NOTE_DURATION, 0);
  // playAudioNote(NOTE_D, 4, NOTE_DURATION, 0);
  // playAudioNote(NOTE_G, 2, NOTE_DURATION, NOTE_PAUSE);
  // playAudioNote(NOTE_D, 4, NOTE_DURATION, 0);
  // playAudioNote(NOTE_D, 4u, NOTE_DURATION, 0);
  for (int i = 0; i < 10; i++)
  {
    playAudioNote(NOTE_G, 2, 10, 3);
    playAudioNote(NOTE_E, 3, 30, 3);
    if (i % 7 == 0)
    {
      playAudioNote(NOTE_E, 2, 50, 3);
    }
    if (i % 3 == 0)
    {
      playAudioNote(NOTE_G, 3, 77, 5);
    }
  }
}

MFRC522 mfrc522(SS, RST);
MFRC522::MIFARE_Key key = {.keyByte = {0x43, 0x43, 0x43, 0x61, 0x6d, 0x70}};
byte knownUid[] = {0x63, 0x61, 0x6d, 0x70};
byte secretFlag[] = {
    0x7B,
    0x66,
    0x6C,
    0x61,
    0x67,
    0x7D,
    0x41,
    0x66,
    0x55,
    0x34,
    0x78,
    0x41,
    0x5A,
    0x4E,
    0x54,
    0x43,
};
int LED_I2C_ADDR = 0x60;
TwoWire ledWire = TwoWire(0);
float currentRGBA[4] = {0, 0, 0, 0};
float conversionFactor = 0.0;
static int fadeTableSteps = 21;
bool breakLoop = false;
struct LEDFadeInstruction
{
  int r;
  int g;
  int b;
  float a;
  float duration;
};
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

void setupAudio()
{
  ledcSetup(0, 2500, 8);
  ledcAttachPin(AUDIO_BUS, 0);
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

void playAudio()
{
  int initialValue = 4000;
  int steps = 30;
  int finalValue = initialValue + steps;

  for (int i = initialValue; i < finalValue; i += 1)
  {
    ledcWriteTone(0, i);
    delay(1);
  }
  ledcWrite(0, 0);
}

void playAudioError()
{
  int initialValue = 3000;
  int steps = -100;
  int finalValue = initialValue + steps;

  for (int i = initialValue; i > finalValue; i -= 1)
  {
    ledcWriteTone(0, i);
    delay(1);
  }
  ledcWrite(0, 0);
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

void setup()
{
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  setupLED();
  setupAudio();
  defaultLED();
}

MFRC522::StatusCode status_code;
byte buffer[34];
byte len = sizeof(buffer);
// bool successful_read = false;
byte bufferATQA[8];
byte bufferATQASize = sizeof(bufferATQA);

void loop()
{
  // defaultLED();
  // successful_read = false;
  byte block = 4;

  // Serial.println("LOOP");
  // status_code = mfrc522.PICC_WakeupA(bufferATQA, &bufferATQASize);
  // if (status_code != MFRC522::STATUS_OK)
  // {
  //   Serial.println("WUPA Status not OK: ");
  //   Serial.println(mfrc522.GetStatusCodeName(status_code));
  // }

  if (!mfrc522.PICC_IsNewCardPresent())
  {
    // Serial.println("No new card present");
    // delay(500);
    // defaultLED();
    goto finalize;
  }
  if (!mfrc522.PICC_ReadCardSerial())
  {
    // Serial.println("no serial read");
    // delay(500);
    // defaultLED();
    goto finalize;
  }
  for (int i = 0; i < len; i++)
  {
    buffer[i] = 0x00;
  }

  Serial.printf("Scanned UID (len %d): ", mfrc522.uid.size);
  for (int i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();
  Serial.printf("Known UID (len %d): ", 4);
  for (int i = 0; i < 4; i++)
  {
    Serial.print(knownUid[i], HEX);
  }
  Serial.println();

  if ((memcmp(mfrc522.uid.uidByte, knownUid, 4)))
  {
    Serial.println(F("Unexpected UID"));
    errorLED();
    playAudioFailure();
    goto finalize;
  }

  Serial.println(F("Will auth:"));
  // defaultLED();

  status_code = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A,
      block,
      &key,
      &(mfrc522.uid));
  if (status_code != MFRC522::STATUS_OK)
  {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status_code));
    errorLED();
    playAudioFailure();
    goto finalize;
  }
  else
  {
    Serial.println(F("PCD_Authenticate() success: "));
    // successLED();
    // defaultLED();
  }

  Serial.println(F("Will read block "));
  status_code = mfrc522.MIFARE_Read(block, buffer, &len);
  if (status_code != MFRC522::STATUS_OK)
  {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status_code));
    errorLED();
    playAudioFailure();
    goto finalize;
  }
  else
  {
    Serial.println(F("MIFARE_Read() success: "));

    Serial.printf("Block %02d: ", block);
    for (int i = 0; i < 16; i++)
    {
      if (buffer[i] < 0x10)
      {
        Serial.print('0');
      }
      Serial.print(buffer[i], HEX);
    }
    Serial.println();

    if (memcmp(buffer, secretFlag, 16) == 0)
    {
      successLED();
      playAudioSuccess();
      // successful_read = true;
      goto finalize;
    }
    else
    {
      errorLED();
      playAudioFailure();
      goto finalize;
    }
  }

finalize:
  mfrc522.PCD_StopCrypto1();
  mfrc522.PICC_HaltA();
  delay(1000);
}
