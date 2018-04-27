#include <stdio.h>
#include <stdlib.h>
#include <wiringPiSPI.h>

/* Definitions to support MS5607 altimeter */
static const unsigned int F_CPU = 4000000; // 4MHz XTAL
static const char CMD_RESET = 0x1E;        // ADC reset command
static const char CMD_ADC_READ = 0x00;     // ADC read command
static const char CMD_ADC_CONV = 0x40;     // ADC conversion command
static const char CMD_ADC_D1 = 0x00;       // ADC D1 conversion
static const char CMD_ADC_D2 = 0x10;       // ADC D2 conversion
static const char CMD_ADC_256 = 0x00;      // ADC OSR=256
static const char CMD_ADC_512 = 0x02;      // ADC OSR=512
static const char CMD_ADC_1024 = 0x04;     // ADC OSR=1024
static const char CMD_ADC_2048 = 0x06;     // ADC OSR=2056
static const char CMD_ADC_4096 = 0x08;     // ADC OSR=4096
static const char CMD_PROM_RD = 0xA0;      // Prom read command

static const int CHANNEL = 0;              // SPI channel

/*
 * altimeterInit(): Initialize the altimeter
 *                  Returns the Linux file-descriptor for the device
 *                  Returns -1 if there is an error
 *****************************************************************************
 */

int altimeterInit(void) {
  unsigned int ret;
  ret = wiringPiSPISetupMode(CHANNEL, F_CPU, 3);
  return ret;
}

/*
 * altimeterReset(): Reset the altimeter
 *****************************************************************************
 */

void altimeterReset(void) {
  unsigned char buffer[1] = {0};

  buffer[0] = CMD_RESET;
  wiringPiSPIDataRW(CHANNEL, buffer, 1);
}

unsigned int altimeterCalibration(char coeffNum) {
  unsigned char buffer[1] = {0};
  unsigned int ret;
  unsigned int rC = 0;

  buffer[0] = CMD_PROM_RD + (coeffNum * 2); // Send PROM READ command
  wiringPiSPIDataRW(CHANNEL, buffer, 1);

  buffer[0] = CMD_ADC_READ;
  ret = wiringPiSPIDataRW(CHANNEL, buffer, 1);  // Send 0 to read the MSB

  rC = 256 * (int)buffer[0];

  buffer[0] = CMD_ADC_READ;
  ret = wiringPiSPIDataRW(CHANNEL, buffer, 1);  // Send 0 to read the LSB

  rC = rC + (int)buffer[0];

  return rC;
}

unsigned long altimeterADC(char cmd) {
  unsigned char buffer[1] = {0};
  unsigned int ret;
  unsigned long temp = 0;

  buffer[0] = CMD_ADC_CONV + cmd; // Send conversion command
  wiringPiSPIDataRW(CHANNEL, buffer, 1);

  buffer[0] = CMD_ADC_READ; // Send ADC read command
  wiringPiSPIDataRW(CHANNEL, buffer, 1);

  buffer[0] = CMD_ADC_READ; // Send again to read first byte
  wiringPiSPIDataRW(CHANNEL, buffer, 1);

  ret = (int)buffer[0];
  temp = 65536 * ret;

  buffer[0] = CMD_ADC_READ; // Send again to read second byte
  wiringPiSPIDataRW(CHANNEL, buffer, 1);

  ret = (int)buffer[0];
  temp = temp + 256 * ret;

  buffer[0] = CMD_ADC_READ; // Send again to read third byte
  wiringPiSPIDataRW(CHANNEL, buffer, 1);

  ret = (int)buffer[0];
  temp = temp + ret;

  return temp;
}

unsigned char crc4(unsigned int n_prom[]) {
  
}
