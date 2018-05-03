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
#include "star_common.h"
#include "geiger.h"
#include "MS5607.h"

#ifdef DEBUG
  #define DEBUG_PRINT(...) do { fprintf(stdout, __VA_ARGS__); } while(false)
#else
  #define DEBUG_PRINT(...) do { } while(false)
#endif

#ifdef DEBUG2
  #define DEBUG2_PRINT(...) do { fprintf(stdout, __VA_ARGS__); } while(false)
#else
  #define DEBUG2_PRINT(...) do { } while(false)
#endif


// A structure to hold onto the last <buffer_seconds> seconds of data
struct data_second {
   double elapsed;
   int counts;
   unsigned long T;
   double T1;
   unsigned long P;
   double P1;
   double P2;
   float altitude;
   double deadTime;
   int deadCounts;
};

// Buffer five seconds before writing to file
static int buffer_seconds = 5;

// Keep the main loop running forever unless CTRL-C
volatile bool keepRunning;

// Minimum altitude before Geiger circuit turns on
static int geigerAlt = 100;

// The dead band keeps small altitude fluctuations from
// turning the Geiger circuit on and off quickly
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

int main (int argc, char *argv[]) {

  int result = 0;             // Result of file operations
  bool doPost = true;         // Do a POST when first started
  long long start_time;       // Time the main loop started
  double elapsed = 0.0;       // Elapsed time since the main loop started
  unsigned long curSec = 0;   // The current second we are addressing in the counts buffer
  int bufSec = 0;             // The current second we are addressing in the write buffer
  int counts = 0;             // Number of counts in the last second
  int deadCounts = 0;         // Number of dead time counts in the last second
  double deadTime = 0;        // Amount of dead time in the last second
  int c[8];                   // Altimeter calibration coefficients
  int opt;                    // Command line options

  // Parse simple command line options
  while ((opt = getopt(argc, argv, "t")) != -1) {
    switch (opt) {
    case 't': geigerAlt = 0; break;  // Bypass the altitude limitations
    default:
      fprintf(stderr, "Usage: %s [-t]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // Buffer a certain number of seconds of data
  // so we're not saving to disk every second
  struct data_second data[buffer_seconds];

  // Set up a signal handler for CTRL-C
  struct sigaction act;
  act.sa_handler = breakHandler;
  sigaction(SIGINT, &act, NULL);

  // Initialize random number generator
  srand(time(NULL));
  int r = rand();

  // Define the output file
  FILE *csvf;
  char csvfname[100];
  sprintf(csvfname, "counts_%s_%d.txt", getTimeStamp(), r);

  // Attempt to open our output file, write+append
  csvf = fopen(csvfname, "aw");

  // If we failed to open the file, complain and exit
  if (csvf == NULL) {
    DEBUG_PRINT("Can't open data file!\n");
    fprintf(stderr, "Can't open data file!\n");
    exit(EXIT_FAILURE);
  }

  // Do not buffer, write directly to disk!
  setbuf(csvf, NULL);

  // Define the log file
  FILE *errf;
  char errfname[100];
  sprintf(errfname, "error_%s_%d.txt", getTimeStamp(), r);

  // Attempt to open our log file, write+append
  errf = fopen(errfname, "aw");

  // If we failed to open the file, complain and exit
  if (errf == NULL) {
    DEBUG_PRINT("Can't open log file!\n");
    fprintf(stderr, "Can't open log file!\n");
    exit(EXIT_FAILURE);
  }

  // Do not buffer, write directly to disk!
  setbuf(errf, NULL);

  // Run forever unless halted
  keepRunning = true;

  DEBUG2_PRINT("%s ****************************************\n", getTimeStamp());
  fprintf(errf, "%s ****************************************\n", getTimeStamp());

  // Setup the altimeter
  altimeterSetup();
  DEBUG2_PRINT("%s altimeterSetup()\n", getTimeStamp());
  fprintf(errf, "%s altimeterSetup()\n", getTimeStamp());

  // Get the altimeter calibration coefficients
  getAltimeterCalibration(c);

  DEBUG2_PRINT("%s getAltimeterCalibration(): ", getTimeStamp());
  fprintf(errf, "%s getAltimeterCalibration(): ", getTimeStamp());
  for (int i = 0; i < 8; i++) {
    fprintf(errf, "%d = %d ", i, c[i]);
  }
  fprintf(errf, "\n");

  // Calculate the QFF value (for low altitudes)
  setQFF(42.29, 46, 1);
  fprintf(errf, "%s setQFF(42.29, 46, 1): %f\n", getTimeStamp(), getQFF());
  DEBUG2_PRINT("%s setQFF(42.29, 46, 1): %f\n", getTimeStamp(), getQFF());

  // Sleep 2s so we don't power everything on at once
  sleep(2);

  // Setup the Geiger circuit
  geigerSetup();
  DEBUG2_PRINT("%s geigerSetup()\n", getTimeStamp());
  fprintf(errf, "%s geigerSetup()\n", getTimeStamp());

  // Start the Geiger circuit
  geigerStart();
  fprintf(errf, "%s geigerStart()\n", getTimeStamp());
  DEBUG2_PRINT("%s geigerStart()\n", getTimeStamp());

  fprintf(errf, "%s entering main()\n", getTimeStamp());
  DEBUG2_PRINT("%s entering main()\n", getTimeStamp());

  waitNextSec();                // Sleep until next second
  start_time = getTimeMS();     // Save the start time
  curSec = 0;                   // Start at zero seconds
  geigerReset();                // Reset the Geiger circuit

  // Loop forever or until CTRL-C
  while (keepRunning) {

    // Elapsed time since start
    elapsed = (getTimeMS() - start_time) / 1000.0;
    DEBUG_PRINT("getTimeMS() %lld, start_time %lld, elapsed %f\n", getTimeMS(), start_time, elapsed);

    // Get the counts from the last second
    counts = getCounts(curSec);

    // Get the dead time from the last second
    deadTime = getDeadTime(curSec);

    // Get the dead time counts from the last second
    deadCounts = getDeadCounts(curSec);

    // Whole number of current second
    curSec = elapsed;

    DEBUG_PRINT("main()\n");

    // Advance the count timer
    setSecNum(curSec);
    DEBUG2_PRINT("setSecNum(%ld)\n", curSec);

    // Wrap around the circular write buffer
    bufSec = (int)(curSec % buffer_seconds);
    DEBUG2_PRINT("bufSec = %d\n", bufSec);

    // Every so often, print the header to screen
    if (curSec % 20 == 0) {
      printf("-----+-----------+------+---------+--------+---------+----------+----------+----------+----------+-----\n");
      printf(" Buf |   Elapsed |    N |       T |     T1 |       P |       P1 |       P2 |        H | Deadtime |  DTC \n");
      printf("-----+-----------+------+---------+--------+---------+----------+----------+----------+----------+-----\n");
    }

    // Put data into our struct
    data[bufSec].elapsed = elapsed;

    // If HV is on, record counts
    if (getHVOn() == true) {
      data[bufSec].counts = counts;
      data[bufSec].deadTime = deadTime;
      data[bufSec].deadCounts = deadCounts;
    }
    // If HV is not on, record an impossible result
    else {
      data[bufSec].counts = -1;
    }

    // Read the raw T and P values from the altimeter
    data[bufSec].T = readTUncompensated();
    data[bufSec].P = readPUncompensated();

    // Do some calculations with T and P
    data[bufSec].T1 = calcFirstOrderT(data[bufSec].T);
    data[bufSec].P1 = calcFirstOrderP(data[bufSec].T, data[bufSec].P);
    data[bufSec].P2 = calcSecondOrderP(data[bufSec].T, data[bufSec].P);
    data[bufSec].altitude = calcAltitude(data[bufSec].P2, data[bufSec].T1);

    // If HV is not on
    if (getHVOn() == false) {
      // If we're above our threshold altitude, turn HV on
      if (data[bufSec].altitude > geigerAlt) {

        fprintf(csvf, "Elapsed, Counts, T (Raw), T1 (C), P (Raw), P1 (mbar), P2 (mbar), Altitude (m), Dead Time (s), Dead Time Counts\n");

        HVOn();                    // Turn the Geiger tube on
        fprintf(errf, "%s HVOn(), altitude = %f\n", getTimeStamp(), data[bufSec].altitude);
        DEBUG2_PRINT("%s HVOn(), altitude = %f\n", getTimeStamp(), data[bufSec].altitude);

        doPost = false;
      }
      // If we're below our threshold altitude and we haven't done a POST, do a POST
      else if ((doPost) && (data[bufSec].altitude < (geigerAlt - deadBand))) {
        fprintf(errf, "%s entering POST(), altitude = %f\n", getTimeStamp(), data[bufSec].altitude);
        DEBUG2_PRINT("%s entering POST(), altitude = %f\n", getTimeStamp(), data[bufSec].altitude);

        // Start the POST thread
        //pthread_create(&post_id, &attr, post, NULL);
        HVOn();
        for (int i = 0; i < 30; i++) {
          if (keepRunning) {
            waitNextSec();
          }
        }
        HVOff();
        start_time = getTimeMS();     // Save the start time
        curSec = 0;                   // Start at zero seconds
        geigerReset();                // Reset the Geiger circuit
        doPost = false;
        fprintf(errf, "%s exiting POST()\n", getTimeStamp());
        DEBUG2_PRINT("%s exiting POST()\n", getTimeStamp());
      }
    }

    // If HV is on and we're below our threshold altitude, turn HV off
    else if ((getHVOn() == true) && (data[bufSec].altitude < (geigerAlt - deadBand))) {
      HVOff();                   // Turn the Geiger tube off
      fprintf(errf, "%s HVOff()\n", getTimeStamp());
      DEBUG2_PRINT("%s HVOff()\n", getTimeStamp());
    }

    // Every so often, write to file
    if (bufSec == (buffer_seconds - 1)) {
      for (int i = 0; i < buffer_seconds; i++) {
        fprintf(csvf, "%lf, %d, %ld, %lf, %ld, %lf, %lf, %f, %lf, %d\n", data[i].elapsed, data[i].counts, data[i].T, data[i].T1, data[i].P, data[i].P1, data[i].P2, data[i].altitude, data[i].deadTime, data[i].deadCounts);
      }
    }

    // Every so often, let the log file know we're alive
    if ((curSec % 60 == 0) && (curSec != 0)) {
      fprintf(errf, "%s main() 60 seconds\n", getTimeStamp());
      DEBUG2_PRINT("%s main() 60 seconds\n", getTimeStamp());
    }

    // Write some output to the screen
    printf("  %2d | %9.3lf | %4d | %7ld | %6.2lf | %7ld | %8.3lf | %8.3lf | %8.2f | %1.6lf | %4d\n", getSecNum(), data[bufSec].elapsed, data[bufSec].counts, data[bufSec].T, data[bufSec].T1, data[bufSec].P, data[bufSec].P1, data[bufSec].P2, data[bufSec].altitude, deadTime, deadCounts);

    waitNextSec();              // Sleep until next second
  }

  // Turn the Geiger tube off
  if (getHVOn() == true) {
    HVOff();
    fprintf(errf, "%s HVOff()\n", getTimeStamp());
    DEBUG2_PRINT("%s HVOff()\n", getTimeStamp());
  }

  fprintf(errf, "%s exiting main()\n", getTimeStamp());
  DEBUG2_PRINT("%s exiting main()\n", getTimeStamp());

  // Stop the Geiger circuit
  geigerStop();
  fprintf(errf, "%s geigerStop()\n", getTimeStamp());
  DEBUG2_PRINT("%s geigerStop()\n", getTimeStamp());

  // Close the output file
  fclose(csvf);
  fprintf(errf, "%s Closed output file.\n", getTimeStamp());

  // Close the log file
  fprintf(errf, "%s Closing log file.\n", getTimeStamp());
  fprintf(errf, "%s ****************************************\n", getTimeStamp());
  result = fclose(errf);

  if (result != 0) {
    printf("Error closing log file!\n");
  }

  return EXIT_SUCCESS;
}
