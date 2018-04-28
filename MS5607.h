#ifndef MS5607_H
#define MS5607_H

int altimeterSetup(void);
int altimeterInit(void);
void altimeterReset(void);
unsigned int altimeterCalibration(char coeffNum);
unsigned long altimeterADC(char cmd);
unsigned char crc4(unsigned int n_prom[]);

unsigned long readPUncompensated(void);
unsigned long readTUncompensated(void);

double calcDT(unsigned long T);
double calcOffset(unsigned long T);
double calcSens(unsigned long T);

double firstOrderP(unsigned long T, unsigned long P);
double secondOrderP(unsigned long T, unsigned long P);
double firstOrderT(unsigned long T);

float CtoF(double temp);
float mbartoInHg(double pressure);
double PtoAlt(double pressure, double temp);

void setQFF(float latitude, float elevation, float height);
float getQFF();

#endif
