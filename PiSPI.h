#ifndef GEIGERSPI_H
#define GEIGERSPI_H

void SPISetDelay(unsigned short delay);
unsigned short SPIGetDelay(void);
void SPISetBPW(unsigned char bpw);
int SPIGetFd(int channel);
int SPIDataRW(int channel, unsigned char *data, int len);
int SPISetup(int channel, int speed, int mode);

#endif
