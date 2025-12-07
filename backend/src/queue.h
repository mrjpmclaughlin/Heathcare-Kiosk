#ifndef QUEUE_H
#define QUEUE_H

#define MAX_PATIENTS 100

typedef struct {
    int id;
    char name[64];
    int triage;
    int kiosk_id;
    char room[32];   // NEW: exam room label, empty when waiting
} Patient;

typedef struct {
    Patient items[MAX_PATIENTS];
    int size;
} PatientQueue;

void init_queue(PatientQueue* q);
void enqueue(PatientQueue* q, Patient p);
Patient dequeue(PatientQueue* q);

#endif