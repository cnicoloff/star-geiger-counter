/*
 *****************************************************************************
 * geiger.c:  interfaces a Raspberry Pi with the custom Geiger circuit 
 *            inside STAR.
 *
 * (C) 2018, Catherine Nicoloff, GNU GPL-3.0-or-later
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

static int size = 60;             // Array size
static volatile int secNum;       // Which index in the seconds array
static volatile int sec[60]={0};  // Array of collected counts per second
static volatile int minNum;       // Which index of the minutes array
static volatile int min[60]={0};  // Array of collected counts per minute
static volatile int hourNum;      // Which index of the hours array
static volatile int hour[60]={0}; // Array of collected counts per hour
static volatile int elapsed;      // How many seconds have elapsed

static volatile int LEDTime;      // How much time is left to light LED
static volatile bool LEDisOn;     // Is LED on?
static volatile bool keepRunning; // Signals when to exit
static volatile bool turnHVOn;    // Signals when to turn the HV on
static volatile bool HVisOn;      // Is HV on?

// Initialize the GPIO pins.  Note that these are not the BCM GPIO pin 
// numbers or the physical header pin numbers!  Conversion table is at 
// http://wiringpi.com/pins/
static int ledPin = 4;
static int geigerPin = 5;
static int gatePin = 6;

// How long to flash the LED when a count is recorded, in milliseconds
static int flashTime = 10;


/*
 * countInterrupt: Runs when a count is detected.
 *****************************************************************************
 */

void countInterrupt(void) {
  // Increment the various counters
  sec[secNum]++;
  min[minNum]++;
  hour[hourNum]++;

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
 * getSecNum: 
 *****************************************************************************
 */

int getSecNum(void) {
  return secNum;
}

/*
 * getMinNum: 
 *****************************************************************************
 */

int getMinNum(void) {
  return minNum;
}

/*
 * getHourNum: 
 *****************************************************************************
 */

int getHourNum(void) {
  return hourNum;
}

/*
 * getIndex: Get the counts for a particular index from the circular buffer.
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

  int total = 0;                                           // Total counts
  int numHours = numSecs / 3600;                           // Seconds to hours
  int numMins = (numSecs - (numHours * 3600)) / 60;        // Seconds to minutes
  numSecs = numSecs - (numMins * 60) - (numHours * 3600);  // Remaining seconds

  // Sum hours
  for (int i=0; i < numHours; i++) {
    total += hour[getIndex(hourNum - i)];
  }

  // Sum minutes
  for (int i=0; i < numMins; i++) {
    total += min[getIndex(minNum - i)];
  }

  // Sum seconds
  for (int i=0; i < numSecs; i++) {
    total += sec[getIndex(secNum - i)];
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
 * count: Thread to handle count_related activities.
 *****************************************************************************
 */

void *count (void *vargp) {
  
  while (keepRunning) {

    // Increment the elapsed time counter
    // FIXME: Not currently in use
    //elapsed++;

    waitNextNanoSec(1000000000);

    // Increment the seconds counter
    secNum++;

    // Roll the seconds buffer
    if (secNum % size == 0) {

      minNum++;  // Increment the minutes counter

      // Roll the minutes buffer
      if (minNum % size == 0) {

        hourNum++;  // Increment the hours counter

        // Roll the hours buffer
        if (hourNum % size == 0) {
          hourNum = 0;
        }

        minNum = 0;
        hour[hourNum] = 0;  // Initialize the current hour data to zero
      }

      secNum = 0;
      min[minNum] = 0;  // Initialize the current minute data to zero
    }

    sec[secNum] = 0;  // Initialize the current second data to zero
    
    printf("count(): %02d:%02d:%02d\n", hourNum, minNum, secNum);
  }

  pthread_exit(NULL);
}

/*
 * HVOn: Turns HV on and logs it.
 *****************************************************************************
 */

void HVOn (void) {
  if (!HVisOn) {
    digitalWrite(gatePin, HIGH);  // Turn on the MOSFET gate pin
    HVisOn = true;                // HV is now on
    printf("HV = ON\n");        // FIXME: Log to file
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
    printf("HV = OFF\n");       // FIXME: Log to file
  }
}

/*
 * HVControl: Turns HV on and off
 *****************************************************************************
 */

void *HVControl (void *vargp) {

  while (keepRunning) {
    // Is HV supposed to be on?
    if ((turnHVOn) && (!HVisOn)) {
      HVOn();   // Turn HV on
    }
    // Is HV supposed to be off?
    else if ((!turnHVOn) && (HVisOn)) {
      HVOff();  // Turn HV off
    }

    sleep(1);
  }

  HVOff();
  pthread_exit(NULL);
}

/*
 * geigerSetTime: Sets the Geiger counting variables
 *
 *                seconds is the elapsed seconds since start
 *****************************************************************************
 */
void geigerSetTime(unsigned long seconds) {
  
  int numHours = seconds / 3600;                             // Seconds to hours
  int numMins = (seconds - (numHours * 3600)) / 60;            // Seconds to minutes
  int numSecs = seconds - (numMins * 60) - (numHours * 3600);  // Remaining seconds

  // Set counting variables
  if (numHours % size != hourNum) {
    hourNum   = numHours % size;
    hour[hourNum] = 0;
  }
  if (numMins % size != minNum) {
    minNum    = numMins % size;
    min[minNum] = 0;
  }
  if (numSecs % size != secNum) {
    secNum    = numSecs % size;
    sec[secNum] = 0;
  }
  
  printf("geigerSetTime: %02d:%02d:%02d\n", hourNum, minNum, secNum);
}


/*
 * geigerReset: Resets the Geiger counting variables.
 *****************************************************************************
 */
int geigerReset(void) {

  // Initialize counting variables
  hourNum   = 0;
  minNum    = 0;
  secNum    = 0;

  // Initialize the counting arrays
  for (int i=0; i < size; i++) {
    sec[i] = min[i] = hour[i] = 0;
  }

  return 0;
}

/*
 * geigerSetup: Sets up the Geiger circuit.
 *****************************************************************************
 */
int geigerSetup(void) {

  wiringPiSetup(); 

  //turnHVOn = false;          // Do not turn HV on at this time
  HVisOn = false;            // HV is off by default
  pinMode(gatePin, OUTPUT);  // Set up MOSFET gate pin
  //pthread_t HV_id;           // Set up the HV control thread
  //pthread_create(&HV_id, &attr, HVControl, NULL);

  LEDTime = 0;               // Initialize the LED
  LEDisOn = false;           // LED is off by default
  pinMode(ledPin, OUTPUT);   // Set up LED pin

  // Configure wiringPi to detect pulses with a falling
  // edge on the Geiger pin
  wiringPiISR(geigerPin, INT_EDGE_FALLING, &countInterrupt);
  pullUpDnControl(geigerPin, PUD_OFF);  // Pull up/down resistors off

  geigerReset();             // Reset all counting variables
  
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

  //pthread_t count_id;           // Set up the counting thread
  //pthread_create(&count_id, &attr, count, NULL);

  geigerReset();                // Reset all counting variables

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
  LEDOff();                     // Make sure LED is off
}
