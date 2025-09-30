#ifndef ARRAY_H
#define ARRAY_H

#include <semaphore.h>

#define ARRAY_SIZE 8       // max elements
#define MAX_NAME_LENGTH 16 // max hostname length
#define SHARED 1

// shared, circular FIFO array
typedef struct {
  char *arr[ARRAY_SIZE]; // array of char* (strings)
  int head;              // track first item - consume here
  int tail;              // track last item - produce here
  int count;             // count items
} s_array;

// unnamed semaphores primarily for synchronization within
// same process or between related processes sharing common memory region
// source: google -> "difference between unnamed and named semaphores"

extern sem_t empty; // track empty slots - blocks producer = ARRAY_SIZE
extern sem_t full;  // track filled slots - blocks consumer when full = 0
extern sem_t mutex; // binary semaphore - ensure access by single thread

/* semaphore relate */
void synchronize_init(int buf_size);
void synchronize_free();

/* circular array related */
int array_init(s_array *s);
int array_put(s_array *s, char *hostname);
// char** to allow for newly allocated mem to be returned (addr change)
// change addr of calling pointer
int array_get(s_array *s, char **hostname);
void array_free(s_array *s);

#endif
