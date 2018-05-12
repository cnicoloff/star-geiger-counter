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

#ifdef DEBUG2
  #define DEBUG2_PRINT(...) do { fprintf(stderr, __VA_ARGS__); } while(false)
#else
  #define DEBUG2_PRINT(...) do { } while (false)
#endif

static int size = 60;           // Array size

volatile int secNum;            // Which index in the seconds array
volatile int sec[60];           // Array of collected counts per second

volatile int LEDTime;           // How much time is left to light LED
volatile bool LEDisOn;          // Is LED on?
volatile bool keepRunning;      // Signals when to exit
volatile bool HVisOn;           // Is HV on?

volatile unsigned long long t1, t2; // Used for dead time calculations
volatile double deadTime[60];       // Array of dead time totals per secons
volatile int deadCounts[60];

pthread_mutex_t lock_sec;       // Prevent a race condition involving secNum read/write
pthread_mutex_t lock_hv;        // Prevent a race condition involving HVisOn read/write
pthread_mutex_t lock_count;     // Prevent a race condition involving t1/t2 read/write
pthread_mutex_t lock_led;       // Prevent a race condition involving LEDTime read/write

// Initialize the GPIO pins.  Note that these are not the BCM GPIO pin
// numbers or the physical header pin numbers!  Conversion table is at
// http://wiringpi.com/pins/
static int ledPin = 4;
static int geigerPin = 5;
static int gatePin = 6;

// How long to flash the LED when a count is recorded, in milliseconds
static int flashTime = 5;



/*
 * getSecNum: Gets the current Geiger counting second.
 *****************************************************************************
 */

int getSecNum(void) {
  int ret;

  pthread_mutex_lock(&lock_sec);
  DEBUG2_PRINT("    getSecNum()\n");
  DEBUG2_PRINT("        secNum: %d\n", secNum);
  ret = secNum;
  pthread_mutex_unlock(&lock_sec);

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

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_count);
  pthread_mutex_lock(&lock_sec);

  DEBUG2_PRINT("    setSecNum(%ld)\n", seconds);
  DEBUG2_PRINT("        seconds: %ld\n", seconds);

  // We only care about the seconds buffer
  numSecs = seconds % size;
  DEBUG2_PRINT("        numSecs: %d\n", numSecs);

  // If it's not the same second, initialize the array element
  if (numSecs != secNum) {
    sec[numSecs] = 0;
    deadTime[numSecs] = 0;
    deadCounts[numSecs] = 0;
    // This is to prevent huge values of LEDTime when
    // counting at high rates
    if (LEDTime > 0) {
      // Prevent other threads from clobbering this value
      pthread_mutex_lock(&lock_led);
      LEDTime = flashTime;
      pthread_mutex_unlock(&lock_led);
    }
  }
  secNum = numSecs;

  DEBUG2_PRINT("        secNum: %d\n", secNum);

  pthread_mutex_unlock(&lock_sec);
  pthread_mutex_unlock(&lock_count);

}

/*
 * countInterrupt: Runs when a count is detected.
 *****************************************************************************
 */

void countInterrupt(void) {
  struct timespec tim1, tim2;
  int numSec;
  double dt_s;

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_count);

  numSec = getSecNum();

  // Waiting for the falling edge
  if (t1 == 0) {
    clock_gettime(CLOCK_MONOTONIC, &tim1);

    // Increment the counter
    sec[numSec]++;

    // Prevent other threads from clobbering this value
    pthread_mutex_lock(&lock_led);
    LEDTime += flashTime;             // Tell the LED to turn on
    pthread_mutex_unlock(&lock_led);

    // Set the time that the falling pulse began and wait
    // for a rising edge
    t1 = t2 = tim1.tv_sec * 1000000000 + tim1.tv_nsec;
  }
  // Waiting for the rising edge
  else if (t1 == t2) {
    clock_gettime(CLOCK_MONOTONIC, &tim2);

    // Set the time that the rising pulse began
    t2 = tim2.tv_sec * 1000000000 + tim2.tv_nsec;

    // Get the time distance between the two
    dt_s = (t2 - t1) / 1000000000.0;

    // The time distance was positive
    if (dt_s > 0) {

      // The time distance was realistic.  800us is around
      // four times the dead time of the tube
      if (dt_s <= 0.000800) {

        // Tally the dead time
        deadCounts[numSec] += 1;
        deadTime[numSec] += dt_s;
        DEBUG_PRINT("    getSecNum() = %d: %lf %lf\n", numSec, dt_s, deadTime[numSec]);

        // Reset and wait for a falling edge
        t1 = 0;
      }

      // The time distance wasn't realistic
      else {
        // Assume we somehow got double falling/rising edges.
        // We don't know which, but we're out of phase,
        // so start the timer over at this point
        t1 = t2;
      }
    }
    // The time distance was negative, I don't know how to handle this.
    // It shouldn't be happening, but I have seen it with CLOCK_REALTIME
    else {
      DEBUG_PRINT("%lld < %lld\n", t2, t1);
    }
  }
  // This state should be impossible, which means it's probable.
  else {
      DEBUG_PRINT("t1 != t2\n");
  }

  pthread_mutex_unlock(&lock_count);
}

/*
 * LEDOn: Turns LED on
 *****************************************************************************
 */

void LEDOn(void) {

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_led);

  digitalWrite(ledPin, HIGH); // Turn on the LED
  LEDisOn = true;             // The LED is now on

  pthread_mutex_unlock(&lock_led);
}

