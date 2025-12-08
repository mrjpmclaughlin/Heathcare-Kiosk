#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <semaphore.h>
#include "globals.h"
#include "scheduler.h"
#include "queue.h"

// Writes into backend/output/ (or adjust to "../frontend/" if you prefer)
#define OUTPUT_PATH "output/"

static const char* policy_name(SchedulingPolicy p) {
    switch (p) {
        case POLICY_FCFS:    return "FCFS";
        case POLICY_PRIORITY:return "PRIORITY";
        case POLICY_RR:      return "ROUND_ROBIN";
        case POLICY_MLFQ:    return "MLFQ_AGING";
        default:             return "UNKNOWN";
    }
}

void write_log(const char* fmt, ...) {
    char path[256];
    snprintf(path, sizeof(path), "%slog.txt", OUTPUT_PATH);

    FILE* f = fopen(path, "a");
    if (!f) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fclose(f);
}

void write_json_files() {
    char app_path[256];
    snprintf(app_path, sizeof(app_path), "%sappointments.json", OUTPUT_PATH);

    FILE* f = fopen(app_path, "w");
    if (!f) return;

    // ---- Waiting patients ----
    fprintf(f, "{\n  \"waiting\": [\n");

    for (int i = 0; i < queue.size; i++) {
        Patient p = queue.items[i];
        fprintf(f,
            "    {\"id\": %d, \"name\": \"%s\", \"triage\": %d, \"kiosk_id\": %d, \"status\": \"waiting\"}%s\n",
            p.id, p.name, p.triage, p.kiosk_id,
            (i == queue.size - 1 ? "" : ",")
        );
    }

    fprintf(f, "  ],\n");

    // ---- Being seen patients ----
    fprintf(f, "  \"being_seen\": [\n");
    for (int i = 0; i < being_seen.size; i++) {
        Patient p = being_seen.items[i];
        fprintf(f,
            "    {\"id\": %d, \"name\": \"%s\", \"triage\": %d, \"kiosk_id\": %d, \"status\": \"being_seen\", \"room\": \"%s\"}%s\n",
            p.id, p.name, p.triage, p.kiosk_id, p.room,
            (i == being_seen.size - 1 ? "" : ",")
        );
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);

    // ---- Status file ----
    char status_path[256];
    snprintf(status_path, sizeof(status_path), "%sstatus.json", OUTPUT_PATH);

    FILE* s = fopen(status_path, "w");
    if (!s) return;

    int total = queue.size + being_seen.size;

    fprintf(s,
        "{\n"
        "  \"scheduling_policy\": \"%s\",\n"
        "  \"total_patients\": %d,\n"
        "  \"average_wait_seconds\": %d,\n"
        "  \"last_update\": \"N/A\"\n"
        "}\n",
        policy_name(scheduling_policy),
        total,
        0
    );

    fclose(s);
}

// Choose which index in the waiting queue to schedule next
static int select_patient_index(SchedulingPolicy policy, PatientQueue* q) {
    if (q->size <= 0) return -1;

    switch (policy) {
        case POLICY_FCFS:
            // First-Come-First-Served: take front of queue
            return 0;

        case POLICY_PRIORITY: {
            // Highest triage value (5 = most critical)
            int best_index = 0;
            int best_triage = q->items[0].triage;
            for (int i = 1; i < q->size; i++) {
                if (q->items[i].triage > best_triage) {
                    best_triage = q->items[i].triage;
                    best_index = i;
                }
            }
            return best_index;
        }

        case POLICY_RR: {
            // Round Robin over indices
            static int rr_pos = 0;
            int idx = rr_pos % q->size;
            rr_pos = (rr_pos + 1) % q->size;
            return idx;
        }

        case POLICY_MLFQ: {
            // After aging step, still pick highest triage
            int best_index = 0;
            int best_triage = q->items[0].triage;
            for (int i = 1; i < q->size; i++) {
                if (q->items[i].triage > best_triage) {
                    best_triage = q->items[i].triage;
                    best_index = i;
                }
            }
            return best_index;
        }

        default:
            return 0;
    }
}

void* scheduler_main(void* arg) {
    (void)arg;

    int next_room = 1;
    int tick = 0;

    write_log("[SCHEDULER] Starting with policy %s\n", policy_name(scheduling_policy));

    while (1) {
        sleep(2);  // scheduler tick
        tick++;

        pthread_mutex_lock(&lock);

        // ---- 1) Aging for MLFQ: increase wait_ticks and boost triage over time ----
        if (scheduling_policy == POLICY_MLFQ) {
            for (int i = 0; i < queue.size; i++) {
                Patient *p = &queue.items[i];
                p->wait_ticks++;
                if (p->wait_ticks >= AGE_THRESHOLD && p->triage < 5) {
                    p->triage++;          // promote priority
                    p->wait_ticks = 0;    // reset wait counter
                    write_log("[SCHEDULER] Aging: boosted %s to triage %d\n",
                              p->name, p->triage);
                }
            }
        } else {
            // For other policies, just increment wait_ticks (optional: for stats later)
            for (int i = 0; i < queue.size; i++) {
                queue.items[i].wait_ticks++;
            }
        }

        // ---- 2) Update patients already being seen (service time & discharge) ----
        int i = 0;
        while (i < being_seen.size) {
            Patient *p = &being_seen.items[i];
            p->time_in_room++;

            if (p->time_in_room >= SERVICE_TICKS) {
                // Discharge this patient
                write_log("[SCHEDULER] Discharged %s from %s (triage %d)\n",
                          p->name, p->room, p->triage);

                // Remove from being_seen by shifting left
                for (int j = i + 1; j < being_seen.size; j++) {
                    being_seen.items[j - 1] = being_seen.items[j];
                }
                being_seen.size--;

                // Free one ER room slot in the semaphore
                sem_post(er_slots);

                continue;  // do not increment i; we just shifted
            }

            i++;
        }

        // ---- 3) While there is a free ER slot, admit patients based on scheduling policy ----
        while (queue.size > 0) {
            // Try to take a room slot without blocking
            if (sem_trywait(er_slots) != 0) {
                // No free rooms right now
                break;
            }

            int index = select_patient_index(scheduling_policy, &queue);
            if (index < 0) {
                // Nothing to schedule after all; return the slot
                sem_post(er_slots);
                break;
            }

            Patient p = remove_at(&queue, index);

            // Assign exam room and reset time in room
            snprintf(p.room, sizeof(p.room), "Exam %d", next_room);
            p.time_in_room = 0;
            p.wait_ticks   = 0;  // they’re no longer waiting

            next_room++;
            if (next_room > MAX_ROOMS) {
                next_room = 1;
            }

            enqueue(&being_seen, p);
            write_log("[SCHEDULER] Moved %s (triage %d) into %s using policy %s\n",
                      p.name, p.triage, p.room, policy_name(scheduling_policy));

            // For RR over the waiting queue, you can optionally rotate here,
            // but since you use rr_pos in select_patient_index, it’s not necessary.
        }

        // ---- 4) Write JSON snapshots ----
        write_json_files();
        write_log("[SCHEDULER] JSON updated (%d waiting, %d being seen) under %s\n",
                  queue.size, being_seen.size, policy_name(scheduling_policy));


        pthread_mutex_unlock(&lock);
    }

    return NULL;
}
