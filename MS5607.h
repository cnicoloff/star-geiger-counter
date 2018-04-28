#ifndef MS5607_H
#define MS5607_H

int altimeterInit(void);
void altimeterReset(void);
unsigned int altimeterCalibration(char coeffNum);
unsigned long altimeterADC(char cmd);
unsigned char crc4(unsigned int n_prom[]);

unsigned long readPUncompensated(void);
unsigned long readTUncompensated(void);

double calcDT(unsigned int coeffs[]);
double calcOffset(unsigned int coeffs[]);
double calcSens(unsigned int coeffs[]);

double firstOrderP(unsigned int coeffs[]);
double secondOrderP(unsigned int coeffs[]);
double firstOrderT(unsigned int coeffs[]);

float CtoF(double temp);
float mbartoInHg(double pressure);
double PtoAlt(double pressure, double temp);

#endif
