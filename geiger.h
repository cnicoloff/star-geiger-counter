#ifndef GEIGER_H
#define GEIGER_H

// LED routines
void LEDOn (void);
void LEDOff (void);
void *blinkLED (void *vargp);

// Count routines
void waitOneSec(void);
int getSecNum(void);
int getMinNum(void);
int getHourNum(void);
int getIndex(int numIndex);
int sumCounts(int numSecs);
float averageCounts(int numSecs);
float cpmTouSv(int numSecs);
void countInterrupt (void);
void *count (void *vargp);

// HV routines
void HVOn(void);
void HVOff(void);
void *HVControl (void *vargp);

// Setup routines
int geigerReset(void);
int geigerSetup(void);
void geigerStart(void);
void geigerStop(void);

#endif