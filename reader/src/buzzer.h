#include <Arduino.h>

#define AUDIO_BUS (25)

#define NOTE_DURATION 50 // duration of each note in milliseconds
#define NOTE_PAUSE 12    // pause between notes in milliseconds

void playAudioNote(note_t note, uint8_t octave, uint16_t duration, uint16_t pause);
void playAudioSuccess();
void playAudioFailure();
void setupAudio();
void playAudio();
void playAudioError();
