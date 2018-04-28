/*
 *****************************************************************************
 * Code adapted from:
 * https://www.parallax.com/sites/default/files/downloads/29124-APPNote_520_C_code.pdf
 * Copyright (c) 2009 MEAS Switzerland
 *
 * Adapted for Raspberry Pi and spidev libraries.
 * Copyright (c) 2018 by Catherine Nicoloff, GNU GPL-3.0-or-later
 *****************************************************************************
 */

// FIXME: Sample temperature once and use it to make calculations, don't keep
// sampling both in each function!

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <linux/spi/spidev.h>
#include "PiSPI.h"

// Definitions to support MS5607 altimeter
static const unsigned int F_CPU = 4000000; // 4MHz XTAL
static const char CMD_RESET = 0x1E;        // ADC reset command
static const char CMD_ADC_READ = 0x00;     // ADC read command
static const char CMD_ADC_CONV = 0x40;     // ADC conversion command
static const char CMD_ADC_D1 = 0x00;       // ADC D1 conversion
static const char CMD_ADC_D2 = 0x10;       // ADC D2 conversion
static const char CMD_ADC_256 = 0x00;      // ADC OSR=256
static const char CMD_ADC_512 = 0x02;      // ADC OSR=512
static const char CMD_ADC_1024 = 0x04;     // ADC OSR=1024
static const char CMD_ADC_2048 = 0x06;     // ADC OSR=2048
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
  return SPISetup(CHANNEL, F_CPU, 3);  // Set up the SPI channel
}

/*
 * altimeterReset(): Reset the altimeter
 *****************************************************************************
 */

void altimeterReset(void) {
  unsigned char buffer[1] = {0};

  SPISetDelay(3000);              // Set a 3ms read/write delay
  buffer[0] = CMD_RESET;          // Put the reset command in the buffer
  SPIDataRW(CHANNEL, buffer, 1);  // Send the command
}

/*
 * altimeterCalibration(): Get the altimeter's factory calibration 
 *                         coefficient (0..5)
 *                         0: Pressure sensitivity
 *                         1: Pressure offset
 *                         2: Temperature coefficient of pressure sensitivity
 *                         3: Temperature coefficient of pressure offset
 *                         4: Reference temperature
 *                         5: Temperature coefficient of the temperature
 *
 *                         These values are 16 bit, which is why they are
 *                         acquired in two parts.
 *****************************************************************************
 */
unsigned int altimeterCalibration(char coeffNum) {
  unsigned char buffer[5] = {0};
  unsigned int ret;
  unsigned int rC = 0;

  coeffNum &= 7;   // Enforce 0..7
  SPISetDelay(0);  // No read/write delay

  buffer[0] = CMD_PROM_RD + (coeffNum * 2); // Send PROM READ command
  buffer[1] = CMD_ADC_READ;                 // Get next char
  buffer[2] = CMD_ADC_READ;                 // Get next char

  // FIXME: Do something with this return value
  ret = SPIDataRW(CHANNEL, buffer, 3);      // Send and receive

  rC = 256 * (int)buffer[1];                // Convert the high bits
  rC = rC + (int)buffer[2];                 // Add the low bits

  return rC;
}

/*
 * altimeterADC(): Query the altimeter's analog to digital converter
 *
 *                 These values are 32 bit, which is why they are acquired
 *                 in three parts.
 *****************************************************************************
 */
unsigned long altimeterADC(char cmd) {

  unsigned char buffer[5] = {0};            // Set up a buffer
  unsigned char delay = (cmd & 0x0F);       // Calculate how much delay we need
  unsigned short delayOld = SPIGetDelay();  // Save our old delay value
  unsigned int ret;
  unsigned long temp = 0;

  // Set an appropriate read/write delay
  if (delay == CMD_ADC_256)
    SPISetDelay(900);
  else if (delay == CMD_ADC_512)
    SPISetDelay(3000);
  else if (delay == CMD_ADC_1024)
    SPISetDelay(4000);
  else if (delay == CMD_ADC_2048)
    SPISetDelay(6000);
  else if (delay == CMD_ADC_4096)
    SPISetDelay(10000);
  else
    SPISetDelay(1000);

  buffer[0] = CMD_ADC_CONV + cmd;       // Send conversion command
  ret = SPIDataRW(CHANNEL, buffer, 1);  // Send and receive

  SPISetDelay(delay);                   // Set the delay

  buffer[0] = CMD_ADC_READ;             // Send ADC read command
  buffer[1] = CMD_ADC_READ;             // Send again to read first byte
  buffer[2] = CMD_ADC_READ;             // Send again to read second byte
  buffer[3] = CMD_ADC_READ;             // Send again to read third byte

  // FIXME: Do something with this value
  ret = SPIDataRW(CHANNEL, buffer, 4);  // Send and receive

  SPISetDelay(delayOld);                // Set the delay to previous value

  temp = 65536 * (int)buffer[1];        // Convert the high bits
  temp = temp + 256 * (int)buffer[2];   // Convert the middle bits and add them
  temp = temp + (int)buffer[3];         // Add the low bits

  return temp;
}

