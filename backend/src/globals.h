#include <pthread.h>
#include <semaphore.h>   // ADD THIS
#include "queue.h"

typedef enum {
    POLICY_FCFS = 0,
    POLICY_PRIORITY = 1,
    POLICY_RR = 2,
    POLICY_MLFQ = 3
} SchedulingPolicy;


#define MAX_ROOMS 3
#define SERVICE_TICKS 5
#define AGE_THRESHOLD 5
#define MAX_PATIENTS 100
#define NUM_KIOSKS 4

extern int discharged_count;
extern int simulation_done;
extern pthread_mutex_t lock;
extern PatientQueue queue;
extern PatientQueue being_seen;
extern SchedulingPolicy scheduling_policy;
extern sem_t* er_slots;   //semaphore for ER slots
extern pthread_cond_t not_full;   // signaled when a slot in the waiting room frees up
extern pthread_cond_t not_empty;  // signaled when at least one patient is waiting


#define MAX_WAITING 20   // max number of patients in waiting room
