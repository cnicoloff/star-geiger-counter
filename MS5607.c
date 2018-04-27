#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <linux/spi/spidev.h>
#include "PiSPI.h"

/* Code adapted from:
 * https://www.parallax.com/sites/default/files/downloads/29124-APPNote_520_C_code.pdf
 * Copyright (c) 2009 MEAS Switzerland
 *
 * (and wiringPi)
 *
 * Adapted for Raspberry Pi and spidev libraries by Catherine Nicoloff, April 2018
 */

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
  ret = SPISetup(CHANNEL, F_CPU, 3);
  return ret;
}

/*
 * altimeterReset(): Reset the altimeter
 *****************************************************************************
 */

void altimeterReset(void) {
  unsigned char buffer[1] = {0};

  SPISetDelay(3000);
  buffer[0] = CMD_RESET;
  SPIDataRW(CHANNEL, buffer, 1);
}

unsigned int altimeterCalibration(char coeffNum) {
  unsigned char buffer[5] = {0};
  unsigned int ret;
  unsigned int rC = 0;

  SPISetDelay(0);

  buffer[0] = CMD_PROM_RD + (coeffNum * 2); // Send PROM READ command
  buffer[1] = CMD_ADC_READ;
  buffer[2] = CMD_ADC_READ;
  // FIXME: Do something with this return value
  ret = SPIDataRW(CHANNEL, buffer, 3);

  rC = 256 * (int)buffer[1];
  rC = rC + (int)buffer[2];

  return rC;
}

unsigned long altimeterADC(char cmd) {
  unsigned char buffer[5] = {0};
  unsigned char sw = (cmd & 0x0F);
  unsigned short delay = SPIGetDelay();
  unsigned int ret;
  unsigned long temp = 0;

  if (sw == CMD_ADC_256)
    SPISetDelay(900);
  else if (sw == CMD_ADC_512)
    SPISetDelay(3000);
  else if (sw == CMD_ADC_1024)
    SPISetDelay(4000);
  else if (sw == CMD_ADC_2048)
    SPISetDelay(6000);
  else if (sw == CMD_ADC_4096)
    SPISetDelay(10000);
  else
    SPISetDelay(1000);

  buffer[0] = CMD_ADC_CONV + cmd; // Send conversion command
  ret = SPIDataRW(CHANNEL, buffer, 1);

  SPISetDelay(delay);

  buffer[0] = CMD_ADC_READ; // Send ADC read command
  buffer[1] = CMD_ADC_READ; // Send again to read first byte
  buffer[2] = CMD_ADC_READ; // Send again to read second byte
  buffer[3] = CMD_ADC_READ; // Send again to read third byte
  // FIXME: Do something with this value
  ret = SPIDataRW(CHANNEL, buffer, 4);

  temp = 65536 * (int)buffer[1];
  temp = temp + 256 * (int)buffer[2];
  temp = temp + (int)buffer[3];

  return temp;
}

unsigned char crc4(unsigned int n_prom[]) {
  int cnt;                // simple counter
  unsigned int n_rem;     // crc reminder
  unsigned int crc_read;  // original value of the crc
  unsigned char n_bit;

  n_rem = 0x00;
  crc_read = n_prom[7];               // save read CRC
  n_prom[7] = (0xFF00 & (n_prom[7])); // CRC byte is replaced by 0

  // operation is performed on bytes
  for (cnt = 0; cnt < 16; cnt++) {
    // choose LSB or MSB
    if (cnt % 2 == 1) {
       n_rem ^= (unsigned short)((n_prom[cnt >> 1]) & 0x00FF);
    }
    else {
       n_rem ^= (unsigned short)(n_prom[cnt >> 1] >> 8);
    }
    for (n_bit = 8; n_bit > 0; n_bit--) {
      if (n_rem & (0x8000)) {
        n_rem = (n_rem << 1) ^ 0x3000;
      }
      else {
        n_rem = (n_rem << 1);
      }
    }
  }

  n_rem = (0x000F & (n_rem >> 12)); // final 4-bit reminder is CRC code
  n_prom[7] = crc_read;             // restore the crc_read to its original place

  return (n_rem ^ 0x00);
}

