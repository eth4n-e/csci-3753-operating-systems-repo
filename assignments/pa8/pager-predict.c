#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simulator.h"

#define FREEBUF 18
#define ALPHA 0.8f
#define DECAY_INTERVAL 48000
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define TRUE 1
#define WINDOWSIZE 1000
#define LINEAR 0
#define LOOP 1
#define BRANCH 2

typedef struct {
  int proc;       // process waiting on paging operation
  int page;       // page being operated on
  int page_start; // when paging began
  int page_end;   // expected paging end
  int waiting;    // boolean value to signal if process is still waiting
  int pagein;     // is operation pagein or pageout
} Paging;

void set_pending_page_op(Paging tracker[][MAXPROCPAGES], int proc, int page,
                         int tick, int pagein) {
  tracker[proc][page].page_start = tick;
  tracker[proc][page].page_end = tick + PAGEWAIT;
  tracker[proc][page].pagein = pagein;
  tracker[proc][page].waiting = TRUE;
}

int find_lru_page_local(int timestamps[][MAXPROCPAGES], Pentry q[MAXPROCESSES],
                        Paging wait[][MAXPROCPAGES], int proc, int *lru_page) {
  int res = 0;
  int lru_time = INT_MAX;
  *lru_page = -1;
  for (int vic = 0; vic < MAXPROCPAGES; vic++) {
    if (q[proc].pages[vic] == 1 && wait[proc][vic].waiting == 0 &&
        timestamps[proc][vic] < lru_time) {
      lru_time = timestamps[proc][vic];
      *lru_page = vic;
      res = 1;
    }
  }

  return res;
}

int find_lru_page_global(int timestamps[][MAXPROCPAGES], Pentry q[MAXPROCESSES],
                         Paging wait[][MAXPROCPAGES], int *lru_proc,
                         int *lru_page) {
  int res = 0;
  int lru_time = INT_MAX;
  *lru_proc = -1;
  *lru_page = -1;
  for (int proc = 0; proc < MAXPROCESSES; proc++) {
    for (int vic = 0; vic < MAXPROCPAGES; vic++) {
      // only select in memory processes not already waiting on paging operation
      if (q[proc].pages[vic] == 1 && wait[proc][vic].waiting == 0 &&
          timestamps[proc][vic] < lru_time) {
        res = 1;
        lru_time = timestamps[proc][vic];
        *lru_proc = proc;
        *lru_page = vic;
      }
    }
  }

  return res;
}

typedef struct {
  int page;
  int freq;
  // int next; - if it makes sense to keep track of the next most likely page
  // from this one int timestamp; - if it makes sense to keep track of time of
  // access
} PageData;

void init_transition(PageData transitions[][MAXPROCPAGES][MAXPROCPAGES]) {
  for (int i = 0; i < MAXPROCESSES; i++) {
    for (int from = 0; from < MAXPROCPAGES; from++) {
      for (int to = 0; to < MAXPROCPAGES; to++) {
        transitions[i][from][to].page = to;
        transitions[i][from][to].freq = 0;
      }
    }
  }
}

void apply_decay(PageData arr[][MAXPROCPAGES][MAXPROCPAGES]) {
  for (int i = 0; i < MAXPROCESSES; i++) {
    for (int from = 0; from < MAXPROCPAGES; from++) {
      for (int to = 0; to < MAXPROCPAGES; to++) {
        arr[i][from][to].freq = (int)arr[i][from][to].freq * ALPHA;
      }
    }
  }
}

int compare_frequencies(const void *a, const void *b) {
  const PageData *pageA = (const PageData *)a;
  const PageData *pageB = (const PageData *)b;

  return (pageB->freq - pageA->freq);
}

/* Sorts transition frequencies stored in 3rd dimension of transition matrix
** in descending order to identify max frequencies and thus
** most likely transitions

** @param pages - list of page objects
** @param sorted - pointer to arr to store sorted frequencies
** @param n_freqs - number of frequencies to sort

** @return void - sorts frequencies in place
**
*/
PageData *sort(PageData *pages, PageData *sorted, int n_freqs) {
  size_t num_bytes = n_freqs * sizeof(pages[0]);
  memcpy(sorted, pages, num_bytes);

  qsort(sorted, n_freqs, sizeof(sorted[0]), compare_frequencies);

  return sorted;
}

// PageData* predict(int proc, int pc, PageData
// transitions[][MAXPROCPAGES][MAXPROCPAGES]) {
//     int future_page = (pc + 101) / PAGESIZE;
//     return transitions[proc][future_page];
// }

