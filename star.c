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
  FILE *csvf;
  char csvfname[] = "out.txt";
  csvf = fopen(csvfname, "awb");  // Attempt to open our output file, write+binary, append

  // If we failed to open the file, complain and exit
  if (csvf == NULL) {
    fprintf(stderr, "Can't open output file!\n");
    exit(1);
  }

  // Define the log file
  FILE *errf;
  char errfname[] = "error.txt";
  errf = fopen(errfname, "aw");  // Attempt to open our log file, write, append

  // If we failed to open the file, complain and exit
  if (errf == NULL) {
    fprintf(stderr, "Can't open log file!\n");
    exit(1);
  }

  // Set up the attribute that allows our threads to run detached
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  keepRunning = true;        // Run forever unless halted
  altimeterSetup();          // Setup the altimeter
  
  setQFF(42.29, 46, 1);
  printf("Calculated QFF = %f\n", getQFF());

  //pthread_t post_id;         // Set up the POST thread
  //pthread_create(&post_id, &attr, post, NULL);

  float uSv;
  double T, P, T2, P2, alt;
  unsigned long ms;
  float elapsed;
  int counts;

  sleep(1);                  // Sleep 1s just so we don't power everything on at once
  geigerSetup();             // Setup the Geiger circuit
  geigerStart();             // Start the Geiger circuit
  HVOn();                    // FIXME: Base this on altitude
  
  ms = getTimeMS();          // Save the start time

  geigerReset();             // Reset the Geiger counting variables

  // Loop forever or until CTRL-C
  while (keepRunning) {

    elapsed = (getTimeMS() - ms) / 1000.0;
    counts = sumCounts(1);
    geigerSetTime((long)elapsed);
    
    uSv = cpmTouSv(120);
    T = readTUncompensated();
    P = readPUncompensated();
    T2 = roundPrecision(calcFirstOrderT(T), 2);
    P2 = roundPrecision(calcSecondOrderP(T, P), 2);
    alt = roundPrecision(calcAltitude(P2, T2), 1);
    // Write some output
    fprintf(stdout, "Elapsed: %.3f  Counts: %d, T: %3.1f C (%3.1f F), P: %.0f mbar, h: %.0f m\n", elapsed, counts, T2, cvtCtoF(T2), P2, alt);
    fprintf(errf, "Elapsed: %.3f  uSv/hr: %d, T: %3.1f C (%3.1f F), P: %.0f mbar, h: %.0f m\n", elapsed, counts, T2, cvtCtoF(T2), P2, alt);

    waitNextNanoSec(1000000000);  // Sleep until next second
  }

  geigerStop();                 // Stop the Geiger circuit
  pthread_attr_destroy(&attr);  // Clean up
  fclose(csvf);                 // Close the output file
  fclose(errf);                 // Close the log file

  return EXIT_SUCCESS;          // Exit
}
