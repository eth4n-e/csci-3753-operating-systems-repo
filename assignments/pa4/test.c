#include "array.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_ITEMS 2
#define NUM_THREADS 4

typedef struct {
  pthread_t tid;
  shared_t *shared_arr;
  char *hostname;
  int num_items;
} thread_args_t;

void *produce_routine(void *t_args) {
  thread_args_t *args = t_args;
  int thread_id = args->tid;
  int n_items = args->num_items;
  shared_t *shared = args->shared_arr;
  char *host = args->hostname;

  int result;

  for (int i = 0; i < n_items; i++) {
    result = array_put(shared, host);
    if (result == -1) {
      return (void *)-1;
      free(args);
    }
  }

  return (void *)0;
}

void *consume_routine(void *t_args) {
  thread_args_t *args = t_args;
  int thread_id = args->tid;
  int n_items = args->num_items;
  shared_t *shared = args->shared_arr;
  char *host = args->hostname;
  char **hostname_store = &host;

  int result;

  for (int i = 0; i < n_items; i++) {
    result = array_get(shared, hostname_store);
    if (result == -1) {
      return (void *)-1;
      free(args);
    }
  }

  return (void *)0;
}

int main() {
  shared_t shared_arr;

  if (array_init(&shared_arr) < 0) {
    array_free(&shared_arr);
  }

  pthread_t threads[NUM_THREADS];

  int result;

  for (int i = 0; i < NUM_THREADS; i++) {
    thread_args_t *args = malloc(sizeof(thread_args_t));
    if (args == NULL) {
      printf("Error allocating memory\n");
      exit(EXIT_FAILURE);
    }

    args->tid = i + 1;
    args->num_items = NUM_ITEMS;
    args->shared_arr = &shared_arr;
    args->hostname = "facebook.com";

    if (i % 2 == 0) {
      result = pthread_create(&threads[i], NULL, produce_routine, (void *)args);
    } else {
      result = pthread_create(&threads[i], NULL, consume_routine, (void *)args);
    }

    if (result == -1) {
      printf("Error creating thread %d\n", threads[i]);
      free(args);
      return -1;
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  array_free(&shared_arr);
}
