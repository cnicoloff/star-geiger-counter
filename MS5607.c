#include <stdio.h>
#include <stdlib.h>
#include <wiringPiSPI.h>

/* Code adapted from:
 * https://www.parallax.com/sites/default/files/downloads/29124-APPNote_520_C_code.pdf
 *
 * Copyright (c) 2009 MEAS Switzerland
 * Adapted for Raspberry Pi and wiringPi libraries by Catherine Nicoloff, April 2018
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
  int cnt;                // simple counter
  unsigned int n_rem;     // crc reminder
  unsigned int crc_read;  // original value of the crc
  unsigned char n_bit;
 
  n_rem = 0x00;
  crc_read = n_prom[7];   //save read CRC
  n_prom[7] = (0xFF00 & (n_prom[7])); //CRC byte is replaced by 0
  for (cnt = 0; cnt < 16; cnt++) {    // operation is performed on bytes
    // choose LSB or MSB
    if (cnt%2==1) n_rem ^= (unsigned short) ((n_prom[cnt>>1]) & 0x00FF);
    else n_rem ^= (unsigned short) (n_prom[cnt>>1]>>8);
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

double firstOrderP(void) {
  double P; // compensated pressure value
  double dT; // difference between actual and measured temperature
  double OFF; // offset at actual temperature
  double SENS; // sensitivity at actual temperature 
  
  unsigned long D1 = readPUncompensated()
  unsigned long D2 = readTUncompensated();
  
  // calculate 1st order pressure (MS5607 1st order algorithm)
  dT=D2-C[5]*pow(2,8);
  OFF=C[2]*pow(2,17)+dT*C[4]/pow(2,6);
  SENS=C[1]*pow(2,16)+dT*C[3]/pow(2,7);

  P=(((D1*SENS)/pow(2,21)-OFF)/pow(2,15))/100;
  
  return P;
}

double firstOrderT(void) {
  double T; // compensated temperature value
  double dT; // difference between actual and measured temperature
  double OFF; // offset at actual temperature
  double SENS; // sensitivity at actual temperature 
  
  unsigned long D2 = readTUncompensated();

  // calculate 1st order temperature (MS5607 1st order algorithm)
  dT=D2-C[5]*pow(2,8);
  OFF=C[2]*pow(2,17)+dT*C[4]/pow(2,6);
  SENS=C[1]*pow(2,16)+dT*C[3]/pow(2,7);

  T=(2000+(dT*C[6])/pow(2,23))/100;
  
  return T;
}