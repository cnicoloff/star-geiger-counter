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

unsigned long long getTimeMS(void) {
  struct timespec tim;

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
 * getTimeStamp: Gets the current timestamp [HH:MM:SS.ms]
 *****************************************************************************
 */

const char * getTimeStamp(void) {
  double ms = getTimeMS() / 1000000.0;
  unsigned int h, m;
  unsigned long s = ms;
  static char ts[12] = {0};

  // Calculate hours
  h = s / 3600;
  s %= 3600;
  // Calculate minutes
  m = s / 60;
  s %= 60;

  sprintf(ts, "[%2d:%2d:%2ld.%.3f]", h, m, s, ms);

  return ts;
}

/*
 * getDateTimeStamp: Gets the current timestamp YYYY-MM-DD.HH.MM.SS
 *****************************************************************************
 */

const char * getDateTimeStamp(void) {
  static char ts[20] = {0};
  struct tm tstruct;

  // Get the current time (UTC)
  time_t now = time(0);
  // Convert to the local time
  tstruct = *localtime(&now);

  strftime(ts, sizeof(ts), "%Y-%m-%d_%H-%M-%S", &tstruct);

  return ts;
}

/*
 * waitNanoSec: Waits for a specified number of nanoseconds.
 *              The maximum interval is 999999999 ns.
 *****************************************************************************
 */

void waitNanoSec(long interval) {
  struct timespec tim, tim2, rem;

  // Get the current time
  clock_gettime(CLOCK_MONOTONIC, &tim);

  // Calculate how long to wait until the next interval
  interval = interval - tim.tv_nsec;

  tim2.tv_sec = interval / 1000000000;    // seconds
  tim2.tv_nsec = interval % 1000000000;   // nanoseconds

  // nanosleep() can be interrupted by the system
  // If it is interrupted, it returns -1 and places
  // the remaining time into rem
  while (nanosleep(&tim2, &rem) < 0) {
    tim2 = rem;             // If any time remaining, wait some more
  }
}

/*
 * waitNextSec: Attempts to wait until the next whole second.
 *****************************************************************************
 */

void waitNextSec(void) {
  struct timespec tim, tim2, rem;

  // Get the current time
  clock_gettime(CLOCK_MONOTONIC, &tim);

  tim2.tv_nsec = 999999999 - tim.tv_nsec; // nanoseconds
  tim2.tv_sec = 0;                        // seconds

  nanosleep(&tim2, &rem);
}

/*
 * roundPrecision: Round a value to a certain number of digits after the
 *                 decimal point
 *****************************************************************************
 */

double roundPrecision(double val, int precision) {
  long p10 = pow(10, precision);
  double valR;

  // Multiply by 10^precision
  valR = val * p10;
  // Round up
  valR = ceil(valR);
  // Divide by 10^precision
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
