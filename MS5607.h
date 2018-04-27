#ifndef MS5607_H
#define MS5607_H

int altimeterInit(void);
void altimeterReset(void);
unsigned int altimeterCalibration(char coeffNum);
unsigned long altimeterADC(char cmd);
unsigned char crc4(unsigned int n_prom[]);
unsigned long readPUncompensated(void) {
unsigned long readTUncompensated(void) {
double firstOrderP(void) {
double firstOrderT(void) {

#endif
