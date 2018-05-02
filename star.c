/*
 *****************************************************************************
 * star.c:  Software control of the STAR radiation monitor.
 *
 * Copyright 2018, Catherine Nicoloff, GNU GPL-3.0-or-later
 *****************************************************************************
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

struct data_second {
   float elapsed;
   int counts;
   unsigned long T;
   double T1;
   unsigned long P;
   double P1;
   double P2;
   float altitude;
};

static volatile bool keepRunning;     // main() infinite loop
static int buffer_seconds = 10;

/*
 * breakHandler: Captures CTRL-C so we can shut down cleanly.
 *****************************************************************************
 */

void breakHandler(int s) {

  printf("%s CTRL-C, exiting.\n", getTimeStamp());

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
  // Buffer a certain number of seconds of data to log to file
  struct data_second data[buffer_seconds];

  // Set up a signal handler for CTRL-C
  struct sigaction act;
  act.sa_handler = breakHandler;
  sigaction(SIGINT, &act, NULL);

  // Define the output file
  FILE *csvf;
  char csvfname[] = "counts.txt";
  // FIXME: Does this need to be binary?
  csvf = fopen(csvfname, "awb");  // Attempt to open our output file, write+binary, append

  // If we failed to open the file, complain and exit
  if (csvf == NULL) {
    fprintf(stderr, "Can't open output file!\n");
    exit(1);
  }

  setbuf(csvf, NULL);             // Do not buffer, write directly to disk!

  // Define the log file
  FILE *errf;
  char errfname[] = "error.txt";
  errf = fopen(errfname, "aw");  // Attempt to open our log file, write, append

  // If we failed to open the file, complain and exit
  if (errf == NULL) {
    fprintf(stderr, "Can't open log file!\n");
    exit(1);
  }

  setbuf(errf, NULL);            // Do not buffer, write directly to disk!

  // Set up the attribute that allows our threads to run detached
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  keepRunning = true;        // Run forever unless halted
  altimeterSetup();          // Setup the altimeter
  fprintf(errf, "%s altimeterSetup()\n", getTimeStamp());

  setQFF(42.29, 46, 1);
  fprintf(errf, "%s Calculated QFF = %f\n", getTimeStamp(), getQFF());

  //pthread_t post_id;         // Set up the POST thread
  //pthread_create(&post_id, &attr, post, NULL);

  unsigned long start_time;   // Time the main loop started
  float elapsed;              // Elapsed time since the main loop started
  int curSec;                 // The current second we are addressing in the counts buffer
  int bufSec = 0;             // The current second we are addressing in the write buffer
  int counts;                 // Number of counts in the last second

  sleep(1);                  // Sleep 1s just so we don't power everything on at once

  geigerSetup();             // Setup the Geiger circuit
  fprintf(errf, "%s geigerSetup()\n", getTimeStamp());
  geigerStart();             // Start the Geiger circuit
  fprintf(errf, "%s geigerStart()\n", getTimeStamp());

  HVOn();                    // FIXME: Base this on altitude
  fprintf(errf, "%s HVOn()\n", getTimeStamp());

  geigerReset();             // Reset the Geiger counting variables
  fprintf(errf, "%s geigerReset()\n", getTimeStamp());

  fprintf(csvf, "Elapsed, Counts, T (Raw), T1 (C), P (Raw), P1 (mbar), P2 (mbar), Altitude (m)\n");
  fprintf(stdout, "----------+------+---------+--------+---------+----------+----------+---------\n");
  fprintf(stdout, "  Elapsed |    N |       T |     T1 |       P |       P1 |       P2 |        H\n");
  fprintf(stdout, "----------+------+---------+--------+---------+----------+----------+---------\n");

  fprintf(errf, "%s entering main()\n", getTimeStamp());

  waitNextSec();                // Sleep until next second

  start_time = getTimeMS();     // Save the start time

  // Loop forever or until CTRL-C
  while (keepRunning) {

    // Elapsed time since start
    elapsed = (getTimeMS() - start_time) / 1000.0;

    // Get the counts from the last second
    counts = sumCounts(1);

    // Whole number of current second
    curSec = (long)elapsed;

    // Advance the count timer
    geigerSetTime(curSec);

    // Wrap around the circular write buffer
    bufSec = curSec % buffer_seconds;

    // Every so often, print the header to screen
    if ((curSec % 20 == 0) && (curSec != 0)) {
      fprintf(stdout, "----------+------+---------+--------+---------+----------+----------+---------\n");
      fprintf(stdout, "  Elapsed |    N |       T |     T1 |       P |       P1 |       P2 |        H\n");
      fprintf(stdout, "----------+------+---------+--------+---------+----------+----------+---------\n");
    }

    data[bufSec].elapsed = elapsed;
    data[bufSec].counts = counts;
    data[bufSec].T = readTUncompensated();
    data[bufSec].P = readPUncompensated();

    // Do some calculations
    data[bufSec].T1 = calcFirstOrderT(data[bufSec].T);
    data[bufSec].P1 = calcFirstOrderP(data[bufSec].T, data[bufSec].P);
    data[bufSec].P2 = calcSecondOrderP(data[bufSec].T, data[bufSec].P);
    data[bufSec].altitude = calcAltitude(data[bufSec].P2, data[bufSec].T1);

    // Every so often, write to file
    if (bufSec == (buffer_seconds - 1)) {
      for (int i = 0; i < buffer_seconds; i++) {
        fprintf(csvf, "%f, %d, %ld, %f, %ld, %f, %f, %f\n", data[i].elapsed, data[i].counts, data[i].T, data[i].T1, data[i].P, data[i].P1, data[i].P2, data[i].altitude);
      }
    }

    // Write some output
    fprintf(stdout, "%9.3f | %4d | %7ld | %6.2f | %7ld | %7.3f | %7.3f | %8.2f\n", data[bufSec].elapsed, data[bufSec].counts, data[bufSec].T, data[bufSec].T1, data[bufSec].P, data[bufSec].P1, data[bufSec].P2, data[bufSec].altitude);

    waitNextSec();  // Sleep one second
  }

  fprintf(errf, "%s exiting main()\n", getTimeStamp());

  geigerStop();                 // Stop the Geiger circuit
  fprintf(errf, "%s geigerStop()\n", getTimeStamp());

  pthread_attr_destroy(&attr);  // Clean up

  fclose(csvf);                 // Close the output file
  fprintf(errf, "%s Closed output file.\n", getTimeStamp());

  fprintf(errf, "%s Closing log file.\n", getTimeStamp());
  fclose(errf);                 // Close the log file

  return EXIT_SUCCESS;          // Exit
}
