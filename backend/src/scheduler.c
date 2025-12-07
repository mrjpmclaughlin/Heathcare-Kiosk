#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include "globals.h"
#include "scheduler.h"
#include "queue.h"

// Writes into backend/output/
#define OUTPUT_PATH "output/"
#define MAX_ROOMS 3   // maximum number of patients being seen at once

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
        "  \"scheduling_policy\": \"FCFS\",\n"
        "  \"total_patients\": %d,\n"
        "  \"average_wait_seconds\": %d,\n"
        "  \"last_update\": \"N/A\"\n"
        "}\n",
        total,
        0
    );

    fclose(s);
}

void* scheduler_main(void* arg) {
    (void)arg;

    int next_room = 1;

    while (1) {
        sleep(2);  // scheduler tick

        pthread_mutex_lock(&lock);

        // Move a patient from waiting queue into a room if possible
        if (queue.size > 0 && being_seen.size < MAX_ROOMS) {
            Patient p = dequeue(&queue);

            // Assign an exam room
            snprintf(p.room, sizeof(p.room), "Exam %d", next_room);
            next_room++;
            if (next_room > MAX_ROOMS) {
                next_room = 1;
            }

            enqueue(&being_seen, p);
            write_log("[SCHEDULER] Moved %s (triage %d) into %s\n",
                      p.name, p.triage, p.room);
        }

        // Write JSON snapshots
        write_json_files();
        write_log("[SCHEDULER] JSON updated (%d waiting, %d being seen)\n",
                  queue.size, being_seen.size);

        pthread_mutex_unlock(&lock);
    }

    return NULL;
}