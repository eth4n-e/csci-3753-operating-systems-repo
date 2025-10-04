#ifndef ARRAY_H
#define ARRAY_H

#include <semaphore.h>

#define ARRAY_SIZE 8 // max elements
#define MAX_NAME_LENGTH                                                        \
  18 // max hostname length + 1 slot for \n + 1 slot for \0 to be safe
#define SHARED 1

// shared, circular FIFO array
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

// empty - track empty slots - blocks producer = ARRAY_SIZE
// full - track filled slots - blocks consumer when full = 0
// mutex - binary semaphore - ensure access by single thread

/* semaphore relate */
void synchronize_init(shared_t *s);
void synchronize_free(shared_t *s);

/* circular array related */
int array_init(shared_t *s);
int array_put(shared_t *s, char *hostname);
// wrapper methods to integrate with threads
// int array_put_wrapper(void* arg);
// char** to allow for newly allocated mem to be returned (addr change)
// change addr of calling pointer
int array_get(shared_t *s, char **hostname);
// int array_get_wrapper(void* arg);
void array_free(shared_t *s);

#endif
