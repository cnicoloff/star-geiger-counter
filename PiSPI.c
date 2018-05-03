/*
 *****************************************************************************
 * PiSPI.c:  Communications library for SPI devices on the Raspberry Pi.
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

#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <linux/spi/spidev.h>


static const char *spiDev0 = "/dev/spidev0.0";  // SPI device 0
static const char *spiDev1 = "/dev/spidev0.1";  // SPI device 1
static volatile unsigned char spiBPW = 8;       // SPI bits per word
static volatile unsigned short spiDelay = 1000; // SPI delay, microseconds
static unsigned int spiSpeeds[2];               // SPI bus speeds
static int spiFds[2];                           // SPI file descriptors

/*
 * SPISetDelay: Sets the delay to a specific number of microseconds
 *****************************************************************************
 */
void SPISetDelay(unsigned short delay) {
  spiDelay = delay;
}

/*
 * SPIGetDelay: Gets the current value of spiDelay in microseconds
 *****************************************************************************
 */
unsigned short SPIGetDelay(void) {
  return spiDelay;
}

/*
 * SPISetBPW: Sets the number of bits per word
 *****************************************************************************
 */
void SPISetBPW(unsigned char bpw) {
  spiBPW = (bpw & 8);
}

/*
 * SPIGetFd: Gets the file descriptor for the given channel (0 or 1)
 *****************************************************************************
 */
int SPIGetFd(int channel) {
  return spiFds[channel & 1];
}

/*
 * SPIDataRW: Sends SPI data to a given channel
 *            channel can be 0 or 1
 *            data is an array of char
 *            len is how many elements of the array to send (0..len-1)
 *****************************************************************************
 */
int SPIDataRW(int channel, unsigned char *data, int len) {
  struct spi_ioc_transfer spi;  // Create an SPI struct
  channel &= 1;                 // Enforce channel 0 or 1

  memset(&spi, 0, sizeof(spi)); // Allocate memory for the struct

  spi.tx_buf        = (unsigned long)data; // TX buffer
  spi.rx_buf        = (unsigned long)data; // RX buffer is the same as TX 
  spi.len           = len;                 // Number of chars to send
  spi.delay_usecs   = spiDelay;            // Delay between read/write
  spi.speed_hz      = spiSpeeds[channel];  // Bus speed of device
  spi.bits_per_word = spiBPW;              // Bits per word

  // Send the data to the given file descriptor
  return ioctl(spiFds[channel], SPI_IOC_MESSAGE(1), &spi);
}

/*
 * SPISetup: Sets up a given SPI device
 *           channel can be 0 or 1
 *           speed is the bus speed of the device
 *           mode can be 0, 1, 2, or 3
 *****************************************************************************
 */
int SPISetup(int channel, int speed, int mode) {
  int fd;        // file descriptor
  mode &= 3;     // mode is 0, 1, 2, or 3
  channel &= 1;  // channel is 0 or 1

  // If we can't open the SPI channel read-write, exit
  if ((fd = open(channel == 0 ? spiDev0 : spiDev1, O_RDWR)) < 0)
    return -1;

  // Save these parameters for the given channel
  spiSpeeds[channel] = speed;
  spiFds[channel] = fd;

  // If we can't set the SPI mode, exit
  if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0)
    return -2;

  // If we can't set the SPI bits per word, exit
  if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0)
    return -3;

  // If we can't set the SPI bus speed, exit
  if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
     return -4;

  // Success!  Return the file descriptor.
  return fd;
}
