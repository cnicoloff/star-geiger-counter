#ifndef MS5607_H
#define MS5607_H

int altimeterInit(void);
void altimeterReset(void);
unsigned int altimeterCalibration(char coeffNum);
unsigned long altimeterADC(char cmd);

#endif
