/* Derivative work of wiringPiSPI */

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

void SPISetDelay(unsigned short delay) {
  spiDelay = delay;
}

unsigned int SPIGetDelay(void) {
  return spiDelay;
}

void SPISetBPW(unsigned char bpw) {
  spiBPW = bpw;
}

int SPIGetFd(int channel) {
  return spiFds[channel & 1];
}

int SPIDataRW(int channel, unsigned char *data, int len) {
  struct spi_ioc_transfer spi;
  channel &= 1;

  memset(&spi, 0, sizeof(spi));

  spi.tx_buf        = (unsigned long)data;
  spi.rx_buf        = (unsigned long)data;
  spi.len           = len;
  spi.delay_usecs   = spiDelay;
  spi.speed_hz      = spiSpeeds[channel];
  spi.bits_per_word = spiBPW;

  return ioctl(spiFds[channel], SPI_IOC_MESSAGE(1), &spi);
}

int SPISetup(int channel, int speed, int mode) {
  int fd;
  mode &= 3;     // Mode is 0, 1, 2, or 3
  channel &= 1;  // Channel is 0 or 1

  if ((fd = open(channel == 0 ? spiDev0 : spiDev1, O_RDWR)) < 0)
    return -1;

  spiSpeeds[channel] = speed;
  spiFds[channel] = fd;

  if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0)
    return -1;

  if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0)
    return -1;

  if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
     return -1;

  return fd;
}
