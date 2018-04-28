#ifndef GEIGER_H
#define GEIGER_H

int geigerSetup(void);

void LEDOn (void);
void LEDOff (void);
void *blinkLED (void *vargp);

int getSecNum(void);
int getMinNum(void);
int getHourNum(void);
int getIndex(int numIndex);

int sumCounts(int numSecs);
float averageCounts(int numSecs);
float cpmTouSv(int numSecs);

void countInterrupt (void);
void *count (void *vargp);

void HVOn (void);
void HVOff (void);
void *HVControl (void *vargp);

#endif