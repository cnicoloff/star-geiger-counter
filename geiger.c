
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
static volatile int sec;
static volatile int min;
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
  /* Increment the global counter, currently a placeholder */
  ++globalCount;

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
 * count: Thread to handle count_related activities.
 *********************************************************************************
 */

void *count (void *vargp) {

  while (keepRunning) {
    sleep(1);
    sec++;

    if ((sec != 0) && (sec % 10 == 0)) {
      printf("%d:%d Counter: %5d\n", min, sec, globalCount);
    }

    /* If a minute has passed... */
    if (sec % 60 == 0) {
      /* Write some output */
      /* fprintf(opf, "%d Counter: %5d\n", min, globalCount); */

      /* Increment the minute counter and reset the 
       * second counter */
      min++;
      sec = 0;
    }
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
  min = 0;
  sec = 0;
  globalCount = 0;

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
