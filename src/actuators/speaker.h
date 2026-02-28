// =============================================================
// Speaker — MAX98357A I2S Output + Alarm + OpenAI TTS
// =============================================================
#ifndef SPEAKER_H
#define SPEAKER_H

#include <Arduino.h>

void speakerInit();         // configure I2S #1 in TX mode
void speakerPlayTone(float freqHz, unsigned long durationMs); // simple test tone

// Play a short beep sequence to indicate status
void speakerBeepOK();       // two short beeps
void speakerBeepError();    // one long beep

// Non-blocking alarm pattern (replaces buzzer)
// Call speakerAlarmUpdate() every loop iteration while active.
void speakerAlarmStart();
void speakerAlarmStop();
bool speakerAlarmUpdate();  // returns true while alarm is sounding
bool speakerAlarmIsActive();

#endif // SPEAKER_H
