#include <stdio.h>
#include <wiringPiSPI.h>

/* Definitions to support MS5607 altimeter */
#define F_CPU 4000000UL     // 4MHz XTAL
#define CMD_RESET 0x1E      // ADC reset command
#define CMD_ADC_READ 0x00   // ADC read command
#define CMD_ADC_CONV 0x40   // ADC conversion command
#define CMD_ADC_D1 0x00     // ADC D1 conversion
#define CMD_ADC_D2 0x10     // ADC D2 conversion
#define CMD_ADC_256 0x00    // ADC OSR=256
#define CMD_ADC_512 0x02    // ADC OSR=512
#define CMD_ADC_1024 0x04   // ADC OSR=1024
#define CMD_ADC_2048 0x06   // ADC OSR=2056
#define CMD_ADC_4096 0x08   // ADC OSR=4096
#define CMD_PROM_RD 0xA0    // Prom read command

static const int CHANNEL = 0;     // SPI channel

/*
 * altimeterReset(): Initialize the altimeter
 *                   Returns the Linux file-descriptor for the device
 *                   Returns -1 if there is an error
 *****************************************************************************
 */

int altimeterInit(void) {
  unsigned int ret;
  ret = wiringPiSPISetup(CHANNEL, F_CPU);
  return ret;
}

/*
 * altimeterReset(): Reset the altimeter
 *****************************************************************************
 */

void altimeterReset(void) {
  unsigned char buffer[1];
  buffer[0] = CMD_RESET;
  wiringPiSPIDataRW(CHANNEL, buffer, 1);
}

unsigned int altimeterCalibration(char coeffNum) {
  unsigned char buffer[256];
  unsigned int ret;
  unsigned int rC = 0;

  buffer[0] = CMD_PROM_RD + (coeffNum * 2); // Send PROM READ command
  printf("cmd: %d\n", (int)buffer[0]);
  wiringPiSPIDataRW(CHANNEL, buffer, 1);
  buffer[0] = 0x00;
  wiringPiSPIDataRW(CHANNEL, buffer, 1);  // Send 0 to read the MSB

  for (int i=0; i<256; i++) {
    printf("%c", buffer[i]);
  }
  printf("\n");

  ret = buffer[0];
  rC = 256 * ret;
  printf("ret: %d, rC: %d\n", ret, rC);

  buffer[0] = 0x00;
  wiringPiSPIDataRW(CHANNEL, buffer, 1);  // Send 0 to read the LSB

  ret = (int)buffer[0];
  rC = rC + ret;

  return rC;
}

unsigned long altimeterADC(char cmd) {
  unsigned long ret = 0;
  return ret;
}

