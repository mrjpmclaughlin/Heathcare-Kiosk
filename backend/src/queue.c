#include "queue.h"
#include <stdio.h>

void init_queue(PatientQueue* q) {
    q->size = 0;
}

void enqueue(PatientQueue* q, Patient p) {
    if (q->size < MAX_PATIENTS) {
        q->items[q->size++] = p;
    }
}

Patient dequeue(PatientQueue* q) {
    Patient empty = {0};
    if (q->size == 0) return empty;

    Patient first = q->items[0];

    for (int i = 1; i < q->size; i++)
        q->items[i - 1] = q->items[i];

    q->size--;
    return first;
}