#include "queue.h"

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

    for (int i = 1; i < q->size; i++) {
        q->items[i - 1] = q->items[i];
    }

    q->size--;
    return first;
}

// Remove and return patient at index, shifting others left.
Patient remove_at(PatientQueue* q, int index) {
    Patient empty = {0};
    if (index < 0 || index >= q->size) return empty;

    Patient chosen = q->items[index];
    for (int i = index + 1; i < q->size; i++) {
        q->items[i - 1] = q->items[i];
    }
    q->size--;
    return chosen;
}