#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "globals.h"
#include "kiosk.h"
#include "scheduler.h"

int patient_counter = 1;

void* kiosk_thread(void* arg) {
    int kiosk_id = *(int*)arg;
    free(arg);

    while (1) {
        // Check before doing any more work
        pthread_mutex_lock(&lock);
        if (simulation_done) {
            pthread_mutex_unlock(&lock);
            break;
        }
        pthread_mutex_unlock(&lock);

        // Simulate time between check-ins: 3–6 seconds
        sleep(3 + rand() % 4);

        pthread_mutex_lock(&lock);

        // Check again after sleep in case simulation ended while we slept
        if (simulation_done) {
            pthread_mutex_unlock(&lock);
            break;
        }

        // ----- Producer–consumer bounded buffer: wait if waiting room is full -----
        while (!simulation_done && queue.size >= MAX_WAITING) {
            write_log("[KIOSK %d] Waiting room full (%d/%d); blocking producer\n",
                      kiosk_id, queue.size, MAX_WAITING);
            pthread_cond_wait(&not_full, &lock);
        }

        if (simulation_done) {
            pthread_mutex_unlock(&lock);
            break;
        }

        // Create and enqueue a new patient
        Patient p;
        p.id = patient_counter++;
        snprintf(p.name, sizeof(p.name), "Patient %d", p.id);
        p.kiosk_id = kiosk_id;
        p.triage = (rand() % 5) + 1;  // 1–5
        p.room[0] = '\0';
        p.time_in_room = 0;
        p.wait_ticks = 0;
        p.arrival_time = time(NULL);  // record real-world arrival time
        p.wait_seconds = -1;

        enqueue(&queue, p);
        write_log("[KIOSK %d] Added %s (triage %d)\n",
                  kiosk_id, p.name, p.triage);

        // Wake the scheduler (consumer)
        pthread_cond_signal(&not_empty);

        pthread_mutex_unlock(&lock);
    }

    write_log("[KIOSK %d] Exiting (simulation_done = %d, discharged_count = %d)\n",
              kiosk_id, simulation_done, discharged_count);
    return NULL;
}
