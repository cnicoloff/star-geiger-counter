/*
 *****************************************************************************
 * geiger.c:  interfaces a Raspberry Pi with the custom Geiger circuit 
 *            inside STAR.
 *
 *
 * Copyright 2018 by Catherine Nicoloff, GNU GPL-3.0-or-later
 *****************************************************************************
 * This file is part of STAR.
 *
 * STAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * STAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with STAR.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <wiringPi.h>
#include "star_common.h"

#ifdef DEBUG
  #define DEBUG_PRINT(...) do { fprintf(stderr, __VA_ARGS__); } while(false)
#else
  #define DEBUG_PRINT(...) do { } while (false)
#endif

int secNum;            // Which index in the seconds array
int sec[60];           // Array of collected counts per second

int LEDTime;           // How much time is left to light LED
bool LEDisOn;          // Is LED on?
bool keepRunning;      // Signals when to exit
bool turnHVOn;         // Signals when to turn the HV on
bool HVisOn;           // Is HV on?

pthread_mutex_t lock;

static int size = 60;  // Array size

// Initialize the GPIO pins.  Note that these are not the BCM GPIO pin
// numbers or the physical header pin numbers!  Conversion table is at
// http://wiringpi.com/pins/
static int ledPin = 4;
static int geigerPin = 5;
static int gatePin = 6;

// How long to flash the LED when a count is recorded, in milliseconds
static int flashTime = 10;



/*
 * getSecNum: Gets the current Geiger counting second.
 *****************************************************************************
 */

int getSecNum(void) {
  int ret;

  pthread_mutex_lock(&lock);
  DEBUG_PRINT("    getSecNum()\n");
  DEBUG_PRINT("        secNum: %d\n", secNum);
  ret = secNum;
  pthread_mutex_unlock(&lock);

  return ret;
}

/*
 * setSecNum: Sets the current Geiger counting second
 *
 *            seconds is the elapsed seconds since start
 *****************************************************************************
 */

void setSecNum(unsigned long seconds) {

  int numSecs = 0;

  pthread_mutex_lock(&lock);

  DEBUG_PRINT("    setSecNum(%ld)\n", seconds);
  DEBUG_PRINT("        seconds: %ld\n", seconds);

  // We only care about the seconds buffer
  numSecs = seconds % size;
  DEBUG_PRINT("        numSecs: %d\n", numSecs);

  // If it's not the same second, initialize the array element
  if (numSecs != secNum) {
    sec[numSecs] = 0;
  }
  secNum = numSecs;
  DEBUG_PRINT("        secNum: %d\n", secNum);

  pthread_mutex_unlock(&lock);
}

/*
 * countInterrupt: Runs when a count is detected.
 *****************************************************************************
 */

void countInterrupt(void) {
  DEBUG_PRINT("countInterrupt()\n");
  // Increment the counter
  sec[getSecNum()]++;

  // Tell the LED thread to light up
  LEDTime += flashTime;
}

/*
 * LEDOn: Turns LED on
 *****************************************************************************
 */

void LEDOn (void) {

  digitalWrite(ledPin, HIGH); // Turn on the LED
  LEDisOn = true;             // The LED is now on
}

/*
 * LEDOff: Turns LED off
 *****************************************************************************
 */

void LEDOff (void) {

  digitalWrite(ledPin, LOW); // Turn off the LED
  LEDisOn = false;           // The LED is now off
}

/*
 * blinkLED: Thread to handle LED blinking.
 *****************************************************************************
 */

void *blinkLED (void *vargp) {


  // Set up nanosleep() for blinking
  struct timespec tim;
  tim.tv_sec = 0;
  tim.tv_nsec = flashTime * 1000000;  // Convert from ns to ms

  // Set up nanosleep() for preventing 100% CPU use
  struct timespec tim2;
  tim2.tv_sec = 0;
  tim2.tv_nsec = 1000000;            // 1 ms

  LEDisOn = false;                   // LED is off by default

  while (keepRunning) {

    // If the LED is supposed to be lit
    if ((LEDTime > 0) && (!LEDisOn)) {
      LEDOn();                    // Turn on the LED
      LEDTime -= flashTime;       // Subtract the time it will be lit
      nanosleep(&tim, NULL);      // Sleep for flashTime ms
    }
    // If the LED is not supposed to be lit
    else if (LEDisOn) {
      LEDOff();                   // Turn off the LED
    }
    // Prevent this thread from using 100% CPU
    else {
      nanosleep(&tim2, NULL);
    }
  }

  // Turn off the LED before exiting the thread
  LEDOff();
  pthread_exit(NULL);
}