typedef struct {
  int proc;
  int proc_type; // ideally we deduce process type from pc behavior
  int set_size;  // working set size, number of unique page references in window
} WorkSet;

void init_workset(WorkSet work[MAXPROCESSES]) {
  for (int proc = 0; proc < MAXPROCESSES; proc++) {
    work[proc].proc = proc;
    work[proc].proc_type = -1;
    work[proc].set_size = -1;
  }
}

void update_working_set(int timestamps[][MAXPROCPAGES], Pentry q[MAXPROCESSES],
                        WorkSet work[MAXPROCESSES],
                        Paging paging[][MAXPROCPAGES], int tick) {
  // only evaluate on WINDOWSIZE cadence
  if (tick % WINDOWSIZE != 0)
    return;

  // if the tick - timestamp < window size
  // the current page is part of working set
  for (int proc = 0; proc < MAXPROCESSES; proc++) {
    int pg_count = 0;
    for (int page = 0; page < MAXPROCPAGES; page++) {
      // page referenced within last window
      if ((tick - timestamps[proc][page]) < WINDOWSIZE) {
        // page in page that belongs in the working set if not already present
        if (q[proc].pages[page] != 1) {
          if (pagein(proc, page)) {
            set_pending_page_op(paging, proc, page, tick, TRUE);
          }
        }
        pg_count++;
      }
    }

    work[proc].set_size = pg_count;
  }
}

