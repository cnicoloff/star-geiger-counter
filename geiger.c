/*
 *****************************************************************************
 * geiger.c:  interfaces a Raspberry Pi Zero with the custom Geiger
 *            circuit inside STAR.
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
#include "MS5607.h"

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
 * breakHandler: Captures CTRL-C so we can shut down cleanly.
 *****************************************************************************
 */

void breakHandler(int s) {

  // Tell all loops and threads to exit
  keepRunning = false;
}

/*
 * countInterrupt: Runs when a count is detected.
 *****************************************************************************
 */

void countInterrupt (void) {
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

  // Set up nanosleep()
  struct timespec tim;
  tim.tv_sec = 0;
  tim.tv_nsec = flashTime * 1000000;  // Convert from ns to ms

  LEDisOn = false;

  while (keepRunning) {

    // If the LED is supposed to be lit
    if ((LEDTime > 0) && (!LEDisOn)) {
      LEDOn();                    // Turn on the LED
      LEDTime -= flashTime;       // Subtract the time it will be lit
      nanosleep(&tim, NULL);      // Sleep for flashTime ms
    }
    // If the LED is not supposed to be lit
    else if (LEDisOn) {
      LEDOff();                   // Turn on the LED
    }
  }

  // Turn off the LED before exiting the thread
  LEDOff();
  pthread_exit(NULL);
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
    // FIXME: Make this more accurate
    sleep(1);

    // Increment the elapsed time counter
    // FIXME: Not currently in use
    elapsed++;

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

  }

  pthread_exit(NULL);
}

/*
 * post: Thread to perform a power on self-test.
 *****************************************************************************
 */

void *post (void *vargp) {

  printf("POST for next 10 seconds...\n");
  turnHVOn = true;
  sleep(10);
  turnHVOn = false;
  printf("POST complete!\n");

  pthread_exit(NULL);
}

/*
 * HVOn: Turns HV on and logs it.
 *****************************************************************************
 */

void HVOn (void) {

  digitalWrite(gatePin, HIGH);  // Turn on the MOSFET gate pin
  HVisOn = true;                // HV is now on
  printf("HV is on!\n");        // FIXME: Log to file
}

/*
 * HVOff: Turns HV off and logs it.
 *****************************************************************************
 */

void HVOff (void) {

  digitalWrite(gatePin, LOW);   // Turn off the MOSFET gate pin
  HVisOn = false;               // HV is now off
  printf("HV is off.\n");       // FIXME: Log to file
}

/*
 *****************************************************************************
 * main
 *****************************************************************************
 */

int main (void)
{

  // Set up a signal handler for CTRL-C
  struct sigaction act;
  act.sa_handler = breakHandler;
  sigaction(SIGINT, &act, NULL);

  // Define the output file
  FILE *opf;
  char opfname[] = "out.txt";
  opf = fopen(opfname, "w");  // Attempt to open our output file

  // If we failed to open the file, complain and exit
  if (opf == NULL) {
    fprintf(stderr, "Can't open output file!\n");
    exit(1);
  }

  // Set up the attribute that allows our threads to run detached
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  keepRunning = true;        // Run forever unless halted

  wiringPiSetup();           // Initialize wiringPi

  turnHVOn = false;          // Do not turn HV on at this time
  HVisOn = false;            // HV is off by default
  pinMode(gatePin, OUTPUT);  // Set up MOSFET gate pin

  LEDTime = 0;               // Initialize the LED
  LEDisOn = false;           // LED is off by default
  pinMode(ledPin, OUTPUT);   // Set up LED pin
  pthread_t led_id;          // Set up the LED blink thread
  pthread_create(&led_id, &attr, blinkLED, NULL);

  // Configure wiringPi to detect pulses with a falling
  // edge on the Geiger pin
  wiringPiISR(geigerPin, INT_EDGE_FALLING, &countInterrupt);
  pullUpDnControl(geigerPin, PUD_OFF);  // Pull up/down resistors off

  // Initialize counting variables
  hourNum   = -1;
  minNum    = -1;
  secNum    = 0;
  float uSv = 0.0;

  // Initialize the counting arrays
  for (int i=0; i < size; i++) {
    sec[i] = min[i] = hour[i] = 0;
  }

  pthread_t count_id;  // Set up the counting thread
  pthread_create(&count_id, &attr, count, NULL);

  pthread_t post_id;   // Set up the POST thread
  pthread_create(&post_id, &attr, post, NULL);

  sleep(1);  // Sleep 1s just so we don't power everything on at once

  // Initialize the altimeter variables
  double T = 0.0;
  double P = 0.0;
  double PP = 0.0;

  // Initialize the altimeter
  if (altimeterInit() < 0)
    printf("SPI init failed!\n");  // SPI communications failed
  else
    altimeterReset();              // Reset after power on

  // Get altimeter factory calibration coefficients
  unsigned int coeffs[8] = {0};
  for (int i=0; i < 8; i++) {
    coeffs[i] = altimeterCalibration(i);
    printf("%d: %d\n", i, coeffs[i]);
  }

  // CRC value of the coefficients
  // unsigned char n_crc = crc4(coeffs);

  // Loop forever or until CTRL-C
  while (keepRunning) {

    // Is HV supposed to be on?
    if ((turnHVOn) && (!HVisOn)) {
      HVOn();   // Turn HV on
    }
    // Is HV supposed to be off?
    else if ((!turnHVOn) && (HVisOn)) {
      HVOff();  // Turn HV off
    }

    sleep(1);  // Sleep for 1 second

    // Every 20 seconds, give some output
    if (secNum % 20 == 0) {
      uSv = cpmTouSv(120);
      T = firstOrderT(coeffs);
      P = firstOrderP(coeffs);
      PP = secondOrderP(coeffs);

      // Write some output
      printf("uSv/hr: %f, T: %f C (%f F), P 1st: %f mbar, P 2nd: %f mbar (%f inHg)\n", uSv, T, CtoF(T), P, PP, mbartoInHg(PP));
      turnHVOn = true;  // FIXME: Base this on altitude
    }

    // Every minute...
    if (secNum % 60 == 0) {
    }
  }

  fclose(opf);                  // Close the output file
  pthread_attr_destroy(&attr);  // Clean up
  HVOff();                      // Make sure HV is off
  LEDOff();                     // Make sure LED is off

  return EXIT_SUCCESS;          // Exit
}
