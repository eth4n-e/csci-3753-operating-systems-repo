#include "multi-lookup.h"
#include "util.h"
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BASE_ARG_NUM 6
#define DATA_START_IDX 5

const char *manual =
    "NAME\nmulti-lookup - resolve a set of hostnames to IP "
    "addresses\n\nSYNOPSIS\nmulti-lookup <# requester> <# resolver> <requester "
    "log><resolver log> [ <data file> ...]\n\nDESCRIPTION\nThe file names "
    "specified by <data file> are passed to the pool of requester threads "
    "which place information into a shared data area. Resolver threads read "
    "the shared data area and find the corresponding IP address.\n\n<# "
    "requesters> number of requester threads to place into the thread pool\n<# "
    "resolvers> number of resolver threads to place into the thread "
    "pool\n<requester log> name of the file into which requested hostnames are "
    "written\n<resolver log> name of the file which hostnames and resolved IP "
    "addresses are written\n<data file> filename to be processed. Each file "
    "contains a list of host names, oone per line, that are to be resolved\n";

// TODO: if these methods become bloated, separate into another header file and
// leave this file to only include main
void output_mutexes_init(output_mutexes_t *output) {
  // TODO: init returns an int (tied to error code) may have to check
  pthread_mutex_init(&output->results, NULL);
  pthread_mutex_init(&output->serviced, NULL);
  pthread_mutex_init(&output->sout, NULL);
  pthread_mutex_init(&output->serr, NULL);
}

void output_mutexes_free(output_mutexes_t *output) {
  pthread_mutex_destroy(&output->results);
  pthread_mutex_destroy(&output->serviced);
  pthread_mutex_destroy(&output->sout);
  pthread_mutex_destroy(&output->serr);
}

void init_resources(array *files, array *hosts, output_mutexes_t *output) {
  array_init(files);
  array_init(hosts);
  output_mutexes_init(output);
}
void free_resources(array *files, array *hosts, output_mutexes_t *output) {
  array_free(files);
  array_free(hosts);
  output_mutexes_free(output);
}

/* Method to handle writing to files
 */
ssize_t handle_write(output_mutexes_t *f, FILE *file, char *buf) {}

/* Method to handle reading file inputs
 */
// TODO: intending on only using this in main method
// TODO: not sure if mutexes necessary here
ssize_t handle_read(array *shared, FILE *file, char *buf) {}
/* what do I need to have:
- mutex initialization for requester / resolver
- method to spawn resolver threads
- method to spawn requester threads
- main method
- routine to read filenames from argv and perform array puts into a shared array
*/

/* Thread routine for requester threads
** Functionality:
** - Reads filenames from a shared array
** - Opens and reads each file in the shared array
** - Puts contents of file into another shared array
*/

// TODO: consider adding buf to store data to thread args
void *requester(void *arg) {
  // track method time
  // reason for using clock_gettime:
  // https://stackoverflow.com/questions/5362577/c-gettimeofday-for-computing-time
  struct timespec start, finish;
  clock_gettime(CLOCK_MONOTONIC, &start);

  thread_args_t *args = (thread_args_t *)arg;
  pthread_t thread_id = pthread_self();

  int result;
  result = 0;

  // args to retrieve file names
  char file_buf[MAX_FILE_NAME_LENGTH];
  char *file_name = file_buf;

  // vars for reading from file
  FILE *file;
  char buffer[MAX_NAME_LENGTH];

  while (1) {
    // consumption from first shared array
    if (array_get(args->consume_arr, &file_name) == ERROR) {
      result = ERROR;
      break;
    }
    file_name[MAX_FILE_NAME_LENGTH - 1] = '\0';

    // Main thread finished writing file names
    if (strcmp(file_name, POISON) == 0) {
      // produce poison pill into shared array used by resolvers
      // no more incoming hosts
      fprintf(stdout, "Poison pill consumed");
      result = ERROR;
      break;
    }

    // restrict access to file received
    pthread_mutex_lock(&args->out_locks->serviced);
    file = fopen(file_name, "r");
    if (file == NULL) {
      pthread_mutex_lock(&args->out_locks->serr);
      fprintf(stderr, "Invalid file: %s\n", file);
      pthread_mutex_unlock(&args->out_locks->serr);

      result = ERROR;
      break;
    }

    // read each line of file - store in buffer
    while (fgets(buffer, MAX_NAME_LENGTH, file) != NULL) {
      // replace newline with null term
      // source:
      // https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
      buffer[strcspn(buffer, "\n")] = 0;
      // add hostname to shared arr for resolver
      if (array_put(args->produce_arr, buffer) == ERROR) {
        result = ERROR;
        break;
      };
      // write hostname to serviced file
      fprintf(args->output_file, "%s\n", buffer);
    }
    if (result == ERROR)
      break; // catch break from loop above
    pthread_mutex_unlock(&args->out_locks->serviced);

    args->num_serviced++;
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);
  // display thread stats
  pthread_mutex_lock(&args->out_locks->sout);
  fprintf(stdout, "thread %lu serviced %d files in %ld seconds\n", thread_id,
          args->num_serviced, finish.tv_sec - start.tv_sec);
  pthread_mutex_unlock(&args->out_locks->sout);

  // cleanup thread resources
  free(arg);
  arg = NULL;
  fclose(file);

  return result;
}

