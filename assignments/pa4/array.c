#include "array.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// shared, circular FIFO array
// typedef struct {
//    char* arr[ARRAY_SIZE]; // array of char* (strings)
//    int head; // track first item - consume here
//    int tail; // track last item - produce here
//    int count; // count items
// } shared_t;

// extern sem_t mutex;
// extern sem_t full;
// extern sem_t empty;

void synchronize_init(shared_t *s) {
  sem_init(&s->mutex, SHARED, 1);
  sem_init(&s->full, SHARED, 0);
  sem_init(&s->empty, SHARED, ARRAY_SIZE);

  return;
}

int array_init(shared_t *s) {
  synchronize_init(s);
  sem_wait(&s->mutex);

  s->head = 0;
  s->tail = 0;

  // allocate mem for all slots, limit to max name
  for (int i = 0; i < ARRAY_SIZE; i++) {
    char *str = (char *)malloc(MAX_NAME_LENGTH * sizeof(char));
    if (str == NULL) {
      printf("Failed to allocate memory\n");
      return -1;
    }

    s->arr[i] = str;
  }

  sem_post(&s->mutex);

  return 0;
}

int array_put(shared_t *s, char *hostname) {
  sem_wait(&s->empty); // block producer if no empty slots
  sem_wait(&s->mutex); // acquire exclusive access

  if (strlen(hostname) > MAX_NAME_LENGTH) {
    printf("Hostname %s too large to store\n", hostname);
    return -1;
  }

  int num_items;
  if (sem_getvalue(&s->full, &num_items) == -1) {
    printf("sem_getvalue failed\n");
    return -1;
  }

  if (num_items == ARRAY_SIZE)
    return -1; // avoid overwriting full buffer

  // modulo for circular behavior
  // tail = next open index
  s->tail = (s->head + num_items) % ARRAY_SIZE;
  // strcpy(dest, src) - copy string without overwriting mem addr
  // prevents putting a var with a stack address that will disappear
  strcpy(s->arr[s->tail], hostname);
  printf("Added %s\n", hostname);

  sem_post(&s->mutex); // release access
  sem_post(&s->full);  // signal that a slot has been filled

  return 0;
}

// int array_put_wrapper(void* arg) {
//     put_args_t* put_args;
//     int result;
//
//     put_args = (put_args_t *) arg;
//
//     result = array_put_core(put_args->shared_arr, put_args->hostname);
//
//     return result;
// }

int array_get(shared_t *s, char **hostname) {
  sem_wait(&s->full);  // block consumer if no full slots
  sem_wait(&s->mutex); // acquire exclusive access

  // char* name = s->arr[s->head];
  printf("Retrieving hostname %s\n", s->arr[s->head]);
  // *hostname = name;
  hostname = &s->arr[s->head];
  // update queue info
  s->head = (s->head + 1) % ARRAY_SIZE;

  sem_post(&s->mutex); // release access
  sem_post(&s->empty); // signal that a slot has been emptied

  return 0;
}

// int array_get_wrapper(void* arg) {
//     get_args_t*  get_args;
//     int result;
//
//     get_args = (get_args_t*) arg;
//
//     result = array_get_core(get_args -> shared_arr, get_args->hostname);
//
//     return result;
// }

void array_free(shared_t *s) {
  sem_wait(&s->mutex);
  // free all associated mem
  for (int i = 0; i < ARRAY_SIZE; i++) {
    free(s->arr[i]);
    s->arr[i] = NULL;
  }

  sem_post(&s->mutex); // release mutual exclusion
  synchronize_free(s); // destroy synchronization mechanisms

  return;
}

void synchronize_free(shared_t *s) {
  sem_destroy(&s->mutex);
  sem_destroy(&s->full);
  sem_destroy(&s->empty);

  return;
}
