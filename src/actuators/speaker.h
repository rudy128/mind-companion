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

// Non-blocking nudge buzzer — 3 short beeps at 1000 Hz, then auto-stops.
// Call speakerBuzzerStart() once to kick it off.
// Call speakerBuzzerUpdate() every loop iteration while active.
void speakerBuzzerStart();
void speakerBuzzerStop();
bool speakerBuzzerUpdate();    // returns true while beeping
bool speakerBuzzerIsActive();

#endif // SPEAKER_H
