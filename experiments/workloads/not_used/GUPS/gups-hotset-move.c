/*
 * =====================================================================================
 *
 *       Filename:  gups.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  02/21/2018 02:36:27 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include "./lib/timer.h"


#include "gups.h"

#define MAX_THREADS     64

#define BATCH			10000000

#define GUPS_PAGE_SIZE      (4 * 1024)
#define PAGE_NUM            3
#define PAGES               2048

#ifdef HOTSPOT
extern uint64_t hotset_start;
extern double hotset_fraction;
#endif

int threads;

bool move_hotset1 = false;

uint64_t hot_start = 0;
uint64_t hotsize = 0;

struct gups_args {
  int tid;                      // thread id
  uint64_t *indices;       // array of indices to access
  void* field;                  // pointer to start of thread's region
  uint64_t iters;          // iterations to perform
  uint64_t size;           // size of region
  uint64_t elt_size;       // size of elements
  uint64_t hot_start;            // start of hot set
  uint64_t hotsize;        // size of hot set
};


static inline uint64_t rdtscp(void)
{
    uint32_t eax, edx;
    // why is "ecx" in clobber list here, anyway? -SG&MH,2017-10-05
    __asm volatile ("rdtscp" : "=a" (eax), "=d" (edx) :: "ecx", "memory");
    return ((uint64_t)edx << 32) | eax;
}

uint64_t thread_gups[MAX_THREADS];

static unsigned long updates, nelems;

bool stop = false;

static void *timing_thread()
{
  uint64_t tic = -1;
  bool printed1 = false;
  for (;;) {
    tic++;
    if (tic >= 150 && tic < 300) {
      if (!printed1) {
        move_hotset1 = true;
        fprintf(stderr, "moved hotset1\n");
        printed1 = true;
      }
    }
    if (tic >= 250) {
      stop = true;
    }
    sleep(1);
  }
  return 0;
}

uint64_t tot_updates = 0;

static void *print_instantaneous_gups()
{
  FILE *tot;
  uint64_t tot_gups, tot_last_second_gups = 0;


  tot = fopen("tot_gups.txt", "w");
  if (tot == NULL) {
    perror("fopen");
  }

  for (;;) {
    tot_gups = 0;
    for (int i = 0; i < threads; i++) {
      tot_gups += thread_gups[i];
    }
    fprintf(tot, "%.10f\n", (1.0 * (abs(tot_gups - tot_last_second_gups))) / (1.0e9));
    tot_updates += abs(tot_gups - tot_last_second_gups);
    tot_last_second_gups = tot_gups;
    sleep(1);
  }

  return NULL;
}


static uint64_t lfsr_fast(uint64_t lfsr)
{
  lfsr ^= lfsr >> 7;
  lfsr ^= lfsr << 9;
  lfsr ^= lfsr >> 13;
  return lfsr;
}

char *filename = "indices1.txt";

static void *do_gups(void *arguments)
{
  //printf("do_gups entered\n");
  struct gups_args *args = (struct gups_args*)arguments;
  struct timeval starttime, checktime;
  FILE *performancefile = NULL;
  uint64_t *field = (uint64_t*)(args->field);
  uint64_t i;
  uint64_t index1, index2;
  uint64_t elt_size = args->elt_size;
  char data[elt_size];
  uint64_t lfsr;
  uint64_t hot_num;
  uint64_t before_accesses = 0;

  srand(args->tid);
  lfsr = rand();

  index1 = 0;
  index2 = 0;

  for (i = 0; i < args->iters; i++) {
    hot_num = lfsr_fast(lfsr) % 100;
    if (hot_num < 90) {
      lfsr = lfsr_fast(lfsr);
      index1 = args->hot_start + (lfsr % args->hotsize);
      if (move_hotset1) {
        if ((index1 < (args->hotsize / 4))) {
          index1 += args->hotsize;
        }
      }
      else {
        if ((index1 < (args->hotsize / 4))) {
          before_accesses++;
        }
      }
      if (elt_size == 8) {
        uint64_t  tmp = field[index1];
        tmp = tmp + i;
        field[index1] = tmp;
      }
      else {
        memcpy(data, &field[index1 * elt_size], elt_size);
        memset(data, data[0] + i, elt_size);
        memcpy(&field[index1 * elt_size], data, elt_size);
      }
    }
    else {
      lfsr = lfsr_fast(lfsr);
      index2 = lfsr % (args->size);
      if (elt_size == 8) {
        uint64_t tmp = field[index2];
        tmp = tmp + i;
        field[index2] = tmp;
      }
      else {
        memcpy(data, &field[index2 * elt_size], elt_size);
        memset(data, data[0] + i, elt_size);
        memcpy(&field[index2 * elt_size], data, elt_size);
      }
    }

    if ((i + 1) % BATCH == 0) {
		gettimeofday(&checktime, NULL);
		performancefile = fopen("./data/performance.txt", "w");
		
		if (!performancefile) {
			perror("fopen");
			assert(0);
		}

		fprintf(performancefile, "%.0f\n", BATCH / elapsed(&starttime, &checktime));

		fclose(performancefile);
		gettimeofday(&starttime, NULL);
	}


    if (i % 10000 == 0) {
      thread_gups[args->tid] += 10000;
    }

    if (stop) {
      break;
    }
  }

  fprintf(stderr, "before_accesses: %lu\n", before_accesses);

  //fclose(timefile);
  return 0;
}

int main(int argc, char **argv)
{
  unsigned long expt;
  unsigned long size, elt_size;
  unsigned long tot_hot_size;
  int log_hot_size;
  struct timeval starttime, stoptime;
  double secs, gups;
  int i;
  void *p;
  struct gups_args** ga;
  pthread_t t[MAX_THREADS];

  if (argc != 6) {
    fprintf(stderr, "Usage: %s [threads] [updates per thread] [exponent] [data size (bytes)] [noremap/remap]\n", argv[0]);
    fprintf(stderr, "  threads\t\t\tnumber of threads to launch\n");
    fprintf(stderr, "  updates per thread\t\tnumber of updates per thread\n");
    fprintf(stderr, "  exponent\t\t\tlog size of region\n");
    fprintf(stderr, "  data size\t\t\tsize of data in array (in bytes)\n");
    fprintf(stderr, "  hot size\t\t\tlog size of hot set\n");
    return 0;
  }

  gettimeofday(&starttime, NULL);

  threads = atoi(argv[1]);
  assert(threads <= MAX_THREADS);
  ga = (struct gups_args**)malloc(threads * sizeof(struct gups_args*));

  updates = atol(argv[2]);
  updates -= updates % 256;
  expt = atoi(argv[3]);
  assert(expt > 8);
  assert(updates > 0 && (updates % 256 == 0));
  size = (unsigned long)(1) << expt;
  size -= (size % 256);
  assert(size > 0 && (size % 256 == 0));
  elt_size = atoi(argv[4]);
  log_hot_size = atof(argv[5]);
  tot_hot_size = (unsigned long)(1) << log_hot_size;

  fprintf(stderr, "%lu updates per thread (%d threads)\n", updates, threads);
  fprintf(stderr, "field of 2^%lu (%lu) bytes\n", expt, size);
  fprintf(stderr, "%ld byte element size (%ld elements total)\n", elt_size, size / elt_size);

  p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  if (p == MAP_FAILED) {
    perror("mmap");
    assert(0);
  }

  gettimeofday(&stoptime, NULL);
  fprintf(stderr, "Init took %.4f seconds\n", elapsed(&starttime, &stoptime));
  fprintf(stderr, "Region address: %p - %p\t size: %ld\n", p, (p + size), size);
  
  nelems = (size / threads) / elt_size; // number of elements per thread
  fprintf(stderr, "Elements per thread: %lu\n", nelems);

  memset(thread_gups, 0, sizeof(thread_gups));

  gettimeofday(&stoptime, NULL);
  secs = elapsed(&starttime, &stoptime);
  fprintf(stderr, "Initialization time: %.4f seconds.\n", secs);

  hot_start = 0;
  hotsize = (tot_hot_size / threads) / elt_size;

  gettimeofday(&starttime, NULL);
  for (i = 0; i < threads; i++) {
    ga[i] = (struct gups_args*)malloc(sizeof(struct gups_args));
    ga[i]->tid = i;
    ga[i]->field = p + (i * nelems * elt_size);
    ga[i]->iters = updates;
    ga[i]->size = nelems;
    ga[i]->elt_size = elt_size;
    ga[i]->hot_start = 0;        // hot set at start of thread's region
    ga[i]->hotsize = hotsize;
  }

  // run through gups once to touch all memory
  // spawn gups worker threads
  for (i = 0; i < threads; i++) {
    int r = pthread_create(&t[i], NULL, do_gups, (void*)ga[i]);
    assert(r == 0);
  }

  // wait for worker threads
  for (i = 0; i < threads; i++) {
    int r = pthread_join(t[i], NULL);
    assert(r == 0);
  }

  gettimeofday(&stoptime, NULL);

  secs = elapsed(&starttime, &stoptime);
  gups = threads * ((double)updates) / (secs * 1.0e9);
  memset(thread_gups, 0, sizeof(thread_gups));

  filename = "indices2.txt";

  pthread_t print_thread;
  int pt = pthread_create(&print_thread, NULL, print_instantaneous_gups, NULL);
  assert(pt == 0);


  pthread_t timer_thread;
  int tt = pthread_create(&timer_thread, NULL, timing_thread, NULL);
  assert (tt == 0);

  fprintf(stderr, "Timing.\n");
  gettimeofday(&starttime, NULL);

  // spawn gups worker threads
  for (i = 0; i < threads; i++) {
    ga[i]->iters = updates * 2;
    int r = pthread_create(&t[i], NULL, do_gups, (void*)ga[i]);
    assert(r == 0);
  }

  // wait for worker threads
  for (i = 0; i < threads; i++) {
    int r = pthread_join(t[i], NULL);
    assert(r == 0);
  }
  gettimeofday(&stoptime, NULL);

  secs = elapsed(&starttime, &stoptime);
  printf("Elapsed time: %.4f seconds.\n", secs);
  gups = ((double)tot_updates) / (secs * 1.0e9);
  printf("GUPS = %.10f\n", gups);

  memset(thread_gups, 0, sizeof(thread_gups));

  for (i = 0; i < threads; i++) {
    free(ga[i]);
  }
  free(ga);

  munmap(p, size);

  return 0;
}

