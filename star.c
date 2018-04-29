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
  geigerStart();             // Start the Geiger circuit

  sleep(1);                  // Sleep 1s just so we don't power everything on at once
  altimeterSetup();          // Setup the altimeter
  setQFF(42.29, 45, 2);
  printf("QFF: %f", getQFF());
  //pthread_t post_id;         // Set up the POST thread
  //pthread_create(&post_id, &attr, post, NULL);

  pthread_t led_id;          // Set up the LED blink thread
  pthread_create(&led_id, &attr, blinkLED, NULL);

  pthread_t count_id;        // Set up the counting thread
  pthread_create(&count_id, &attr, count, NULL);

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
      printf("uSv/hr: %2.2f, T: %3.2f C (%3.2f F), P: %4.2f mbar, h: %7.2f m\n", uSv, T2, CtoF(T2), P2, PtoAlt(P2, T2));
      HVOn();  // FIXME: Base this on altitude
    }

    // Every minute...
    if (getSecNum() % 60 == 0) {
    }
  }

  fclose(opf);                  // Close the output file
  pthread_attr_destroy(&attr);  // Clean up
  geigerStop();                 // Stop the Geiger circuit
  HVOff();                      // Make sure HV is off
  LEDOff();                     // Make sure LED is off

  return EXIT_SUCCESS;          // Exit
}
