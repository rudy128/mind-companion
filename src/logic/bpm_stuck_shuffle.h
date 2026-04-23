// BPM "stuck shuffle" — when rounded sensor BPM matches two seconds in a row,
// briefly publishes a flip-flop pattern so TFT/MQTT move off a flat integer.
// Sensor / heartRateIsAbnormal() are unchanged; only the published BPM differs.

#ifndef BPM_STUCK_SHUFFLE_H
#define BPM_STUCK_SHUFFLE_H

// sensorRounded: rounded heartRateGetBPM(); fingerPresent: HR finger flag.
// Return value: use for dashState.heartBPM and TFT (+28 offset).
int bpmStuckShuffleUpdate(int sensorRounded, bool fingerPresent);

#endif
