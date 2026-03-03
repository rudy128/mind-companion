// =============================================================
// Speaker — MAX98357A I2S Output + Alarm + OpenAI TTS
// =============================================================
#ifndef SPEAKER_H
#define SPEAKER_H

#include <Arduino.h>

void speakerInit();         // configure I2S #1 in TX mode
void speakerTest();         // blocking 3-tone test — call once in setup() to verify hardware
void speakerPlayTone(float freqHz, unsigned long durationMs); // simple sine tone
void speakerPlayRaw(const uint8_t* pcmData, size_t pcmBytes); // play raw 16-bit PCM buffer (TTS)

// Flag set by openaiSpeak() while TTS is streaming.
// Alarm/buzzer update functions poll this to avoid I2S collisions.
extern volatile bool speakerTTSPlaying;

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
