#ifndef MS5607_H
#define MS5607_H

int altimeterSetup(void);
int altimeterInit(void);
void altimeterReset(void);

unsigned int altimeterCalibration(char CNum);
unsigned long altimeterADC(char cmd);
unsigned char altimeterCRC4(unsigned int n_prom[]);

double roundPrecision(double val, int precision);

unsigned long readPUncompensated(void);
unsigned long readTUncompensated(void);

double calcDT(unsigned long T);
double calcOffset(unsigned long T);
double calcSens(unsigned long T);

double calcFirstOrderP(unsigned long T, unsigned long P);
double calcSecondOrderP(unsigned long T, unsigned long P);
double calcFirstOrderT(unsigned long T);

float cvtCtoF(double temp);
float cvtMbartoInHg(double pressure);
double calcAltitude(double pressure, double temp);

void setQFF(float latitude, float elevation, float height);
float getQFF();

#endif