/*
 * getIndex: Get a particular index from the circular buffer.
 *****************************************************************************
 */

int getIndex(int numIndex) {

  // Make sure our number is a valid index
  numIndex = numIndex % size;

  // If it's negative, count backwards from the end of the array
  // -1 = the last value in the array, etc.
  if (numIndex < 0) {
    numIndex = size + numIndex;
  }

  return numIndex;
}

/*
 * sumCounts: Sum the number of counts across the last numSecs seconds.
 *****************************************************************************
 */

int sumCounts(int numSecs) {
  DEBUG_PRINT("sumCounts(%d)\n", numSecs);

  int total = 0;

  // Sum seconds
  for (int i=0; i < numSecs; i++) {
    total += sec[getIndex(getSecNum() - i)];
  }

  return total;

}

/*
 * averageCounts: Average the number of counts across a specific number
 *                of seconds.
 *****************************************************************************
 */

float averageCounts(int numSecs) {

  // Sum the counts, divide by the number of seconds
  return ((float)sumCounts(numSecs) / (float)numSecs);
}

/*
 * cpmTouSv: Convert cpm (counts per minute) to microSieverts/hour
 *****************************************************************************
 */

float cpmTouSv(int numSecs) {

  // Conversion factor for SBM-20 tube
  // From https://sites.google.com/site/diygeigercounter/gm-tubes-supported
  //float factor = 0.0057;

  // Conversion factor for SBM-20 tube
  // From https://www.uradmonitor.com/topic/hardware-conversion-factor/
  // This one gave results consistent with another portable radiation
  // monitor I had available (within 1-2%).
  float factor = 0.006315;

  float uSv;
  float cpm;

  // Moving average of counts
  cpm = averageCounts(numSecs) * 60.0;

  // Multiply by conversion factor
  uSv = factor * cpm;

  return uSv;
}

/*
 * HVOn: Turns HV on and logs it.
 *****************************************************************************
 */

void HVOn (void) {
  if (!HVisOn) {
    digitalWrite(gatePin, HIGH);  // Turn on the MOSFET gate pin
    HVisOn = true;                // HV is now on
  }
}

/*
 * HVOff: Turns HV off and logs it.
 *****************************************************************************
 */

void HVOff (void) {
  if (HVisOn) {
    digitalWrite(gatePin, LOW);   // Turn off the MOSFET gate pin
    HVisOn = false;               // HV is now off
  }
}

/*
 * getHVOn: Queries whether HV is on.
 *****************************************************************************
 */

bool getHVOn (void) {
  return HVisOn;
}

/*
 * geigerReset: Resets the Geiger counting variables.
 *****************************************************************************
 */

int geigerReset(void) {

  // Initialize counting variables
  setSecNum(0);

  // Initialize the counting arrays
  for (int i=0; i < size; i++) {
    sec[i] = 0;
  }

  return 0;
}

/*
 * geigerSetup: Sets up the Geiger circuit.
 *****************************************************************************
 */

int geigerSetup(void) {

  wiringPiSetup();

  HVisOn = false;            // HV is off by default
  pinMode(gatePin, OUTPUT);  // Set up MOSFET gate pin

  LEDTime = 0;               // Initialize the LED
  LEDisOn = false;           // LED is off by default
  pinMode(ledPin, OUTPUT);   // Set up LED pin

  // Configure wiringPi to detect pulses with a falling
  // edge on the Geiger pin
  wiringPiISR(geigerPin, INT_EDGE_FALLING, &countInterrupt);
  pullUpDnControl(geigerPin, PUD_OFF);  // Pull up/down resistors off

  pthread_mutex_init(&lock, NULL);

  return 0;
}

/*
 * geigerStart: Starts the Geiger circuit and associated threads
 *              DOES NOT START HV
 *****************************************************************************
 */
void geigerStart() {
  keepRunning = true;           // Keep threads runnning

  // Set up the attribute that allows our threads to run detached
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_t led_id;             // Set up the LED blink thread
  pthread_create(&led_id, &attr, blinkLED, NULL);

  pthread_attr_destroy(&attr);  // Clean up
}

/*
 * geigerStop: Stops the Geiger circuit and associated threads
 *             Turns off HV and LED
 *****************************************************************************
 */
void geigerStop() {
  keepRunning = false;          // Stop running threads
  HVOff();                      // Make sure HV is off
  pthread_mutex_destroy(&lock);
}
