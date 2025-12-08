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
        // Simulate time between check-ins: 3–6 seconds
        sleep(3 + rand() % 4);

        pthread_mutex_lock(&lock);

        Patient p;
        p.id = patient_counter++;
        snprintf(p.name, sizeof(p.name), "Patient %d", p.id);
        p.kiosk_id = kiosk_id;
        p.triage = (rand() % 5) + 1;  // 1–5
        p.room[0] = '\0';
        p.time_in_room = 0;
        p.wait_ticks = 0;

        enqueue(&queue, p);
        write_log("[KIOSK %d] Added %s (triage %d)\n", kiosk_id, p.name, p.triage);

        pthread_mutex_unlock(&lock);
    }

    return NULL;
}