/* Thread routine for resolver threads
** Functionality:
** - Reads contents from a shared array
** - Resolves each hostname into an IP address
** - Writes (hostname, IP) pair to results file
*/
void *resolver(void *arg) {
  // track execution time
  struct timespec start, finish;
  clock_gettime(CLOCK_MONOTONIC, &start);

  thread_args_t *args = (thread_args_t *)arg;
  pthread_t thread_id = pthread_self();

  int result;
  result = 0;

  // vars to retrieve host names
  char host_buf[MAX_NAME_LENGTH];
  char *host_name = host_buf;

  // vars to retrieve dns resolved hostname
  char dns_buf[MAX_IP_LENGTH];
  char *dns_store = dns_buf;

  while (1) {
    if (array_get(args->consume_arr, &host_name) == ERROR) {
      result = ERROR;
      break;
    }

    if (strcmp(host_name, POISON) == 0) {
      // produce poison pill into shared array used by resolvers
      // no more incoming hosts
      fprintf(stdout, "Poison pill consumed");
      result = ERROR;
      break;
    }

    // resolve hostname
    if (dnslookup(host_name, dns_store, MAX_IP_LENGTH) == UTIL_FAILURE) {
      // copy "NOT_RESOLVED" into buffer
      strncpy(dns_store, NOT_RESOLVED, MAX_IP_LENGTH);
      dns_store[MAX_IP_LENGTH - 1] = '\0';
    }

    pthread_mutex_lock(&args->out_locks->results);
    fprintf(args->output_file, "%s, %s\n", host_name, dns_store);
    pthread_mutex_unlock(&args->out_locks->results);

    args->num_serviced++;
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);

  pthread_mutex_lock(&args->out_locks->sout);
  fprintf(stdout, "thread %lu resolved %d hosts in %ld seconds\n", thread_id,
          args->num_serviced, finish.tv_sec - start.tv_sec);
  pthread_mutex_unlock(&args->out_locks->sout);

  free(arg);
  arg = NULL;

  return (void *)result;
}

/* General thread spawner that allows for varied arguments and thread routines
 */
int spawn_threads(void *(*routine)(void *), pthread_t threads[],
                  thread_args_t *args[], thread_args_t *shared_args,
                  int num_threads) {
  int result = 0;

  for (int i = 0; i < num_threads; i++) {
    args[i] = malloc(sizeof(thread_args_t));
    if (args[i] == NULL) {
      fprintf(stderr, "Error allocating memory for arguments\n");
      result = ERROR;
      free(args[i]);
      args[i] = NULL;
      break;
    }

    // Each thread gets a unique copy of arguments (on heap)
    // but share common resources to begin
    args[i]->consume_arr = shared_args->consume_arr;
    args[i]->produce_arr = shared_args->produce_arr;
    args[i]->output_file = shared_args->output_file;
    args[i]->out_locks = shared_args->out_locks;
    args[i]->num_serviced = shared_args->num_serviced;

    result =
        pthread_create(&threads[i], NULL, (void *)routine, (void *)args[i]);
    if (result == ERROR) {
      pthread_mutex_lock(&args[i]->out_locks->serr);
      fprintf(stderr, "Failed to create thread\n");
      pthread_mutex_unlock(&args[i]->out_locks->serr);

      free(args[i]);
      args[i] = NULL;
    }
  }

  return result;
}

int poison_shared_array(array *shared, output_mutexes_t *out_locks,
                        char *poison, int num_pills) {
  for (int i = 0; i < num_pills; i++) {
    if (array_put(shared, poison) == ERROR) {
      // pthread_mutex_lock(&out_locks->serr);
      // fprintf(stderr, "Failed to write to shared array\n");
      // pthread_mutex_unlock(&out_locks->serr);
      return ERROR;
    }
  }
}