void pageit(Pentry q[MAXPROCESSES]) {

  /* This file contains the stub for a predictive pager */
  /* You may need to add/remove/modify any part of this file */

  /* Static vars */
  static int initialized = 0;
  static int tick = 1; // artificial time
  // static int in_mem_count = 0;
  static int timestamps[MAXPROCESSES]
                       [MAXPROCPAGES]; // track usage time of pages
  static PageData transitions[MAXPROCESSES][MAXPROCPAGES][MAXPROCPAGES];
  static int pg_last[MAXPROCESSES] = {
      -1}; // track last page accessed by each process
  static Paging proc_wait[MAXPROCESSES]
                         [MAXPROCPAGES]; // track paging operations in process
  static WorkSet workset[MAXPROCESSES];

  /* Local vars */
  int proc;
  int page;
  int last_page;
  int cur_page;
  int in_mem_count;

  /* initialize static vars on first run */
  if (!initialized) {
    init_transition(transitions);
    init_workset(workset);
    for (proc = 0; proc < MAXPROCESSES; proc++) {
      for (page = 0; page < MAXPROCPAGES; page++) {
        timestamps[proc][page] = 0;

        // track paging operations
        // TODO: add init method for page waits
        proc_wait[proc][page].proc = proc;
        proc_wait[proc][page].page = page;
        proc_wait[proc][page].page_start = -1;
        proc_wait[proc][page].page_end = -1;
        proc_wait[proc][page].waiting = 0;
        proc_wait[proc][page].pagein = -1;
      }
    }
    initialized = 1;
  }

  // update wait statuses
  for (proc = 0; proc < MAXPROCESSES; proc++) {
    for (page = 0; page < MAXPROCPAGES; page++) {
      // paging operation has completed
      if (proc_wait[proc][page].page_end > 0 &&
          proc_wait[proc][page].page_end <= tick) {
        proc_wait[proc][page].page_start = -1;
        proc_wait[proc][page].page_end = -1;
        proc_wait[proc][page].waiting = !TRUE;

        // track count of pages in memory
        // if (proc_wait[proc][page].pagein == TRUE) {
        //     in_mem_count++;
        // } else if (proc_wait[proc][page].pagein == !TRUE) {
        //     in_mem_count--;
        // }
        proc_wait[proc][page].pagein = -1;
      }
    }
  }

  in_mem_count = 0; // recompute in mem count every tick
  for (proc = 0; proc < MAXPROCESSES; proc++) {
    // count pages 100% in mem
    for (page = 0; page < MAXPROCPAGES; page++) {
      if (q[proc].pages[page]) {
        in_mem_count++;
      } else if (proc_wait[proc][page].waiting &&
                 proc_wait[proc][page].pagein) {
        // factor in ongoing pageins (occupying frame but not yet in mem)
        in_mem_count++;
      }
    }
  }

  // DEBUGGING
  // if (tick % 100000 == 0) {
  //   printf("In mem count: %d\n", in_mem_count);
  //   for (proc=0; proc < MAXPROCESSES; proc++) {
  //       printf("Working set size for proc %d: %d\n", proc,
  //       workset[proc].set_size); for(page=0; page < MAXPROCPAGES; page++) {
  //           printf("Wait status for proc %d, page %d - waiting: %d, pagein:
  //           %d\n", proc, page, proc_wait[proc][page].waiting,
  //           proc_wait[proc][page].pagein);
  //       }
  //   }
  // }

  // # in mem pages cutting into free pool
  if (PHYSICALPAGES - in_mem_count < FREEBUF) {
    // frames we need to free
    int need_free = FREEBUF - (PHYSICALPAGES - in_mem_count);
    for (int i = 0; i < need_free; i++) {
      int lru_proc = -1;
      int lru_page = -1;

      // lru local
      // for (proc=0; proc < MAXPROCESSES; proc++) {
      //     if (find_lru_page_local(timestamps, q, proc_wait, proc, &lru_page))
      //     {
      //         if (pageout(proc, lru_page)) {
      //             set_pending_page_op(proc_wait, proc, lru_page, tick,
      //             !TRUE);
      //         } else {
      //             break; // cannot free mem
      //         }
      //     } else {
      //         break; // exit if unable to find page to evict
      //     }
      // }

      // lru global
      if (find_lru_page_global(timestamps, q, proc_wait, &lru_proc,
                               &lru_page)) {
        if (pageout(lru_proc, lru_page)) {
          set_pending_page_op(proc_wait, lru_proc, lru_page, tick, !TRUE);
        } else {
          break; // cannot free mem
        }
      } else {
        break; // exit if unable to find page to evict
      }
    }
  }

  // Update transition matrix
  for (proc = 0; proc < MAXPROCESSES; proc++) {
    if (q[proc].active != 1)
      continue;

    last_page = pg_last[proc];

    if (last_page == -1)
      continue; // process yet to access pages

    int pc = q[proc].pc;
    cur_page = pc / PAGESIZE;

    if (last_page == cur_page)
      continue; // not considering same page
    // current page becomes previous
    pg_last[proc] = cur_page;

    // update transition states
    transitions[proc][last_page][cur_page].freq += 1;
    transitions[proc][last_page][cur_page].page = cur_page;
    // last page no longer relevant
    // pageout(proc, last_page);

    // decay counts on set intervals to favor recent trends
    if (tick % DECAY_INTERVAL == 0) {
      apply_decay(transitions);
    }
  }

  // DEBUGGING
  // if (tick % 50000 == 0) {
  //     for(proc=0; proc < MAXPROCESSES; proc++) {
  //         for(int from=0; from < MAXPROCPAGES; from++) {
  //             for (int to=0; to < MAXPROCPAGES; to++) {
  //                 printf("Transitions for proc %d from page %d to page %d:
  //                 %d\n", proc, from, to, transitions[proc][from][to].freq);
  //             }
  //         }
  //     }
  // }

  update_working_set(timestamps, q, workset, proc_wait, tick);
  // if (tick % 50000 == 0) {
  //     for(proc=0; proc < MAXPROCESSES; proc++) {
  //         printf("Workset %d set size %d\n", proc, workset[proc].set_size);
  //     }
  // }

  // Attempt current page in
  for (proc = 0; proc < MAXPROCESSES; proc++) {
    if (q[proc].active != 1)
      continue; // only active processes

    int pc = q[proc].pc;
    cur_page = pc / PAGESIZE;

    timestamps[proc][cur_page] = tick;
    // DEBUGGING
    // if(tick < 1005 && tick > 95) {
    //     printf("Current page and pc for process %d at tick %d: %d, %d\n",
    //     proc, tick, pc, cur_page);
    // }

    if (q[proc].pages[cur_page])
      continue; // page already in memory
    if (pagein(proc, cur_page)) {
      // proc waiting on pagein
      set_pending_page_op(proc_wait, proc, cur_page, tick, TRUE);
      continue;
    }

    // WORKING SET PAGEOUT IMPLEMENTATION
    // for (proc = 0; proc < MAXPROCESSES; proc++) {
    //     if (q[proc].active != 1) continue;
    //     for (page = 0; page < MAXPROCPAGES; page++) {
    //         // page not referenced within current window
    //         if ((tick - timestamps[proc][page] >= WINDOWSIZE)) {
    //             // pageout if in memory and not already on way out
    //             if (q[proc].pages[page] && proc_wait[proc][page].waiting ==
    //             !TRUE) {
    //                 if(pageout(proc, page)) {
    //                     set_pending_page_op(proc_wait, proc, page, tick,
    //                     TRUE);
    //                 }
    //             }
    //         }
    //     }
    // }

    // selecting victim process if pagein fails (LRU)
    int lru_page = -1;
    // int lru_proc = -1;

    // if (find_lru_page_global(timestamps, q, proc_wait, &lru_proc, &lru_page))
    // {
    //     if (pageout(lru_proc, lru_page)) {
    //         set_pending_page_op(proc_wait, lru_proc, lru_page, tick, !TRUE);
    //         break;
    //     }
    // }

    if (find_lru_page_local(timestamps, q, proc_wait, proc, &lru_page)) {
      if (pageout(proc, lru_page)) {
        set_pending_page_op(proc_wait, proc, lru_page, tick, !TRUE);
        break;
      }
    }
  }

  // Attempt predictive page ins
  for (proc = 0; proc < MAXPROCESSES; proc++) {
    if (q[proc].active != 1)
      continue;

    cur_page = q[proc].pc / PAGESIZE;

    PageData first_trans[MAXPROCPAGES];  // lookahead 1
    PageData second_trans[MAXPROCPAGES]; // lookahead based on transition 1

    // order by most - least likely transitions
    sort(transitions[proc][cur_page], first_trans, MAXPROCPAGES);

    PageData first_trans_next = first_trans[0]; // next most likely page
    PageData first_trans_alt = first_trans[1];  // alternative

    // current page has yet to transition
    if (first_trans_next.freq == 0)
      continue;

    // find most likely transitions after transition 1 (lookahead 2)
    sort(transitions[proc][first_trans_next.page], second_trans, MAXPROCPAGES);
    PageData second_trans_next = second_trans[0];
    // PageData lookahead_alt = second_trans[1];

    int loop_pg = 0;

    // CASE 1: Linear
    // cur page < next page < lookahead 2
    if (first_trans_next.page > cur_page) {
      workset[proc].proc_type = LINEAR;
      // keep next 2 predicted pages in
      if (pagein(proc, first_trans_next.page)) {
        set_pending_page_op(proc_wait, proc, first_trans_next.page, tick, TRUE);
      }

      if (pagein(proc, second_trans_next.page)) {
        set_pending_page_op(proc_wait, proc, second_trans_next.page, tick,
                            TRUE);
      }

    } else if (first_trans_next.page < cur_page) { // loop
      workset[proc].proc_type = LOOP;
      // try to prefetch loop body
      for (loop_pg = first_trans_next.page; loop_pg < cur_page; loop_pg++) {
        if (pagein(proc, loop_pg)) {
          set_pending_page_op(proc_wait, proc, loop_pg, tick, TRUE);
        }
      }
    } else if (first_trans_next.freq > 0 &&
               first_trans_alt.freq > 0) { // branch
      // for both branches, page in branch + most likely page to follow
      if (pagein(proc, first_trans_next.page)) {
        set_pending_page_op(proc_wait, proc, first_trans_next.page, tick, TRUE);

        // pagein page following branch
        if (pagein(proc, second_trans_next.page)) {
          set_pending_page_op(proc_wait, proc, second_trans_next.page, tick,
                              TRUE);
        }
      }

      if (pagein(proc, first_trans_alt.page)) {
        set_pending_page_op(proc_wait, proc, first_trans_alt.page, tick, TRUE);

        PageData second_trans_alt[MAXPROCPAGES]; // capture transitions from the
                                                 // alternate branch

        sort(transitions[proc][first_trans_alt.page], second_trans_alt,
             MAXPROCPAGES);
        PageData second_branch_next = second_trans_alt[0];
        if (pagein(proc, second_branch_next.page)) {
          set_pending_page_op(proc_wait, proc, second_branch_next.page, tick,
                              TRUE);
        }
      }
    }

    // DEBUGGING
    // if (tick % 50000 == 0) {
    //     printf("Current page: %d\n", cur_page);
    //     for (int i = 0; i < MAXPROCPAGES; i++) {
    //         printf("Sorted freqs [%d].freq: %d\n", i, first_trans[i].freq);
    //     }

    //     for (int j = 0; j < MAXPROCPAGES; j++) {
    //         printf("Sorted freqs [%d].page: %d\n", j, first_trans[j].page);
    //     }

    //     printf("Most likely transition from %d is to %d\n", cur_page,
    //     first_trans[0].page);
    // }
  }

  /* advance time for next pageit iteration */
  tick++;
}
