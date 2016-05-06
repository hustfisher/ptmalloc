/*
 * $Id:t-test1.c $
 * by Wolfram Gloger 1996-1999
 * A multi-thread test for malloc performance, maintaining one pool of
 * allocated bins per thread.
 */

#include "thread-m.h"

#if USE_PTHREADS /* Posix threads */

#include <pthread.h>

pthread_cond_t finish_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t finish_mutex = PTHREAD_MUTEX_INITIALIZER;

#endif

#if (defined __STDC__ && __STDC__) || defined __cplusplus
# include <stdlib.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#if !USE_MALLOC
#include <malloc.h>
#else
#include "ptmalloc.h"
#endif

#include "lran2.h"
#include "t-test.h"

#define N_TOTAL		10
#ifndef N_THREADS
#define N_THREADS	2
#endif
#ifndef N_TOTAL_PRINT
#define N_TOTAL_PRINT 50
#endif
#define STACKSIZE	32768
#ifndef MEMORY
#define MEMORY		8000000l
#endif
#define SIZE		10000
#define I_MAX		10000
#define ACTIONS_MAX	30

#define RANDOM(d,s)	(lran2(d) % (s))

struct bin_info {
	struct bin *m;
	unsigned long size, bins;
};

#if TEST > 0

void
bin_test(struct bin_info *p)
{
	int b;

	for(b=0; b<p->bins; b++) {
		if(mem_check(p->m[b].ptr, p->m[b].size)) {
			printf("memory corrupt!\n");
			exit(1);
		}
	}
}

#endif

struct thread_st {
	int bins, max, flags;
	unsigned long size;
	thread_id id;
	char *sp;
	long seed;
};

#if USE_PTHREADS || USE_THR || defined NO_THREADS
void *
malloc_test(void *ptr)
#else
void
malloc_test(void *ptr, size_t stack_len)
#endif
{
	struct thread_st *st = ptr;
	int b, i, j, actions, pid = 1;
	struct bin_info p;
	struct lran2_st ld; /* data for random number generator */

	lran2_init(&ld, st->seed);
#ifdef TEST_FORK
	if(RANDOM(&ld, TEST_FORK) == 0) {
		int status;

		printf("forking\n");
		pid = fork();
		if(pid > 0) {
			printf("waiting for %d...\n", pid);
			waitpid(pid, &status, 0);
			if(!WIFEXITED(status)) {
				printf("child term with signal %d\n", WTERMSIG(status));
			}
			goto end;
		}
	}
#endif
	p.m = (struct bin *)malloc(st->bins*sizeof(*p.m));
	p.bins = st->bins;
	p.size = st->size;
	for(b=0; b<p.bins; b++) {
		p.m[b].size = 0;
		p.m[b].ptr = NULL;
		if(RANDOM(&ld, 2) == 0)
			bin_alloc(&p.m[b], RANDOM(&ld, p.size) + 1, lran2(&ld));
	}
	for(i=0; i<=st->max;) {
#if TEST > 1
		bin_test(&p);
#endif
		actions = RANDOM(&ld, ACTIONS_MAX);
#if USE_MALLOC && MALLOC_DEBUG
		if(actions < 2) { mallinfo(); }
#endif
		for(j=0; j<actions; j++) {
			b = RANDOM(&ld, p.bins);
			bin_free(&p.m[b]);
		}
		i += actions;
		actions = RANDOM(&ld, ACTIONS_MAX);
		for(j=0; j<actions; j++) {
			b = RANDOM(&ld, p.bins);
			bin_alloc(&p.m[b], RANDOM(&ld, p.size) + 1, lran2(&ld));
#if TEST > 2
			bin_test(&p);
#endif
		}
#if 0 /* Test illegal free()s while setting MALLOC_CHECK_ */
		for(j=0; j<8; j++) {
			b = RANDOM(&ld, p.bins);
			if(p.m[b].ptr) {
			  int offset = (RANDOM(&ld, 11) - 5)*8;
			  char *rogue = (char*)(p.m[b].ptr) + offset;
			  /*printf("p=%p rogue=%p\n", p.m[b].ptr, rogue);*/
			  free(rogue);
			}
		}
#endif
		i += actions;
	}
	for(b=0; b<p.bins; b++) bin_free(&p.m[b]);
	free(p.m);
#ifdef TEST_FORK
end:
#endif
#if USE_PTHREADS
	if(pid > 0) {
		pthread_mutex_lock(&finish_mutex);
		st->flags = 1;
		pthread_mutex_unlock(&finish_mutex);
		pthread_cond_signal(&finish_cond);
	}
	return NULL;
#elif USE_SPROC
	return;
#else
	return NULL;
#endif
}

