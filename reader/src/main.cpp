#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SHA256.h>
#include <secrets.h>

#include <leds.h>
#include <buzzer.h>

#define SCK (18)
#define MOSI (23)
#define MISO (19)
#define SS (5)
#define RST (4)
#define SDA (21)

#define DEBUG_PRINTING

MFRC522 mfrc522(SS, RST);
SHA256 sha;
MFRC522::StatusCode status_code;
byte dataBuffer[64][16];
MFRC522::MIFARE_Key sectorKeyA;
byte buffer[34];
byte bufferLen = sizeof(buffer);
byte bufferATQA[8];
byte bufferATQASize = sizeof(bufferATQA);
bool sectorOneValid;
bool allSectorsValid;
bool hardMode;
uint8_t sector;
char keyType;


void setup()
{
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  setupLED();
  setupAudio();
  defaultLED();
}

void diversifyKey(MFRC522::Uid uid, uint8_t *sector, char *keyType, byte *secret, MFRC522::MIFARE_Key *key)
{
  sha.reset();

#ifdef DEBUG_PRINTING
  Serial.println("diversifyKey(): ");
#endif

  sha.update(uid.uidByte, 4);
#ifdef DEBUG_PRINTING
  Serial.print("  UID: ");
  for (int i = 0; i < 4; i++) {
    Serial.print(uid.uidByte[i], HEX);
  }
  Serial.println();
#endif

  sha.update(sector, 1);
#ifdef DEBUG_PRINTING
  Serial.print("  Sector: ");
  Serial.print(*sector, HEX);
  Serial.println();
#endif

  sha.update(keyType, 1);
#ifdef DEBUG_PRINTING
  Serial.print("  Key Type: ");
  Serial.println(*keyType);
#endif

  sha.finalize(key->keyByte, 6);
#ifdef DEBUG_PRINTING
  Serial.print("  Post-SHA Key Bytes: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(key->keyByte[i], HEX);
  }
  Serial.println();
#endif

#ifdef DEBUG_PRINTING
  Serial.print("  XOR Secret: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(secret[i], HEX);
  }
  Serial.println();
#endif

  for (int i = 0; i < 6; i++)
  {
    key->keyByte[i] = key->keyByte[i] ^ secret[i];
  }

#ifdef DEBUG_PRINTING
  Serial.print("  Post-XOR Key: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(key->keyByte[i], HEX);
  }
  Serial.println();
#endif
}

// mfoc-hardnested will attempt to target keys sector-by-sector, starting with
// the lowest-numbered secured sector.
//
// Using PN532 boards, it takes about 5-30 minutes (depending on luck and
// timing) to crack one key for each sector.
//
// Using the default sector access trailer value of FF0780, both Key A and Key B
// can do everything, and Key A can read Key B.
// By restarting mfoc-hardnested with additional discovered Key A values as `-k`
// options, we can more quickly recover Key B values when it initially checks
// for key reuse.
//
// Goal #1: Dump and clone the contents of Sector 1
// Goal #2: Dump and clone the contents of all the sectors, using Sector 0 or 1
//    as an exploit sector against the rest of the sectors
//
// Reader logic:
// - Select UID and determine type/SAK/ATQA. Exit if type is unknown.
// - Based on the UID, compute diversified keys for each of the sectors (1+)
// - Read Sector 1, Block 4 -- if the content matches what is expected, play the success sound and lights
// - Read all Sectors (1-15) and blocks (5-63)
// -- Play sounds based on sector content?

void loop()
{
  defaultLED();
  hardMode = false;
  sectorOneValid = false;
  allSectorsValid = false;
  memset(buffer, 0x00, bufferLen);

  if (!mfrc522.PICC_IsNewCardPresent())
  {
#ifdef DEBUG_PRINTING
    Serial.println("No new card present");
#endif
    goto finalize;
  }
  if (!mfrc522.PICC_ReadCardSerial())
  {
#ifdef DEBUG_PRINTING
    // Serial.println("no serial read");
#endif
    goto finalize;
  }

  // Zero out the buffer space
  for (int i = 0; i < 64; i++)
  {
    memset(dataBuffer[i], 0x00, 16);
  }

  // This scheme only supports 4-byte UIDs
  if (mfrc522.uid.size != 4)
  {
    Serial.printf("Invalid UID Length: %d\n", mfrc522.uid.size);
    errorLED();
    playAudioFailure();
    goto finalize;
  }

  sector = 1;
  keyType = 'a';
  diversifyKey(mfrc522.uid, &sector, &keyType, KEY_DIVERSIFICATION_SECRET, &sectorKeyA);
  // Read Block 4 for the "easy mode"/"hard mode" flag
  status_code = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &sectorKeyA, &(mfrc522.uid));
  if (status_code != MFRC522::STATUS_OK)
  {
#ifdef DEBUG_PRINTING
    Serial.println("Error Authenticating to Sector 1, Block 4 with Key A");
    Serial.print("Key A: ");
    for (int i = 0; i < 6; i++) {
      Serial.print(sectorKeyA.keyByte[i], HEX);
    }
    Serial.println();
#endif
    errorLED();
    playAudioFailure();
    goto finalize;
  }
  status_code = mfrc522.MIFARE_Read(4, buffer, &bufferLen);
  if (status_code != MFRC522::STATUS_OK)
  {
#ifdef DEBUG_PRINTING
    Serial.println("Error reading Block 4");
#endif
    errorLED();
    playAudioFailure();
    goto finalize;
  }
  memcpy(dataBuffer[4], buffer, 16);
