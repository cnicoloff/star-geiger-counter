/*
 *****************************************************************************
 * star.c:  Software control of the Raspberry Pi based STAR radiation
 *          monitor.
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
#include "geiger.h"
#include "MS5607.h"

static volatile bool keepRunning;

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
 * post: Thread to perform a power on self-test.
 *****************************************************************************
 */

void *post (void *vargp) {

  printf("POST for next 10 seconds...\n");
  HVOn();
  sleep(10);
  HVOff();
  printf("POST complete!\n");

  pthread_exit(NULL);
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

  geigerSetup();             // Setup the Geiger circuit

  sleep(1);                  // Sleep 1s just so we don't power everything on at once

  altimeterSetup();          // Setup the altimeter

  pthread_t post_id;   // Set up the POST thread
  pthread_create(&post_id, &attr, post, NULL);

  // Loop forever or until CTRL-C
  while (keepRunning) {

    sleep(1);  // Sleep for 1 second

    // Every 20 seconds, give some output
    if (getSecNum() % 20 == 0) {
      float uSv = cpmTouSv(120);
      double T = readTUncompensated();
      double P = readPUncompensated();
      double T2 = firstOrderT(T);
      double P2 = secondOrderP(T, P);

      // Write some output
      printf("uSv/hr: %f, T: %f C (%f F), P: %f mbar, h: %f m\n", uSv, T2, CtoF(T2), P2, PtoAlt(P2, T2));
      HVOn();  // FIXME: Base this on altitude
    }

    // Every minute...
    if (getSecNum() % 60 == 0) {
    }
  }

  fclose(opf);                  // Close the output file
  pthread_attr_destroy(&attr);  // Clean up
  HVOff();                      // Make sure HV is off
  LEDOff();                     // Make sure LED is off

  return EXIT_SUCCESS;          // Exit
}
