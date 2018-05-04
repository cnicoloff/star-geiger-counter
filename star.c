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

// A structure to hold onto the laxt buffer_seconds second of data
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

// Buffer five seconds before writing to file
static int buffer_seconds = 5;

// Keep the main loop running forever unless CTRL-C
static volatile bool keepRunning;

static int geigerAlt = 40;
static int deadBand = 10;

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
 *****************************************************************************
 * main
 *****************************************************************************
 */

int main (void)
{

  int result = 0;             // Result of file operations
  bool doPost = true;         // Do a POST when first started
  unsigned long start_time;   // Time the main loop started
  float elapsed = 0.0;        // Elapsed time since the main loop started
  unsigned long curSec = 0;   // The current second we are addressing in the counts buffer
  int bufSec = 0;             // The current second we are addressing in the write buffer
  int counts = 0;             // Number of counts in the last second


  // Buffer a certain number of seconds of data to log to file
  struct data_second data[buffer_seconds];

  // Set up a signal handler for CTRL-C
  struct sigaction act;
  act.sa_handler = breakHandler;
  sigaction(SIGINT, &act, NULL);

  // Define the output file
  FILE *csvf;
  char csvfname[] = "counts.txt";
  csvf = fopen(csvfname, "aw");  // Attempt to open our output file, write+binary, append

  // If we failed to open the file, complain and exit
  if (csvf == NULL) {
    fprintf(stderr, "Can't open data file!\n");
    exit(1);
  }

  // Do not buffer, write directly to disk!
  setbuf(csvf, NULL);

  // Define the log file
  FILE *errf;
  char errfname[] = "error.txt";
  errf = fopen(errfname, "aw");  // Attempt to open our log file, write+binary, append

  // If we failed to open the file, complain and exit
  if (errf == NULL) {
    fprintf(stderr, "Can't open log file!\n");
    exit(1);
  }

  // Do not buffer, write directly to disk!
  setbuf(errf, NULL);

  keepRunning = true;        // Run forever unless halted

  fprintf(errf, "%s ****************************************\n", getTimeStamp());

  altimeterSetup();          // Setup the altimeter
  fprintf(errf, "%s altimeterSetup()\n", getTimeStamp());

  setQFF(42.29, 46, 1);
  fprintf(errf, "%s setQFF(42.29, 46, 1) = %f\n", getTimeStamp(), getQFF());

  sleep(1);                  // Sleep 1s just so we don't power everything on at once

  geigerSetup();             // Setup the Geiger circuit
  fprintf(errf, "%s geigerSetup()\n", getTimeStamp());
  geigerStart();             // Start the Geiger circuit
  fprintf(errf, "%s geigerStart()\n", getTimeStamp());

  fprintf(errf, "%s entering main()\n", getTimeStamp());

  fprintf(stdout, "----------+------+---------+--------+---------+----------+----------+---------\n");
  fprintf(stdout, "  Elapsed |    N |       T |     T1 |       P |       P1 |       P2 |        H\n");
  fprintf(stdout, "----------+------+---------+--------+---------+----------+----------+---------\n");

  waitNextSec();                // Sleep until next second
  curSec = 0;                   // Start at zero seconds
  geigerSetTime(curSec);        // Syncronize the Geiger counting loop
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
    bufSec = (int)(curSec % buffer_seconds);

    // Every so often, print the header to screen
    if ((curSec % 20 == 0) && (curSec != 0)) {
      fprintf(stdout, "----------+------+---------+--------+---------+----------+----------+---------\n");
      fprintf(stdout, "  Elapsed |    N |       T |     T1 |       P |       P1 |       P2 |        H\n");
      fprintf(stdout, "----------+------+---------+--------+---------+----------+----------+---------\n");
    }

    // Put data in our struct
    data[bufSec].elapsed = elapsed;

    // If HV is on, record counts
    if (getHVOn() == true) {
      data[bufSec].counts = counts;
    }
    // If HV is not on, record an impossible result
    else {
      data[bufSec].counts = -1;
    }

    data[bufSec].T = readTUncompensated();
    data[bufSec].P = readPUncompensated();

    // Do some calculations and put them into the struct
    data[bufSec].T1 = calcFirstOrderT(data[bufSec].T);
    data[bufSec].P1 = calcFirstOrderP(data[bufSec].T, data[bufSec].P);
    data[bufSec].P2 = calcSecondOrderP(data[bufSec].T, data[bufSec].P);
    data[bufSec].altitude = calcAltitude(data[bufSec].P2, data[bufSec].T1);

    // If HV is not on
    if (getHVOn() == false) {
      // If we're above our threshold altitude, turn HV on
      if (data[bufSec].altitude > geigerAlt) {

        fprintf(csvf, "Elapsed, Counts, T (Raw), T1 (C), P (Raw), P1 (mbar), P2 (mbar), Altitude (m)\n");

        HVOn();                    // Turn the Geiger tube on
        fprintf(errf, "%s HVOn()\n", getTimeStamp());
      }
      // If we're below our threshold altitude and we haven't done a POST, do a POST
      else if ((doPost) && (data[bufSec].altitude < (geigerAlt - deadBand))) {
        fprintf(errf, "%s POST()\n", getTimeStamp());
        doPost = false;
      }
    }

    // If HV is on and we're below our threshold altitude, turn HV off
    else if ((getHVOn() == true) && (data[bufSec].altitude < (geigerAlt - deadBand))) {
      HVOff();                   // Turn the Geiger tube off
      fprintf(errf, "%s HVOff()\n", getTimeStamp());
    }

    // Every so often, write to file
    if (bufSec == (buffer_seconds - 1)) {
      for (int i = 0; i < buffer_seconds; i++) {
        fprintf(csvf, "%f, %d, %ld, %f, %ld, %f, %f, %f\n", data[i].elapsed, data[i].counts, data[i].T, data[i].T1, data[i].P, data[i].P1, data[i].P2, data[i].altitude);
      }
    }

    if ((curSec % 60 == 0) && (curSec != 0)) {
      fprintf(errf, "%s main() 60 seconds\n", getTimeStamp());
    }

    // Write some output
    fprintf(stdout, "%d: %9.3f | %4d | %7ld | %6.2f | %7ld | %7.3f | %7.3f | %8.2f\n", getSecNum(), data[bufSec].elapsed, data[bufSec].counts, data[bufSec].T, data[bufSec].T1, data[bufSec].P, data[bufSec].P1, data[bufSec].P2, data[bufSec].altitude);

    waitNextSec();              // Sleep until next second
  }

  if (getHVOn() == true) {
    HVOff();                    // Turn the Geiger tube off
    fprintf(errf, "%s HVOff()\n", getTimeStamp());
  }

  fprintf(errf, "%s exiting main()\n", getTimeStamp());

  geigerStop();                 // Stop the Geiger circuit
  fprintf(errf, "%s geigerStop()\n", getTimeStamp());

  fclose(csvf);                 // Close the output file
  fprintf(errf, "%s Closed output file.\n", getTimeStamp());

  fprintf(errf, "%s Closing log file.\n", getTimeStamp());
  fprintf(errf, "%s ****************************************\n", getTimeStamp());
  result = fclose(errf);        // Close the log file
  if (result != 0) {
    printf("Error closing log file!\n");
  }

  return EXIT_SUCCESS;          // Exit
}
