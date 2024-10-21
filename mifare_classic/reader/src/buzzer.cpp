#include <Arduino.h>
#include <buzzer.h>

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

void setupAudio()
{
  ledcSetup(0, 2500, 8);
  ledcAttachPin(AUDIO_BUS, 0);
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