unsigned long readPUncompensated(void) {
  return altimeterADC(CMD_ADC_D1 + CMD_ADC_256);
}

unsigned long readTUncompensated(void) {
  return altimeterADC(CMD_ADC_D2 + CMD_ADC_4096);
}

double firstOrderP(unsigned int coeffs[]) {
  double P = 0.0; // compensated temperature value
  double dT;      // difference between actual and measured temperature
  double offset;  // offset at actual temperature
  double sens;    // sensitivity at actual temperature

  unsigned long pRaw = readPUncompensated();
  unsigned long tRaw = readTUncompensated();

  dT = tRaw - coeffs[5] * pow(2,8);
  offset= coeffs[2] * pow(2,17) + dT * coeffs[4] / pow(2,6);
  sens = coeffs[1] * pow(2,16) + dT * coeffs[3] / pow(2,7);

  // calculate 1st order pressure (MS5607 1st order algorithm)
  P = (((pRaw * sens) / pow(2,21) - offset) / pow(2,15)) / 100;

  return P;
}

double firstOrderT(unsigned int coeffs[]) {
  double T = 0.0; // compensated temperature value
  double dT;      // difference between actual and measured temperature
  //double offset;  // offset at actual temperature
  //double sens;    // sensitivity at actual temperature

  unsigned long tRaw = readTUncompensated();
  //printf("Raw T: %e\n", tRaw);

  dT = tRaw - coeffs[5] * pow(2,8);
  //offset= coeffs[2] * pow(2,17) + dT * coeffs[4] / pow(2,6);
  //sens = coeffs[1] * pow(2,16) + dT * coeffs[3] / pow(2,7);

  // calculate 1st order temperature (MS5607 1st order algorithm)
  T = (2000 + (dT * coeffs[6]) / pow(2,23)) / 100;

  return T;
}

double secondOrderP(unsigned int coeffs[]) {
  double P = 0.0;                     // compensated pressure value
  double temp = firstOrderT(coeffs);  // first order temperature value
  double temp2 = 0.0;                 // ??
  double dT;                          // difference between actual and measured temperature
  double offset, offset2 = 0.0;       // offset at actual temperature
  double sens, sens2 = 0.0;           // sensitivity at actual temperature

  unsigned long pRaw = readPUncompensated();
  unsigned long tRaw = readTUncompensated();

  dT = tRaw - coeffs[5] * pow(2,8);

  // Temperature less than 20 C
  if (temp < 20) {
    temp2 = pow(dT,2) / pow(2,31);
    offset2 = 61 * pow((temp - 2000),2) / pow(2,4);
    sens2 = 2 * pow((temp - 2000),2);
    // Temperature less than -15 C
    if (temp < -15) {
      offset2 = offset2 + 15 * pow((temp + 1500),2);
      sens2 = sens2 + 8 * pow((temp + 1500), 2);
    }
  }
  // Temperature greater than 20 C
  else {
    temp2 = 0;
    offset2 = 0;
    sens2 = 0;
  }

  offset= coeffs[2] * pow(2,17) + dT * coeffs[4] / pow(2,6);
  sens = coeffs[1] * pow(2,16) + dT * coeffs[3] / pow(2,7);

  temp = temp - temp2;
  offset = offset - offset2;
  sens = sens - sens2;

  //offset= coeffs[2] * pow(2,17) + dT * coeffs[4] / pow(2,6);
  //sens = coeffs[1] * pow(2,16) + dT * coeffs[3] / pow(2,7);

  // calculate 2nd order pressure (MS5607 2nd order non-linear algorithm)
  P = (((pRaw * sens) / pow(2,21) - offset) / pow(2,15)) / 100;

  return P;
}

