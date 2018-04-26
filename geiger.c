
/*
 *****************************************************************************
 * geiger.c:  interfaces a Raspberry Pi Zero with the custom Geiger
 *            circuit inside STAR.
 *
 * Catherine Nicoloff, April 2018
 *****************************************************************************
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
#include <wiringPiSPI.h>

#define TRUE 1
#define FALSE 0

#define F_CPU 4000000UL  // 4MHz XTAL
#define CMD_RESET 0x1E   // ADC reset command
#define 


/* Initialize global variables */
static int size = 60;             // Array size
static volatile int secNum;       // Which index in the seconds array
static volatile int sec[60]={0};  // Array of collected counts per second
static volatile int minNum;       // Which index of the minutes array
static volatile int min[60]={0};  // Array of collected counts per minute
static volatile int hourNum;      // Which index of the hours array
static volatile int hour[60]={0}; // Array of collected counts per hour
static volatile int elapsed;      // How many seconds have elapsed

static volatile int ledTime;      // How much time is left to light LED
static volatile bool keepRunning; // Signals when to exit
static volatile bool turnHVOn;    // Signals when to turn the HV on
static volatile bool HVisOn;      // Keeps track of when HV is on/off

/* Initialize the GPIO pins
 * Note that these are not the BCM GPIO pin
 * numbers or the physical header pin numbers!
 * Conversion table is at http://wiringpi.com/pins/
 */
static int ledPin = 4;
static int geigerPin = 5;
static int gatePin = 6;

/* How long to flash the LED when a count is
 * recorded, in milliseconds
 */
static int flashTime = 10;  // ms

/*
 * breakHandler: Captures CTRL-C so we can shut down cleanly.
 *****************************************************************************
 */

void breakHandler(int s) {

  /* Tell all loops and threads to exit */
  keepRunning = false;
}

/*
 * countInterrupt: Runs when a count is detected.
 *****************************************************************************
 */

void countInterrupt (void) {
  /* Increment the various counters */
  sec[secNum]++;
  min[minNum]++;
  hour[hourNum]++;

  /* Tell the LED to light up */
  ledTime += flashTime;
}

/*
 * blinkLED: Thread to handle LED blinking.
 *****************************************************************************
 */

void *blinkLED (void *vargp) {

  /* Set up nanosleep() */
  struct timespec tim;
  tim.tv_sec = 0;
  tim.tv_nsec = flashTime * 1000000;  // Convert from ns to ms

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
 *****************************************************************************
 */

int getIndex(int numIndex) {

  /* Make sure our number is a valid index */
  numIndex = numIndex % size;

  /* If it's negative, count backwards from the end of the array */
  if (numIndex < 0) {
    numIndex = size + numIndex;
  }

  return numIndex;
}

/*
 * sumCounts: Sum the number of counts across the last numSecs seconds.
 *****************************************************************************
 */

int sumCounts(int numSecs) {

  int total = 0;
  int numHours = numSecs / 3600;                           // Convert seconds to hours
  int numMins = (numSecs - (numHours * 3600)) / 60;        // Convert seconds to minutes
  numSecs = numSecs - (numMins * 60) - (numHours * 3600);  // Remaining seconds

  /* Sum hours */
  for (int i=0; i < numHours; i++) {
    total += hour[getIndex(hourNum - i)];
  }

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
 *****************************************************************************
 */

float averageCounts(int numSecs) {

  /* Sum the counts, divide by the number of seconds */
  return ((float)sumCounts(numSecs) / (float)numSecs);
}

/*
 * cpmTouSv: Convert cpm (counts per minute) to microSieverts/hour
 *****************************************************************************
 */
float cpmTouSv(int numSecs) {

  /* Conversion factor for SBM-20 tube
   * From https://sites.google.com/site/diygeigercounter/gm-tubes-supported
   */
  //float factor = 0.0057;

  /* Conversion factor for SBM-20 tube
   * From https://www.uradmonitor.com/topic/hardware-conversion-factor/
   * This one gave results consistent with another portable radiation
   * monitor I had available (within 1-2%).
   */
  float factor = 0.006315;

  float uSv;
  float cpm;

  /* Moving average of counts */
  cpm = averageCounts(numSecs) * 60.0;

  /* Multiply by conversion factor  */
  uSv = factor * cpm;

  return uSv;
}

/*
 * count: Thread to handle count_related activities.
 *****************************************************************************
 */

void *count (void *vargp) {

  while (keepRunning) {
    /* FIXME: Make this more accurate */
    sleep(1);

    /* Increment the elapsed time counter */
    elapsed++;

    /* Increment the seconds counter */
    secNum++;

    /* Roll the seconds buffer */
    if (secNum % size == 0) {

      /* Increment the minutes counter */
      minNum++;

      /* Roll the minutes buffer */
      if (minNum % size == 0) {

        /* Increment the hours counter */
        hourNum++;

        /* Roll the hours buffer */
        if (hourNum % size == 0) {
          hourNum = 0;
        }

        minNum = 0;

        /* Initialize the current hour to zero */
        hour[hourNum] = 0;
      }

      secNum = 0;
      /* Initialize the current minute to zero */
      min[minNum] = 0;
    }

    /* Initialize the current second to zero */
    sec[secNum] = 0;

  }

  pthread_exit(NULL);
}

/*
 * post: Thread to perform a power on self-test.
 *****************************************************************************
 */

void *post (void *vargp) {

  printf("POST for next 10 seconds...\n");
  turnHVOn = true;
  sleep(10);
  turnHVOn = false;
  printf("POST complete!\n");

  pthread_exit(NULL);
}

/*
 * turnHVOn: Turns HV on and logs it.
 *****************************************************************************
 */

void HVOn (void) {

  digitalWrite(gatePin, HIGH);
  HVisOn = true;
  printf("HV is on!\n");
}

/*
 * turnHVOff: Turns HV off and logs it.
 *****************************************************************************
 */

void HVOff (void) {

  digitalWrite(gatePin, LOW);
  HVisOn = false;
  printf("HV is off.\n");
}

/*
 *****************************************************************************
 * main
 *****************************************************************************
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

  /* HV is off by default */
  turnHVOn = false;
  HVisOn = false;

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
  hourNum = -1;
  minNum = -1;
  secNum = 0;

  /* Initialize the counting arrays */
  for (int i=0; i < size; i++) {
    sec[i] = min[i] = hour[i] = 0;
  }

  /* Set up the counting thread */
  pthread_t count_id;
  pthread_create(&count_id, &attr, count, NULL);

  /* Set up the counting thread */
  pthread_t post_id;
  pthread_create(&post_id, &attr, post, NULL);

 /* Loop forever or until CTRL-C */
  while (keepRunning) {

    /* Turn HV on */
    if ((turnHVOn) && (!HVisOn)) {
      HVOn();
    }
    /* Turn HV off */
    else if ((!turnHVOn) && (HVisOn)) {
      HVOff();
    }

    /* Sleep for 1 s */
    sleep(1);

    /* Write some output */
    float temp2 = cpmTouSv(120);

    if (secNum % 20 == 0) {
      //printf("%0d:%0d Counter: %5d\n", minNum, secNum, sumCounts(10));
      printf("uSv/hr: %f\n", temp2);
      turnHVOn = true;
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
  HVOff();
  /* Turn off LED */
  digitalWrite(ledPin, LOW);

  return EXIT_SUCCESS;
}
