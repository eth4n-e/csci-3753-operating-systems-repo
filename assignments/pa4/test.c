#include "array.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_ITEMS 4
#define NUM_THREADS 8
#define SLEEP 1

typedef struct {
  pthread_t tid;
  shared_t *shared_arr;
  char *hostname;
  int num_items;
} thread_args_t;

/*
 * Produce n_items into shared_arr with the host as specified in args
 */
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
    // adding sleeps to encourage thread interleaving
    // ensure shared buffer uncorrupted even if threads interrupted
    sleep(SLEEP);
  }

  return (void *)0;
}

/*
 * Consume n_items from the shared arr specified in args
 */
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
    sleep(SLEEP);
  }

  return (void *)0;
}

int main() {
  shared_t shared_arr;

  if (array_init(&shared_arr) < 0) {
    array_free(&shared_arr);
  }

  // store thread ids
  pthread_t threads[NUM_THREADS];
  thread_args_t *arg_list[NUM_THREADS];

  int result;

  for (int i = 0; i < NUM_THREADS; i++) {
    // dynamically allocate args to use
    // ensures each thread has private copy of data
    // prevent overwriting struct data each iteration
    // prevents main thread from deallocating struct and passing its memory (now
    // invalid) to the other threads
    thread_args_t *args = malloc(sizeof(thread_args_t));
    if (args == NULL) {
      printf("Error allocating memory\n");
      exit(EXIT_FAILURE);
    }

    args->tid = i + 1;
    args->num_items = NUM_ITEMS;
    args->shared_arr = &shared_arr;
    args->hostname = "facebook.com";

    if (i % 2 == 0) { // evens produce
      result = pthread_create(&threads[i], NULL, produce_routine, (void *)args);
    } else { // odds consume
      result = pthread_create(&threads[i], NULL, consume_routine, (void *)args);
    }

    if (result == -1) {
      printf("Error creating thread %d\n", threads[i]);
      free(args);
      args = NULL;
      return -1;
    }

    arg_list[i] = args;
  }

  // wait for threads to finish execution and cleanup resources
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
    free(arg_list[i]);
  }

  array_free(&shared_arr);
}
