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
#include "star_common.h"
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
  printf("QFF: %f\n", getQFF());

  //pthread_t post_id;         // Set up the POST thread
  //pthread_create(&post_id, &attr, post, NULL);

  float uSv;
  double T, P, T2, P2, alt;

  HVOn();  // FIXME: Base this on altitude

  // Loop forever or until CTRL-C
  while (keepRunning) {

    waitNextNanoSec(1000000000);  // Sleep until next second

    uSv = cpmTouSv(120);
    T = readTUncompensated();
    P = readPUncompensated();
    T2 = roundPrecision(calcFirstOrderT(T), 2);
    P2 = roundPrecision(calcSecondOrderP(T, P), 2);
    alt = roundPrecision(calcAltitude(P2, T2), 1);
    // Write some output
    printf("uSv/hr: %2.2f, T: %3.2f C (%3.2f F), P: %4.2f mbar, h: %7.2f m\n", uSv, T2, cvtCtoF(T2), P2, alt);
  }

  pthread_attr_destroy(&attr);  // Clean up
  fclose(opf);                  // Close the output file
  geigerStop();                 // Stop the Geiger circuit

  return EXIT_SUCCESS;          // Exit
}
