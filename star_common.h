/*
 *****************************************************************************
 * star_common.h:   helper functions for STAR
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

#ifndef STAR_COMMON_H
#define STAR_COMMON_H

unsigned long getTimeMS(void);
const char * getTimeStamp(void);
void waitNextNanoSec(long interval);
double roundPrecision(double val, int precision);
float cvtCtoF(double temp);
float cvtMbtoIn(double pressure);

#endif
