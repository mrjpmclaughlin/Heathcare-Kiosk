#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>
#include "queue.h"

// These are defined once in main.c and used everywhere else
extern pthread_mutex_t lock;
extern PatientQueue queue;
extern PatientQueue being_seen;

#endif