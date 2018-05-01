/*
 *****************************************************************************
 * PiSPI.h:  Communications library for SPI devices on the Raspberry Pi.
 *
 * This is a derivative work of wiringPi/wiringPiSPI, which can be found at:
 * http://wiringpi.com/ 
 *
 * wiringPi is protected by the GNU LGPLv3 license.
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
 
#ifndef GEIGERSPI_H
#define GEIGERSPI_H

void SPISetDelay(unsigned short delay);
unsigned short SPIGetDelay(void);
void SPISetBPW(unsigned char bpw);
int SPIGetFd(int channel);
int SPIDataRW(int channel, unsigned char *data, int len);
int SPISetup(int channel, int speed, int mode);

#endif
