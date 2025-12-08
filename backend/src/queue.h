#ifndef QUEUE_H
#define QUEUE_H

#define MAX_PATIENTS 100

typedef struct {
    int id;
    char name[64];
    int triage;        // 1–5; we’ll treat 5 as most critical
    int kiosk_id;
    char room[32];     // exam room label, empty when waiting
    int time_in_room;  // how many scheduler ticks they’ve been in a room
    int wait_ticks;    // how many scheduler ticks they’ve waited in the queue
} Patient;

typedef struct {
    Patient items[MAX_PATIENTS];
    int size;
} PatientQueue;

void init_queue(PatientQueue* q);
void enqueue(PatientQueue* q, Patient p);
Patient dequeue(PatientQueue* q);
Patient remove_at(PatientQueue* q, int index);  // NEW helper

#endif