int
my_start_thread(struct thread_st *st)
{
#if USE_PTHREADS
	pthread_create(&st->id, NULL, malloc_test, st);
#elif USE_THR
	if(!st->sp)
		st->sp = malloc(STACKSIZE);
	if(!st->sp) return -1;
	thr_create(st->sp, STACKSIZE, malloc_test, st, THR_NEW_LWP, &st->id);
#elif USE_SPROC
	if(!st->sp)
		st->sp = malloc(STACKSIZE);
	if(!st->sp) return -1;
	st->id = sprocsp(malloc_test, PR_SALL, st, st->sp+STACKSIZE, STACKSIZE);
	if(st->id < 0) {
		return -1;
	}
#else /* NO_THREADS */
	st->id = 1;
	malloc_test(st);
#endif
	return 0;
}

int n_total=0, n_total_max=N_TOTAL, n_running;

int
my_end_thread(struct thread_st *st)
{
	/* Thread st has finished.  Start a new one. */
#if 0
	printf("Thread %lx terminated.\n", (long)st->id);
#endif
	if(n_total >= n_total_max) {
		n_running--;
	} else if(st->seed++, my_start_thread(st)) {
		printf("Creating thread #%d failed.\n", n_total);
	} else {
		n_total++;
		if(n_total%N_TOTAL_PRINT == 0)
			printf("n_total = %d\n", n_total);
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int i, bins;
	int n_thr=N_THREADS;
	int i_max=I_MAX;
	unsigned long size=SIZE;
	struct thread_st *st;

#if USE_MALLOC && !defined(MALLOC_HOOKS) && !defined(__GLIBC__)
	ptmalloc_init();
#endif
	if(argc > 1) n_total_max = atoi(argv[1]);
	if(n_total_max < 1) n_thr = 1;
	if(argc > 2) n_thr = atoi(argv[2]);
	if(n_thr < 1) n_thr = 1;
	if(n_thr > 100) n_thr = 100;
	if(argc > 3) i_max = atoi(argv[3]);

	if(argc > 4) size = atol(argv[4]);
	if(size < 2) size = 2;

	bins = MEMORY/(size*n_thr);
	if(argc > 5) bins = atoi(argv[5]);
	if(bins < 4) bins = 4;

#if USE_PTHREADS
	printf("Using posix threads.\n");
	pthread_cond_init(&finish_cond, NULL);
	pthread_mutex_init(&finish_mutex, NULL);
#elif USE_THR
	printf("Using Solaris threads.\n");
#elif USE_SPROC
	printf("Using sproc() threads.\n");
#else
	printf("No threads.\n");
#endif
	printf("total=%d threads=%d i_max=%d size=%ld bins=%d\n",
		   n_total_max, n_thr, i_max, size, bins);

	st = (struct thread_st *)malloc(n_thr*sizeof(*st));
	if(!st) exit(-1);

#if !defined NO_THREADS && defined __sun__
	/* I know of no other way to achieve proper concurrency with Solaris. */
	thr_setconcurrency(n_thr);
#endif

	/* Start all n_thr threads. */
	for(i=0; i<n_thr; i++) {
		st[i].bins = bins;
		st[i].max = i_max;
		st[i].size = size;
		st[i].flags = 0;
		st[i].sp = 0;
		st[i].seed = ((long)i_max*size + i) ^ bins;
		if(my_start_thread(&st[i])) {
			printf("Creating thread #%d failed.\n", i);
			n_thr = i;
			break;
		}
		printf("Created thread %lx.\n", (long)st[i].id);
	}

	for(n_running=n_total=n_thr; n_running>0;) {
#if USE_SPROC || USE_THR
		thread_id id;
#endif

		/* Wait for subthreads to finish. */
#if USE_PTHREADS
		pthread_mutex_lock(&finish_mutex);
		pthread_cond_wait(&finish_cond, &finish_mutex);
		for(i=0; i<n_thr; i++) if(st[i].flags) {
			pthread_join(st[i].id, NULL);
			st[i].flags = 0;
			my_end_thread(&st[i]);
		}
		pthread_mutex_unlock(&finish_mutex);
#elif USE_THR
		thr_join(0, &id, NULL);
		for(i=0; i<n_thr; i++)
			if(id == st[i].id) {
				my_end_thread(&st[i]);
				break;
			}
#elif USE_SPROC
		{
			int status = 0;
			id = wait(&status);
			if(status != 0) {
				if(WIFSIGNALED(status))
					printf("thread %id terminated by signal %d\n",
						   id, WTERMSIG(status));
				else
					printf("thread %id exited with status %d\n",
						   id, WEXITSTATUS(status));
			}
			for(i=0; i<n_thr; i++)
				if(id == st[i].id) {
					my_end_thread(&st[i]);
					break;
				}
		}
#else /* NO_THREADS */
		for(i=0; i<n_thr; i++)
			my_end_thread(&st[i]);
		break;
#endif
	}
	for(i=0; i<n_thr; i++) {
		if(st[i].sp)
			free(st[i].sp);
	}
	free(st);
#if USE_MALLOC
	malloc_stats();
#endif
	printf("Done.\n");
	return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * End:
 */
