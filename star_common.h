#ifndef STAR_COMMON_H
#define STAR_COMMON_H

unsigned long getTimeMS(void);
char[] getTimeStamp(void);
void waitNextNanoSec(long interval);
double roundPrecision(double val, int precision);
float cvtCtoF(double temp);
float cvtMbtoIn(double pressure);

#endif