#ifdef DEBUG_PRINTING
  Serial.print("Read hard-mode byte: ");
  Serial.print(dataBuffer[4][0], HEX);
  Serial.println();
#endif
  if (dataBuffer[4][0] == 0xFF)
  {
    Serial.println("EASY MODE");
    hardMode = false;
  }
  else
  {
    Serial.println("HARD MODE");
    hardMode = true;
  }

  // Read Block 5 for the "easy mode" flag
  status_code = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 5, &sectorKeyA, &(mfrc522.uid));
  if (status_code != MFRC522::STATUS_OK)
  {
#ifdef DEBUG_PRINTING
    Serial.println("Error authenticating to Sector 1, Block 5, Key A");
    Serial.print("Key A: ");
    for (int i = 0; i < 6; i++) {
      Serial.print(sectorKeyA.keyByte[i], HEX);
    }
#endif
    errorLED();
    playAudioFailure();
    goto finalize;
  }
  status_code = mfrc522.MIFARE_Read(5, buffer, &bufferLen);
  if (status_code != MFRC522::STATUS_OK)
  {
#ifdef DEBUG_PRINTING
    Serial.println("Error reading Block 5");
#endif
    errorLED();
    playAudioFailure();
    goto finalize;
  }
  memcpy(dataBuffer[5], buffer, 16);
#ifdef DEBUG_PRINTING
  Serial.print("Read easy-flag:  ");
  for (int i = 0; i < 16; i++) {
    Serial.print(dataBuffer[5][i], HEX);
  }
  Serial.println();
  Serial.print("Known easy-flag: ");
  for (int i = 0; i < 16; i++) {
    Serial.print(secretFlag[i], HEX);
  }
  Serial.println();
#endif
  if (memcmp(dataBuffer[5], secretFlag, 16) == 0)
  {
    sectorOneValid = true;
    Serial.println("Easy-mode flag is correct");
    if (hardMode == false)
    {
      // If playing on "easy mode", and the flag is correct; success!
      successLED();
      playAudioSuccess();
      goto finalize;
    }
  } else {
    sectorOneValid = false;
    Serial.println("Easy-mode flag is incorrect");
    errorLED();
    playAudioFailure();
    goto finalize;
  }
  // If we are this far, then we should be playing on hard mode
  // Read in the rest of the hard-mode blocks:
  for (int block = 6; block < 64; block++) {
    if (block % 4 == 3) {
      // Skip reading sector trailer blocks
      continue;
    }
    sector = block / 4;
    Serial.printf("Will read Sector %02d, Block %02d", sector, block);
    Serial.println();
    diversifyKey(mfrc522.uid, &sector, &keyType, KEY_DIVERSIFICATION_SECRET, &sectorKeyA);
    status_code = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &sectorKeyA, &(mfrc522.uid));
    if (status_code != MFRC522::STATUS_OK) {
      Serial.printf("Error Authenticating to Sector %02d, Block %02d\n", sector, block);
      errorLED();
      playAudioFailure();
      goto finalize;
    }
    status_code = mfrc522.MIFARE_Read(block, buffer, &bufferLen);
    if (status_code != MFRC522::STATUS_OK) {
      Serial.printf("Error reading from Block %02d\n", block);
      errorLED();
      playAudioFailure();
      goto finalize;
    }
    memcpy(dataBuffer[block], buffer, 16);
    Serial.printf("Read hard-mode bytes from Block %02d: ", block);
    for (int i = 0; i < 16; i++) {
      Serial.print(dataBuffer[block][i], HEX);
    }
    Serial.println();
  }
  // Validate the contents of hard-mode flags in dataBuffer
  for (int block = 6; block < 64; block++) {
    if (block % 4 == 3) {
      // Skip reading sector trailer blocks
      continue;
    }
    for (int i = 0; i < 16; i++) {
      if (dataBuffer[block][i] != block) {
        Serial.printf("Bad hard-mode block content in Block %02d: %02d != %02d\n", block, dataBuffer[block][i], block);
        errorLED();
        playAudioFailure();
        goto finalize;
      }
    }
  }
  allSectorsValid = true;
  if (sectorOneValid && allSectorsValid) {
    Serial.println("HARD MODE WINNER!");
    successLED();
    playAudioSuccess();
    goto finalize;
  }


finalize:
  mfrc522.PCD_StopCrypto1();
  mfrc522.PICC_HaltA();
  delay(1000);
}
