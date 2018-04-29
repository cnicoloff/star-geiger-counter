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