#ifndef ARRAY_H
#define ARRAY_H

#include <semaphore.h>

#define ARRAY_SIZE 8 // max elements
#define MAX_NAME_LENGTH                                                        \
  18 // max hostname length + 1 slot for \n + 1 slot for \0 to be safe
#define PSHARED 0 // 0 indicates sharing between threads of a process

// shared, circular FIFO array
// added semaphores as members per piazza post
typedef struct {
  char *arr[ARRAY_SIZE]; // array of char* (strings)
  int head;              // track first item - consume here
  int tail;              // track last item - produce here
  sem_t mutex;           // binary semaphore for mutual exclusion
  sem_t full;            // number of filled slots
  sem_t empty;           // number of empty slots
} shared_t;

// unnamed semaphores primarily for synchronization within
// same process or between related processes sharing common memory region
// source: google -> "difference between unnamed and named semaphores"

/* semaphore related */
void synchronize_init(shared_t *s);
void synchronize_free(shared_t *s);

/* circular array related */
int array_init(shared_t *s);
int array_put(shared_t *s, char *hostname);
// char** to allow for newly allocated mem to be returned (addr change)
// change addr stored by calling pointer
int array_get(shared_t *s, char **hostname);
void array_free(shared_t *s);
