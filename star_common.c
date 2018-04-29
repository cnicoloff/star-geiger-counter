/*
 *****************************************************************************
 * Helper functions for STAR
 *
 * Copyright (c) 2018 by Catherine Nicoloff, GNU GPL-3.0-or-later
 *****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


/*
 * waitNextNanoSec: Attempts to wait until the next whole interval, accurate to
 *                  maybe 1-2ms.  The maximum interval is 1s.
 *
 *                  interval is specified in nanoseconds
 *****************************************************************************
 */
void waitNextNanoSec(long interval) {
  struct timespec tim;
  struct timespec tim2;

  // Get the current time
  clock_gettime(CLOCK_REALTIME, &tim);

  // Calculate how long to wait until the next interval
  interval -= tim.tv_nsec;
  interval &= 999999999;

  tim2.tv_sec = 0;        // zero seconds
  tim2.tv_nsec = interval;  // some amount of nanoseconds
  nanosleep(&tim2, NULL); // wait
}

/*
 * roundPrecision: Round a value to a certain number of digits after the
 *                 decimal point
 *****************************************************************************
 */
 
double roundPrecision(double val, int precision) {
  long p10 = pow(10, precision);
  double valR;

  valR = val * p10;
  valR = ceil(valR);
  valR /= p10;

  return valR;
}

/*
 * cvtCtoF: Convert Celsius to Fahrenheit
 *****************************************************************************
 */

float cvtCtoF(double temp) {
  return temp * 9.0/5.0 + 32;
}

/*
 * cvtMbtoIn: Convert pressure from mbar to in hg
 *****************************************************************************
 */

float cvtMbtoIn(double pressure) {
  return pressure * 0.02953;
}