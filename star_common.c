/*
 *****************************************************************************
 * star_common.c:   helper functions for STAR
 *
 * Copyright 2018 by Catherine Nicoloff, GNU GPL-3.0-or-later
 *****************************************************************************
 * This file is part of STAR.
 *
 * STAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * STAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with STAR.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>


/*
 * getTimeMS: Gets the current time in milliseconds
 *****************************************************************************
 */

unsigned long getTimeMS(void) {
  struct timespec tim;

  clock_gettime(CLOCK_MONOTONIC, &tim);

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
 * getTimeStamp: Gets the current timestamp [HH:MM:SS.ms]
 *****************************************************************************
 */

const char * getTimeStamp(void) {
  static char ts[12] = {0};
  unsigned long ms = getTimeMS();

  int h = ms / 3600000;
  ms -= h * 3600000;
  int m = ms / 60000;
  ms -= m * 60000;
  int s = ms / 1000;
  ms -= s * 1000;

  sprintf(ts, "[%02d:%02d:%02d.%03d]", h, m, s, ms);
  return ts;
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
  clock_gettime(CLOCK_REALTIME, &tim);

  // Calculate how long to wait until the next interval
  interval -= tim.tv_nsec;
  interval &= 999999999;

  tim2.tv_sec = 0;          // zero seconds
  tim2.tv_nsec = interval;  // some amount of nanoseconds

  // nanosleep() can be interrupted by the system
  // If it is interrupted, it returns -1 and places
  // the remaining time into rem
  while (nanosleep(&tim2, &rem) < 0) {
    tim2 = rem;             // If any time remaining, wait some more
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
