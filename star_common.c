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
#include <time.h>
#include <inttypes.h>


unsigned long getTimeMS(void) {
  clock_gettime(CLOCK_REALTIME, &tim);

  /* seconds, converted to ms */
  unsigned long ms = tim.tv_sec * 1000;

  /* Add full ms */
  ms += tim.tv_nsec / 1000000;

  /* round up if necessary */
  if (tim.tv_nsec % 1000000 >= 500000) {
      ++ms;
  }
  
  return ms;
}

/*
 * waitNextNanoSec: Attempts to wait until the next whole interval, accurate to
 *                  maybe 10-20ms.  The maximum interval is 1s.
 *
 *                  interval is specified in nanoseconds
 *****************************************************************************
 */
void waitNextNanoSec(long interval) {
  struct timespec tim, tim2, rem;

  // Get the current time
  clock_gettime(CLOCK_MONOTONIC, &tim);

  // Calculate how long to wait until the next interval
  interval -= tim.tv_nsec;
  interval &= 999999999;

  tim2.tv_sec = 0;          // zero seconds
  tim2.tv_nsec = interval;  // some amount of nanoseconds

  // nanosleep() can be interrupted by the system
  // If it is interrupted, it returns -1 and places
  // the remaining time into rem
  while (nanosleep(&tim2, &rem) < 0) {
    &tim2 = &rem;           // If any time remaining, wait some more
  }
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