/*
 * crc4(): This is a CRC check
 *****************************************************************************
 */
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

/*
 * readPUncompensated: Read the raw pressure data from the altimeter
 *****************************************************************************
 */
unsigned long readPUncompensated(void) {
  return altimeterADC(CMD_ADC_D1 + CMD_ADC_4096);
  //return altimeterADC(CMD_ADC_D1 + CMD_ADC_256);
}

/*
 * readTUncompensated: Read the raw temperature data from the altimeter
 *****************************************************************************
 */
unsigned long readTUncompensated(void) {
  return altimeterADC(CMD_ADC_D2 + CMD_ADC_4096);
}

/*
 * calcDT: Calculate the difference between the actual and the reference
 *         temperature
 *****************************************************************************
 */
double calcDT(unsigned int coeffs[]) {
  return (readTUncompensated() - coeffs[5] * pow(2,8));
}

/*
 * calcOffset: Calculate the offset at the actual temperature
 *****************************************************************************
 */
double calcOffset(unsigned int coeffs[]) {
  double dT = calcDT(coeffs);
  return (coeffs[2] * pow(2,17)) + (dT * coeffs[4]) / pow(2,6);
}

/*
 * calcSens: Calculate the sensitivity at the actual temperature
 *****************************************************************************
 */
double calcSens(unsigned int coeffs[]) {
  double dT = calcDT(coeffs);
  return (coeffs[1] * pow(2,16)) + (dT * coeffs[3]) / pow(2,7);
}

/*
 * firstOrderP: Calculate the first order pressure using the MS5607 1st
 *              order algorithm.
 *****************************************************************************
 */
double firstOrderP(unsigned int coeffs[]) {
  double P = 0.0; // compensated temperature value
  double offset;  // offset at actual temperature
  double sens;    // sensitivity at actual temperature

  unsigned long pRaw = readPUncompensated();

  offset = calcOffset(coeffs);
  sens = calcSens(coeffs);

  // calculate 1st order pressure (MS5607 1st order algorithm)

  P = (((pRaw * sens) / pow(2,21) - offset) / pow(2,15)) / 100;

  return P;
}

/*
 * firstOrderT: Calculate the first order temperature using the MS5607 1st
 *              order algorithm.
 *****************************************************************************
 */
double firstOrderT(unsigned int coeffs[]) {
  double T = 0.0; // compensated temperature value
  double dT;      // difference between actual and measured temperature

  dT = calcDT(coeffs);

  // calculate 1st order temperature (MS5607 1st order algorithm)
  T = (2000 + (dT * coeffs[6]) / pow(2,23)) / 100;

  return T;
}

/*
 * secondOrderP: Calculate the second order pressure using the MS5607 2nd
 *               order non-linear algorithm.
 *****************************************************************************
 */
double secondOrderP(unsigned int coeffs[]) {
  double P = 0.0;                     // compensated pressure value
  double temp = firstOrderT(coeffs);  // first order temperature value
  double temp2 = 0.0;                 // ??
  double dT;                          // difference between actual and measured temperature
  double offset, offset2 = 0.0;       // offset at actual temperature
  double sens, sens2 = 0.0;           // sensitivity at actual temperature

  unsigned long pRaw = readPUncompensated();

  offset = calcOffset(coeffs);
  sens = calcSens(coeffs);
  dT = calcDT(coeffs);

  // Temperature less than 20 C
  if (temp < 20.0) {
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

  temp = temp - temp2;
  offset = offset - offset2;
  sens = sens - sens2;

  // calculate 2nd order pressure (MS5607 2nd order non-linear algorithm)
  P = (((pRaw * sens) / pow(2,21) - offset) / pow(2,15)) / 100;

  return P;
}

/*
 * tempCtoF: Convert C to F
 *****************************************************************************
 */
float CtoF(double temp) {
  return temp * 9.0/5.0 + 32;
}

/*
 * mbartoInHg: Convert pressure from mbar to in hg
 *****************************************************************************
 */
float mbartoInHg(double pressure) {
  return pressure * 0.02953;
}

/*
 * PtoAlt: Convert pressure to altitude
 *****************************************************************************
 */
double PtoAlt(double pressure, double temp) {
  float R = 287.057;     // gas constant of air at sea level
  float g = 9.807;       // acceleration due to gravity, m/s^2
  double Ps = 1008;      // pressure at sea level, mbar
  float Ts = 288.15;     // temperature at sea level, K

  return (R/g) * ((Ts + temp + 273.15) / 2.0) * log(Ps/pressure);

  //return (pow((Ps/Pf),(1.0/5.255)) - 1) * ((temp + 273.15) / 0.0065);
}
