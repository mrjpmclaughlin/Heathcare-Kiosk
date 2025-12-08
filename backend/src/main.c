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
PatientQueue queue;       // waiting
PatientQueue being_seen;  // being seen
SchedulingPolicy scheduling_policy;
sem_t* er_slots;

int main(void) {
    srand((unsigned)time(NULL));

    // Initialize mutex and queues
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("pthread_mutex_init");
        return 1;
    }
    init_queue(&queue);
    init_queue(&being_seen);

       // === Ask user to choose scheduling policy ===
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

    printf("Using scheduling policy: %d\n", scheduling_policy);

    // Remove any stale semaphore from previous runs
    sem_unlink("/er_slots_sem");

    // Create/open named semaphore
    er_slots = sem_open("/er_slots_sem", O_CREAT, 0644, MAX_ROOMS);
    if (er_slots == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    pthread_t kiosks[4];
    pthread_t scheduler_thread;

    // Start 4 kiosk threads
    for (int i = 0; i < 4; i++) {
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

    printf("Simulation running. Press Ctrl+C to stop.\n");

    // Keep main alive; threads run indefinitely
    while (1) {
        sleep(1);
    }

    // (Unreached in this version)
    pthread_mutex_destroy(&lock);
    return 0;
}