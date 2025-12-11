#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "globals.h"
#include "kiosk.h"
#include "scheduler.h"
#include "queue.h"

// Define the globals declared in globals.h
pthread_mutex_t lock;
PatientQueue queue;       
PatientQueue being_seen;  
SchedulingPolicy scheduling_policy;
sem_t* er_slots;
pthread_cond_t not_full  = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
int discharged_count = 0;
int simulation_done  = 0;

const char* policy_name(SchedulingPolicy p) {
    switch (p) {
        case POLICY_FCFS:     return "FCFS";
        case POLICY_PRIORITY: return "PRIORITY";
        case POLICY_RR:       return "ROUND_ROBIN";
        case POLICY_MLFQ:     return "MLFQ_AGING";
        default:              return "UNKNOWN";
    }
}

int main(void) {
    srand((unsigned)time(NULL));

    // Initialize mutex and queues
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("pthread_mutex_init");
        return 1;
    }
    init_queue(&queue);
    init_queue(&being_seen);

       // Ask user to choose scheduling policy
    int choice = 1;
    printf("Select scheduling policy:\n");
    printf("  1) FCFS (First-Come, First-Served)\n");
    printf("  2) PRIORITY (by triage level)\n");
    printf("  3) ROUND ROBIN\n");
    printf("  4) MLFQ with aging\n");
    printf("Enter choice (1-4): ");
    if (scanf("%d", &choice) != 1) {
        choice = 1;
    }

    switch (choice) {
        case 1:
            scheduling_policy = POLICY_FCFS;
            break;
        case 2:
            scheduling_policy = POLICY_PRIORITY;
            break;
        case 3:
            scheduling_policy = POLICY_RR;
            break;
        case 4:
            scheduling_policy = POLICY_MLFQ;
            break;
        default:
            scheduling_policy = POLICY_FCFS;
            break;
    }

    const char* policy_string = policy_name(scheduling_policy);
    printf("Using scheduling policy: %s\n", policy_string);

    // Remove any stale semaphore from previous runs
    sem_unlink("/er_slots_sem");

    // Create/open named semaphore. needed to us enamed semaphores for MacOS
    er_slots = sem_open("/er_slots_sem", O_CREAT, 0644, MAX_ROOMS);
    if (er_slots == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    pthread_t kiosks[NUM_KIOSKS];
    pthread_t scheduler_thread;

    // Start 4 kiosk threads
    for (int i = 0; i < NUM_KIOSKS; i++) {
        int *id = malloc(sizeof(int));
        if (!id) {
            perror("malloc");
            return 1;
        }
        *id = i + 1;
        if (pthread_create(&kiosks[i], NULL, kiosk_thread, id) != 0) {
            perror("pthread_create kiosk");
            return 1;
        }
    }

    // Start scheduler thread
    if (pthread_create(&scheduler_thread, NULL, scheduler_main, NULL) != 0) {
        perror("pthread_create scheduler");
        return 1;
    }

    printf("Simulation running. Target: %d discharged patients.\n", MAX_PATIENTS);

    // Wait for scheduler to finish (it will set simulation_done once it hits MAX_PATIENTS)
    pthread_join(scheduler_thread, NULL);

    // Once scheduler is done, kiosks should notice simulation_done and exit too.
    for (int i = 0; i < NUM_KIOSKS; i++) {
        pthread_join(kiosks[i], NULL);
    }

    // Cleanup
    sem_close(er_slots);
    sem_unlink("/er_slots_sem");

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);

    printf("Simulation finished after %d discharged patients.\n", discharged_count);
    return 0;
}