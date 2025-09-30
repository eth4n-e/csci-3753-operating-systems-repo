#include "array.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

// shared, circular FIFO array
// typedef struct {
//    char* arr[ARRAY_SIZE]; // array of char* (strings)
//    int head; // track first item - consume here
//    int tail; // track last item - produce here
//    int count; // count items
// } s_array;

// extern sem_t mutex;
// extern sem_t full;
// extern sem_t empty;

void synchronize_init(int buf_size) {
  sem_init(&mutex, SHARED, 1);
  sem_init(&full, SHARED, 0);
  sem_init(&empty, SHARED, buf_size);
}

int array_init(s_array *s) {
  s->head = 0;
  s->tail = 0;
  s->count = 0;

  return 0;
}

int array_put(s_array *s, char *hostname) {
  sem_wait(&empty); // block producer if no empty slots
  sem_wait(&mutex); // acquire exclusive access

  if (s->count == ARRAY_SIZE)
    return -1; // avoid overwriting full buffer

  s->tail = (s->head + s->count) % ARRAY_SIZE;

  sem_post(&mutex); // release access
  sem_post(&full);  // signal that a slot has been filled
}

int array_get(s_array *s, char **hostname) {
  sem_wait(&full);  // block consumer if no full slots
  sem_wait(&mutex); // acquire exclusive access

  char *name = s->arr[s->head]; // pull first in

  // update queue info
  s->head = (s->head + 1) % ARRAY_SIZE;
  s->count--;

  sem_post(&mutex); // release access
  sem_post(&empty); // signal that a slot has been emptied

  return &name;
}

void array_free(s_array *s) {}

void synchronize_free() {
  sem_destroy(&mutex);
  sem_destroy(&full);
  sem_destroy(&empty);
}
