/*
 *****************************************************************************
 * MS5607.c:  interfaces a Raspberry Pi with the MS5607 altimeter module from
 *            Parallax.  https://www.parallax.com/product/29124
 *
 * Code derived from sample code at:
 * www.parallax.com/sites/default/files/downloads/29124-APPNote_520_C_code.pdf
 * Copyright 2009 MEAS Switzerland
 *
 * Adapted for Raspberry Pi and spidev libraries.
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
 
#ifndef MS5607_H
#define MS5607_H

// Altimeter initialization
int altimeterSetup(void);
int altimeterInit(void);
void altimeterReset(void);

// Altimeter communications
unsigned int altimeterCalibration(char CNum);
unsigned long altimeterADC(char cmd);
unsigned char altimeterCRC4(unsigned int n_prom[]);

// Read raw values from the altimeter
unsigned long readPUncompensated(void);
unsigned long readTUncompensated(void);

// Use raw values to calculate T and P
double calcDT(unsigned long T);
double calcOffset(unsigned long T);
double calcSens(unsigned long T);
double calcFirstOrderP(unsigned long T, unsigned long P);
double calcSecondOrderP(unsigned long T, unsigned long P);
double calcFirstOrderT(unsigned long T);

// Use T and P to calculate altitude
double calcAltitude(double pressure, double temp);

// Set QFF to get an absolute altitude above mean sea level
void setQFF(float latitude, float elevation, float height);
float getQFF();

#endif
