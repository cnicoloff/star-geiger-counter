#ifndef MS5607_H
#define MS5607_H

// Altimeter initialization
int altimeterSetup(void);
int altimeterInit(void);
void altimeterReset(void);

// Altimeter communications
unsigned int altimeterCalibration(char CNum);
unsigned long altimeterADC(char cmd);
unsigned char altimeterCRC4(unsigned int n_prom[]);

// Read raw values from the altimeter
unsigned long readPUncompensated(void);
unsigned long readTUncompensated(void);

// Use raw values to calculate T and P
double calcDT(unsigned long T);
double calcOffset(unsigned long T);
double calcSens(unsigned long T);
double calcFirstOrderP(unsigned long T, unsigned long P);
double calcSecondOrderP(unsigned long T, unsigned long P);
double calcFirstOrderT(unsigned long T);

// Use T and P to calculate altitude
double calcAltitude(double pressure, double temp);

// Set QFF to get an absolute altitude above mean sea level
void setQFF(float latitude, float elevation, float height);
float getQFF();

#endif
