// ================================================================================================
// This header file declares functions to control a vibration motor so can be used in other files.
// ================================================================================================
#ifndef VIBRATION_H
#define VIBRATION_H

#include <Arduino.h>

// Initialize vibration motor pin — call once in setup()
void vibrationInit();

//Start a vibration pulse for the specified duration (1000 ms)
void vibrationPulse(unsigned long durationMs = 1000);  

// Call every loop(); returns true while active
bool vibrationUpdate();  

// Turn vibration motor on immediately
void vibrationOn();

// Turn vibration motor off immediately
void vibrationOff();

#endif 