/*
 * LEDOff: Turns LED off
 *****************************************************************************
 */

void LEDOff(void) {

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_led);

  digitalWrite(ledPin, LOW); // Turn off the LED
  LEDisOn = false;           // The LED is now off
  LEDTime = 0;               // Set remaining blink time to zero

  pthread_mutex_unlock(&lock_led);
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
    while (LEDTime > 0) {
      if (!LEDisOn) {
        LEDOn();                  // Turn on the LED
      }

      // Prevent other threads from clobbering this value
      pthread_mutex_lock(&lock_led);
      LEDTime -= flashTime;       // Subtract the time it will be lit
      pthread_mutex_unlock(&lock_led);

      nanosleep(&tim, NULL);      // Sleep for flashTime ms
    }

    // If the LED is not supposed to be lit
    if (LEDisOn) {
      LEDOff();                   // Turn off the LED
    }

    // Prevent this thread from using 100% CPU
    nanosleep(&tim2, NULL);
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
 * getDeadTime: Get the dead time associated with a given second.
 *****************************************************************************
 */

double getDeadTime(int numSecs) {
  double ret;

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_count);
  ret = deadTime[numSecs % size];
  pthread_mutex_unlock(&lock_count);

  return ret;
}

/*
 * getDeadCounts: Get the dead time counts associated with a given second.
 *****************************************************************************
 */

int getDeadCounts(int numSecs) {
  int ret;

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_count);
  ret = deadCounts[numSecs % size];
  pthread_mutex_unlock(&lock_count);

  return ret;
}

/*
 * getCounts: Get the number of counts associated with a given second.
 *****************************************************************************
 */

int getCounts(int numSecs) {
  int ret;

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_count);
  ret = sec[numSecs % size];
  pthread_mutex_unlock(&lock_count);

  return ret;
}

/*
 * sumCounts: Sum the number of counts across the last numSecs seconds.
 *****************************************************************************
 */

int sumCounts(int numSecs) {
  DEBUG2_PRINT("sumCounts(%d)\n", numSecs);

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
    // Prevent other threads from clobbering this value
    pthread_mutex_lock(&lock_hv);

    digitalWrite(gatePin, HIGH);  // Turn on the MOSFET gate pin
    HVisOn = true;                // HV is now on

    pthread_mutex_unlock(&lock_hv);
  }
}

/*
 * HVOff: Turns HV off and logs it.
 *****************************************************************************
 */

void HVOff (void) {
  if (HVisOn) {
    // Prevent other threads from clobbering this value
    pthread_mutex_lock(&lock_hv);

    digitalWrite(gatePin, LOW);   // Turn off the MOSFET gate pin
    HVisOn = false;               // HV is now off

    pthread_mutex_unlock(&lock_hv);
  }
}

/*
 * getHVOn: Queries whether HV is on.
 *****************************************************************************
 */

bool getHVOn (void) {
  bool ret;

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_hv);
  ret = HVisOn;
  pthread_mutex_unlock(&lock_hv);

  return ret;
}

/*
 * geigerReset: Resets the Geiger counting variables.
 *****************************************************************************
 */

int geigerReset(void) {

  // Initialize counting variables
  setSecNum(0);

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_sec);
  // Initialize the counting arrays
  for (int i=0; i < size; i++) {
    sec[i] = 0;
  }
  pthread_mutex_unlock(&lock_sec);

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_count);
  t1 = t2 = 0;
  pthread_mutex_unlock(&lock_count);

  return 0;
}

/*
 * geigerSetup: Sets up the Geiger circuit.
 *****************************************************************************
 */

int geigerSetup(void) {

  wiringPiSetup();           // Initialize wiringPi

  HVisOn = false;            // HV is off by default

  pinMode(gatePin, OUTPUT);  // Set up MOSFET gate pin

  LEDTime = 0;               // Initialize the LED
  LEDisOn = false;           // LED is off by default
  pinMode(ledPin, OUTPUT);   // Set up LED pin

  // Configure wiringPi to detect pulses with a falling
  // edge on the Geiger pin
  wiringPiISR(geigerPin, INT_EDGE_BOTH, &countInterrupt);

  // Pull up/down resistors off for this pin
  pullUpDnControl(geigerPin, PUD_OFF);

  // Initialize the mutexes
  pthread_mutex_init(&lock_sec, NULL);
  pthread_mutex_init(&lock_hv, NULL);
  pthread_mutex_init(&lock_count, NULL);
  pthread_mutex_init(&lock_led, NULL);

  // Prevent other threads from clobbering this value
  pthread_mutex_lock(&lock_count);
  t1 = t2 = 0.0;
  pthread_mutex_unlock(&lock_count);

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

  // Set up the LED blink thread
  pthread_t led_id;
  pthread_create(&led_id, &attr, blinkLED, NULL);

  // Clean up thread attributes
  pthread_attr_destroy(&attr);
}

/*
 * geigerStop: Stops the Geiger circuit and associated threads
 *             Turns off HV and LED
 *****************************************************************************
 */
void geigerStop() {
  keepRunning = false;          // Stop running threads
  HVOff();                      // Make sure HV is off
  LEDOff();                     // Make sure the LED is off

  // Clean up the mutexes
  pthread_mutex_destroy(&lock_sec);
  pthread_mutex_destroy(&lock_hv);
  pthread_mutex_destroy(&lock_count);
  pthread_mutex_destroy(&lock_led);
}
