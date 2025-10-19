#include "array.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void synchronize_init(shared_t *s) {
  sem_init(&s->mutex, PSHARED, 1);
  sem_init(&s->full, PSHARED, 0);
  sem_init(&s->empty, PSHARED, ARRAY_SIZE);

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
    // zero out buffer to ensure no garbage data + null term
    memset(s->arr[i], 0, MAX_NAME_LENGTH);
  }

  sem_post(&s->mutex);

  return 0;
}

int array_put(shared_t *s, char *hostname) {
  sem_wait(&s->empty); // block producer if no empty slots
  sem_wait(&s->mutex); // acquire exclusive access

  size_t host_len;
  host_len = strlen(hostname);
  if (host_len >= MAX_NAME_LENGTH) {
    printf("Hostname %s too large to store\n", hostname);
    return -1;
  }

  // retrieve number of filled slots
  int num_items;
  if (sem_getvalue(&s->full, &num_items) == -1) {
    printf("sem_getvalue failed\n");
    return -1;
  }

  // modulo for circular behavior
  // tail = next open index
  s->tail = (s->head + num_items) % ARRAY_SIZE;

  // always copy max len - 1 to leave space for null term
  // fixed size copying helps to maintain safety
  strncpy(s->arr[s->tail], hostname, MAX_NAME_LENGTH - 1);
  printf("Added hostname: %s\n", hostname);

  sem_post(&s->mutex); // release access
  sem_post(&s->full);  // signal that a slot has been filled

  return 0;
}

int array_get(shared_t *s, char **hostname) {
  sem_wait(&s->full);  // block consumer if no full slots
  sem_wait(&s->mutex); // acquire exclusive access

  size_t host_len;
  char *host;
  host = s->arr[s->head];
  host_len = strlen(host);

  printf("Retrieving hostname %s\n", host);
  // overwrite caller value with addr of host on heap mem
  if (hostname == NULL) {
    printf("Unable to dereference a NULL pointer\n");
    return -1;
  }

  strncpy(*hostname, host, MAX_NAME_LENGTH - 1);
  s->head = (s->head + 1) % ARRAY_SIZE;

  sem_post(&s->mutex); // release access
  sem_post(&s->empty); // signal that a slot has been emptied

  return 0;
}

void array_free(shared_t *s) {
  sem_wait(&s->mutex);
  // free all associated mem
  for (int i = 0; i < ARRAY_SIZE; i++) {
    free(s->arr[i]);
    s->arr[i] = NULL; // prevent double frees / accessing freed mem
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
