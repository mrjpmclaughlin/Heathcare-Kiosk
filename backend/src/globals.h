#include <pthread.h>
#include <semaphore.h>   // ADD THIS
#include "queue.h"

typedef enum {
    POLICY_FCFS = 0,
    POLICY_PRIORITY = 1,
    POLICY_RR = 2,
    POLICY_MLFQ = 3
} SchedulingPolicy;

// move these here so everyone shares the same values
#define MAX_ROOMS 3
#define SERVICE_TICKS 5
#define AGE_THRESHOLD 5

extern pthread_mutex_t lock;
extern PatientQueue queue;
extern PatientQueue being_seen;
extern SchedulingPolicy scheduling_policy;
extern sem_t* er_slots;   // NEW: semaphore for ER slots
