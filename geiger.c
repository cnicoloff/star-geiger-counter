
/*
 ***********************************************************************
 * geiger.c:  interfaces a Raspberry Pi Zero with the custom Geiger
 *                 circuit inside STAR.
 *
 * Catherine Nicoloff, April 2018
 ***********************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <wiringPi.h>


/* Initialize global variables */
static volatile int secNum;
static volatile int sec[60] = {0};
static volatile int minNum;
static volatile int min[60] = {0};
static volatile int globalCount;
static volatile int ledTime;
static volatile bool keepRunning;
static volatile bool hvOn;

/* Initialize the GPIO pins */
static int ledPin = 4;
static int geigerPin = 3;
static int gatePin = 6;

static int flashTime = 10;

/*
 * breakHandler: Captures CTRL-C so we can shut down cleanly.
 *********************************************************************************
 */

void breakHandler(int s) {

  /* Tell all loops and threads to exit */
  keepRunning = false;
}

/*
 * countInterrupt: Runs when a count is detected.
 *********************************************************************************
 */

void countInterrupt (void) {
  /* Increment the various counters */
  globalCount++;
  sec[secNum]++;
  min[minNum]++;

  /* Tell the LED to light up briefly */
  ledTime += flashTime;
}

/*
 * blinkLED: Thread to handle LED blinking.
 *********************************************************************************
 */

void *blinkLED (void *vargp) {

  /* Set up nanosleep() */
  struct timespec tim;
  tim.tv_sec = 0;
  tim.tv_nsec = flashTime * 1000000;

  while (keepRunning) {

    /* If the LED is supposed to be lit */
    if (ledTime > 0) {
      /* Turn on the LED */
      digitalWrite(ledPin, HIGH);
      /*  Subtract the time it will be lit */
      ledTime -= flashTime;
      /* Sleep for flashTime ms */
      nanosleep(&tim, NULL);
    }
    /* If the LED is not supposed to be lit */
    else {
      /* Turn off the LED */
      digitalWrite(ledPin, LOW);
    }
  }

  /* Turn off the LED before exiting */
  digitalWrite(ledPin, LOW);
  pthread_exit(NULL);
}


/*
 * getIndex: Get the counts for a particular index from the circular buffer.
 *********************************************************************************
 */

int getIndex(int numIndex) {

  /* We want the index that is 60 - numIndex from the end */
  if (numIndex < 0) {
    numIndex = 60 + numIndex;
  }
  else {
    numIndex = numIndex % 60;
  }

  return numIndex;
}

/*
 * sumCounts: Sum the number of counts across the last numSecs seconds.
 *********************************************************************************
 */

int sumCounts(int numMins, int numSecs) {
  int total = 0;

  /* Sum minutes */
  for (int i=0; i < numMins; i++) {
    total += min[getIndex(minNum - i)];
  }

  /* Sum seconds */
  for (int i=0; i < numSecs; i++) {
    total += sec[getIndex(secNum - i)];
  }

  return total;

}

/*
 * averageCounts: Average the number of counts across a specific number
 *                of seconds.
 *********************************************************************************
 */

float averageCounts(int numMins, int numSecs) {
  float average;

  average = (float)sumCounts(numMins, numSecs) / ((numMins * 60.0) + (float)numSecs);

  return average;
}

/*
 * cpmTouSv: Convert cpm (counts per minute) to microSieverts/hour
 *********************************************************************************
 */
float cpmTouSv(void) {

  /* Conversion factor for SBM-20 tube */
  float factor = 0.0057;

  float uSv;
  float cpm;

  /* Three minute moving average */
  cpm = averageCounts(3, 0) * 60.0;

  /* Multiply by conversion factor  */
  uSv = factor * cpm;

  return uSv;
}

/*
 * count: Thread to handle count_related activities.
 *********************************************************************************
 */

void *count (void *vargp) {

  while (keepRunning) {
    sleep(1);

    secNum++;

    if (secNum % 60 == 0) {

      minNum++;
      if (minNum % 60 == 0) {
        minNum = 0;
      }

      secNum = 0;
      min[minNum] = 0;
    }

    sec[secNum] = 0;

  }
  pthread_exit(NULL);
}

/*
 *********************************************************************************
 * main
 *********************************************************************************
 */

int main (void)
{

  /* Set up a signal handler for CTRL-C */
  struct sigaction act;
  act.sa_handler = breakHandler;
  sigaction(SIGINT, &act, NULL);

  /* Define the output file */
  FILE *opf;
  char opfname[] = "out.txt";

  /* Attempt to open our output file */
  opf = fopen(opfname, "w");

  /* If we failed to open the file, complain */
  if (opf == NULL) {
    fprintf(stderr, "Can't open output file!\n");
    exit(1);
  }

  /* Run forever unless halted */
  keepRunning = true;

  /* Turn HV On */
  hvOn = true;

  /* Set up the attribute to allow our threads to run detached */
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  /* Initialize wiringPi */
  wiringPiSetup();

  /* HV gate pin setup */
  pinMode(gatePin, OUTPUT);

  /* Visual count indicator LED */
  pinMode(ledPin, OUTPUT);

  /* Initialize the LED blink */
  ledTime = 0;

  /* Set up the LED blink thread */
  pthread_t led_id;
  pthread_create(&led_id, &attr, blinkLED, NULL);

  /* Configure wiringPi to detect pulses with a falling
   * edge on input pin 6 (GPIO pin 25) */
  wiringPiISR(geigerPin, INT_EDGE_FALLING, &countInterrupt);
  pullUpDnControl(geigerPin, PUD_OFF);

  /* Initialize counting variables */
  minNum = -1;
  secNum = 0;
  globalCount = 0;

  /* Initialize the counting arrays */
  for (int i=0; i < 60; i++) {
    sec[i] = min[i] = 0;
  }

  /* Set up the counting thread */
  pthread_t count_id;
  pthread_create(&count_id, &attr, count, NULL);

  if (hvOn) {
    /* Turn on HV */
    digitalWrite(gatePin, HIGH);
  }

 /* Loop forever or until CTRL-C */
  while (keepRunning) {
    /* Sleep for 1 s */
    sleep(1);

    /* Write some output */
    float temp2 = cpmTouSv();

    if (secNum % 30 == 0) {
      //printf("%0d:%0d Counter: %5d\n", minNum, secNum, sumCounts(10));
      printf("uSv/hr: %f\n", temp2);
    }

    /* If a minute has passed... */
    if (secNum % 60 == 0) {
    }
  }

  /* Close the output file */
  fclose(opf);
  /* Clean up */
  pthread_attr_destroy(&attr);
  /* Turn off HV */
  digitalWrite(gatePin, LOW);
  /* Turn off LED */
  digitalWrite(ledPin, LOW);

  return EXIT_SUCCESS;
}
