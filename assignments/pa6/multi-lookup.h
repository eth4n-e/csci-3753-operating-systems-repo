#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include "array.h"
#include <netinet/in.h> // for INET6_ADDRSTRLEN
#include <pthread.h>
#include <stdio.h>

#define MAX_FILE_NAME_LENGTH 20 // names51.txt (11) + input/ (6) ~ 17 + 3 extra
#define MAX_INPUT_FILES 100
#define MAX_REQUESTER_THREADS 10
#define MAX_RESOLVER_THREADS 10
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define POISON "{END}" // braces not allowed in hostnames
#define ERROR -1
#define NOT_RESOLVED "NOT_RESOLVED"

typedef struct {
  pthread_mutex_t serviced;
  pthread_mutex_t results;
  pthread_mutex_t sout;
  pthread_mutex_t serr;
} output_mutexes_t;

// Key interfaces
typedef struct {
  array *consume_arr; // shared array to consume data from
  array *produce_arr; // shared array to produce to
  FILE *output_file;  // file to log results
  output_mutexes_t
      *out_locks; // mutexes for exclusive access to output (file, stdout, ...)
  int num_serviced;
} thread_args_t;

void output_synchronize_init(output_mutexes_t *output);
void output_synchronize_free(output_mutexes_t *output);

void init_resources(array *files, array *host, output_mutexes_t *output);
void free_resources(array *files, array *host, output_mutexes_t *output);

/* Method to handle writing to file
 */
ssize_t handle_write(output_mutexes_t *output, FILE *file, char *buf);

/* Method to handle reading file inputs
 */
// TODO: intending on only using this in main method
// TODO: not sure if mutexes necessary here
ssize_t handle_read(array *shared, FILE *file, char *buf);

/* Thread routine for requester threads
** Functionality:
** - Reads filenames from a shared array
** - Opens and reads each file in the shared array
** - Puts contents of file into another shared array
*/
void *requester(void *arg);

/* Thread routine for resolver threads
** Functionality:
** - Reads contents from a shared array
** - Resolves each hostname into an IP address
** - Writes (hostname, IP) pair to results file
*/
void *resolver(void *arg);

/* Generic method to spawn threads
** Handles both requesters and resolvers
*/
int spawn_threads(void *(*routine)(void *), pthread_t threads[],
                  thread_args_t *args[], thread_args_t *shared_args,
                  int num_threads);
/* Method to handle creating requester threads and their associated resources
** Params:
** - Thread id
** - Array to hold thread arguments
** - number of threads to create
** Returns: integer response code
*/
int spawn_requesters(pthread_t req_tid[], thread_args_t *args[],
                     thread_args_t *shared_args, int num_requesters);

/* Method to handle creating resolvers and their associated resources
** Returns: integer response code
*/
int spawn_resolvers(pthread_t res_tid[], thread_args_t *args[],
                    thread_args_t *shared_args, int num_resolvers);

int poison_shared_array(array *shared, output_mutexes_t *out_locks,
                        char *poison, int num_pills);

int main(int argc, char **argv);
#endif
