/*
 *****************************************************************************
 * geiger.h:  interfaces a Raspberry Pi with the custom Geiger circuit 
 *            inside STAR.
 *
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

#ifndef GEIGER_H
#define GEIGER_H

// LED routines
void LEDOn (void);
void LEDOff (void);
void *blinkLED (void *vargp);

// Count routines
void waitOneSec(void);
int getSecNum(void);
//int getMinNum(void);
//int getHourNum(void);
int getIndex(int numIndex);
int sumCounts(int numSecs);
float averageCounts(int numSecs);
float cpmTouSv(int numSecs);
void countInterrupt (void);

// HV routines
void HVOn(void);
void HVOff(void);
bool getHVOn(void);

// Setup routines
void geigerSetTime(unsigned long seconds);
int geigerReset(void);
int geigerSetup(void);
void geigerStart(void);
void geigerStop(void);

#endif