int main(int argc, char **argv) {
  if (argc < BASE_ARG_NUM) {
    fprintf(stdout, "%s", manual);
    return ERROR;
  }

  char *endptr; // stores first invalid character from strtol
  errno = 0;    // strtol only modifies errno on error
  long num_requesters;
  long num_resolvers;
  FILE *serviced;
  FILE *results;

  // TODO: move argument parsing / handling into a separate method to clean up
  // main
  num_requesters = strtol(argv[1], &endptr, 10);
  if (errno != 0 || *endptr != '\0' || num_requesters > MAX_REQUESTER_THREADS ||
      num_requesters < 0) {
    fprintf(stderr, "Invalid number of requester threads: %s\n", argv[1]);
    return ERROR;
  }

  num_resolvers = strtol(argv[2], &endptr, 10);
  if (errno != 0 || *endptr != '\0' || num_resolvers > MAX_RESOLVER_THREADS ||
      num_resolvers < 0) {
    fprintf(stderr, "Invalid number of requester threads: %s\n", argv[2]);
    return ERROR;
  }

  serviced = fopen(argv[3], "w");
  if (serviced == NULL) {
    fprintf(stderr, "Invalid filename: %s\n", argv[3]);
    return ERROR;
  }

  results = fopen(argv[4], "w");
  if (results == NULL) {
    fprintf(stderr, "Invalid filename: %s\n", argv[4]);
    return ERROR;
  }

  int num_data_files;
  // 1 index the starting index for data files
  num_data_files = argc - (DATA_START_IDX + 1);
  if (num_data_files > MAX_INPUT_FILES) {
    fprintf(stderr, "Invalid number of data files: %d\n",
            argc - num_data_files);
    return ERROR;
  }

  // initialize synchronization resources
  array file_store;
  array host_store;
  output_mutexes_t output;
  init_resources(&file_store, &host_store, &output);

  int thread_result;
  // setup requesters
  pthread_t req_tid[num_requesters];
  thread_args_t *req_args[num_requesters];
  // define args common across requesters
  thread_args_t shared_req_args;
  shared_req_args.consume_arr = &file_store;
  shared_req_args.produce_arr = &host_store;
  shared_req_args.output_file = serviced;
  shared_req_args.out_locks = &output;
  shared_req_args.num_serviced = 0;

  thread_result = spawn_threads(requester, req_tid, req_args, &shared_req_args,
                                num_requesters);

  if (thread_result == ERROR) {
    return ERROR;
  }

  // setup resolvers
  pthread_t res_tid[num_resolvers];
  thread_args_t *res_args[num_requesters];

  // define args common across resolvers
  thread_args_t shared_res_args;
  shared_res_args.consume_arr = &host_store;
  shared_res_args.produce_arr = NULL; // resolvers do not produce
  shared_res_args.output_file = results;
  shared_res_args.out_locks = &output;
  shared_res_args.num_serviced = 0;

  thread_result = spawn_threads(resolver, res_tid, res_args, &shared_res_args,
                                num_resolvers);

  if (thread_result == ERROR) {
    return ERROR;
  }

  // write filenames to first shared array
  for (int i = DATA_START_IDX; i < argc; i++) {
    if (array_put(&file_store, argv[i]) == ERROR) {
      pthread_mutex_lock(&output.serr);
      fprintf(stderr, "Failed to write to shared array\n");
      pthread_mutex_unlock(&output.serr);
    }
  }

  // poison requesters after filling buffer with all file names
  // poison_shared_array(&file_store, POISON, num_requesters);

  // wait / join threads
  for (int i = 0; i < num_requesters; i++) {
    pthread_join(req_tid[i], NULL);
    free(req_args[i]);
  }

  // TODO: add poison pills for resolver threads
  // TODO: this may occur in the resolver thread logic
  // TODO: idea -> when requester receives poison pill it should write poison
  // pill and then end

  // poison resolvers after all requesters finish
  // poison_shared_array(&host_store, POISON, num_resolvers);

  for (int i = 0; i < num_resolvers; i++) {
    pthread_join(res_tid[i], NULL);
    free(res_args[i]);
  }

  free_resources(&file_store, &host_store, &output);

  if (fclose(serviced) == EOF) {
    fprintf(stderr, "Error closing file");
    return ERROR;
  }

  if (fclose(results) == EOF) {
    fprintf(stderr, "Error closing file");
    return ERROR;
  }
}
