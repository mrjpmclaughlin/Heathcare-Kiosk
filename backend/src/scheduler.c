#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <semaphore.h>

#include "globals.h"
#include "scheduler.h"
#include "queue.h"

// Where backend files go
#define OUTPUT_PATH "output/"

static const char* policy_name(SchedulingPolicy p) {
    switch (p) {
        case POLICY_FCFS:     return "FCFS";
        case POLICY_PRIORITY: return "PRIORITY";
        case POLICY_RR:       return "ROUND_ROBIN";
        case POLICY_MLFQ:     return "MLFQ_AGING";
        default:              return "UNKNOWN";
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

    // Waiting patients
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

    // Being seen patients
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

    // Status file
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

    // Emergency override: always pick a triage-5 patient first if any exist
    for (int i = 0; i < q->size; i++) {
        if (q->items[i].triage == 5) {
            return i;
        }
    }

    //scheduling algorithms
    switch (policy) {
        case POLICY_FCFS:
            // First-Come-First-Served: take front of queue
            return 0;

        case POLICY_PRIORITY: {
            // Highest triage value 5 = most critical
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

    write_log("[SCHEDULER] Starting with policy %s\n", policy_name(scheduling_policy));

    while (1) {
        sleep(2);  // scheduler tick

        pthread_mutex_lock(&lock);

        if (simulation_done) {
            pthread_mutex_unlock(&lock);
            break;  // exit scheduler_main
        }       

        //  Aging for MLFQ so it will increase wait_ticks and boost triage over time. Consulted ChatGPT to help with the logic
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
            // For other policies, just increment wait_ticks
            for (int i = 0; i < queue.size; i++) {
                queue.items[i].wait_ticks++;
            }
        }

        // Update patients already being seen
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

                // Increment discharged count and check stop condition
                discharged_count++;
                write_log("[SCHEDULER] discharged_count = %d (MAX_PATIENTS = %d)\n",
                    discharged_count, MAX_PATIENTS);
                if (discharged_count >= MAX_PATIENTS && !simulation_done) {
                    simulation_done = 1;
                    write_log("[SCHEDULER] Reached MAX_PATIENTS = %d; stopping simulation\n",
                              MAX_PATIENTS);

                    // Wake up any waiting producers/consumers so they can exit
                    pthread_cond_broadcast(&not_full);
                    pthread_cond_broadcast(&not_empty);
                }

                continue;  // do not increment i; we just shifted
            }


            i++;
        }
        if (simulation_done) {
            pthread_mutex_unlock(&lock);
            break;
        }
        // interrupt: emergency preemption for triage-5 patients
        // Look for an emergency in the waiting queue.
        int emergency_index = -1;
        for (int k = 0; k < queue.size; k++) {
            if (queue.items[k].triage == 5) {
                emergency_index = k;
                break;
            }
        }

        // Only consider preemption if:
        //  - There is at least one emergency waiting, AND
        //  - All rooms are currently full.
        if (emergency_index >= 0 && being_seen.size == MAX_ROOMS) {
            // Choose a victim in being_seen to be preempted:
            // lowest triage; if tie, longest time_in_room.
            int victim_index = -1;
            int worst_triage = 6;      // higher than any real triage
            int longest_time = -1;

            for (int k = 0; k < being_seen.size; k++) {
                Patient *bp = &being_seen.items[k];

                // don't preempt another triage-5 emergency.
                if (bp->triage >= 5) {
                    continue;
                }

                if (bp->triage < worst_triage) {
                    worst_triage = bp->triage;
                    longest_time = bp->time_in_room;
                    victim_index = k;
                } else if (bp->triage == worst_triage &&
                           bp->time_in_room > longest_time) {
                    longest_time = bp->time_in_room;
                    victim_index = k;
                }
            }

            if (victim_index >= 0) {
                // found someone to preempt
                Patient victim = being_seen.items[victim_index];

                // Remove victim from being_seen shift left
                for (int j = victim_index + 1; j < being_seen.size; j++) {
                    being_seen.items[j - 1] = being_seen.items[j];
                }
                being_seen.size--;

                // Take the emergency patient from the waiting queue
                Patient emerg = remove_at(&queue, emergency_index);

                // Emergency gets the victim's room
                snprintf(emerg.room, sizeof(emerg.room), "%s", victim.room);
                emerg.time_in_room = 0;
                emerg.wait_ticks   = 0;

                // Victim goes back to the end of the waiting room
                victim.room[0]      = '\0';
                victim.time_in_room = 0;
                victim.wait_ticks   = 0;
                enqueue(&queue, victim);

                write_log("[INTERRUPT] Emergency %s (triage %d) preempted %s for room %s\n",
                          emerg.name, emerg.triage, victim.name, emerg.room);

                // Put emergency into being_seen
                enqueue(&being_seen, emerg);

            }
        }

        // Consumer side of producer–consumer: wait for patients when buffer empty
        if (being_seen.size < MAX_ROOMS) {
            // If no patients are waiting, block the scheduler until a kiosk signals not_empty
            while (queue.size == 0) {
                write_log("[SCHEDULER] Waiting room empty; consumer blocking\n");
                pthread_cond_wait(&not_empty, &lock);
            }

            if (simulation_done) {
                pthread_mutex_unlock(&lock);
                break;
            }
            // should be queue.size > 0

            // Try to claim an ER slot (resource) without blocking forever
            if (sem_trywait(er_slots) == 0) {
                int index = select_patient_index(scheduling_policy, &queue);
                if (index >= 0) {
                    Patient p = remove_at(&queue, index);

                    // remove one from the bounded buffer → signal a free slot to producers
                    pthread_cond_signal(&not_full);

                    // Assign exam room and reset time in room
                    snprintf(p.room, sizeof(p.room), "Exam %d", next_room);
                    p.time_in_room = 0;
                    p.wait_ticks   = 0;

                    next_room++;
                    if (next_room > MAX_ROOMS) {
                        next_room = 1;
                    }

                    enqueue(&being_seen, p);
                    write_log("[SCHEDULER] Moved %s (triage %d) into %s using policy %s\n",
                              p.name, p.triage, p.room, policy_name(scheduling_policy));
                } else {
                    // No valid index after all; return the ER slot
                    sem_post(er_slots);
                }
            } else {
                // No ER slots available according to semaphore; do nothing this tick.
                write_log("[SCHEDULER] No ER slots available (semaphore)\n");
            }
        }

        // Write JSON snapshots
        write_json_files();
        write_log("[SCHEDULER] JSON updated (%d waiting, %d being seen) under %s\n",
                  queue.size, being_seen.size, policy_name(scheduling_policy));

        pthread_mutex_unlock(&lock);
    }
    write_log("[SCHEDULER] Exiting (simulation_done)\n");
    return NULL;
}
