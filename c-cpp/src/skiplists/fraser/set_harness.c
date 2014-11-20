/******************************************************************************
 * set_harness.c
 * 
 * Test harness for the various set implementations.
 * 
 * Copyright (c) 2002-2003, K A Fraser
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright 
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright 
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <ucontext.h>
#include <signal.h>
#include <sched.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>

#include "portable_defns.h"
#include "set.h"
#include "ptst.h"

/* This produces an operation log for the 'replay' checker. */
/*#define DO_WRITE_LOG*/

#ifdef DO_WRITE_LOG
#define MAX_ITERATIONS 100000
#define MAX_WALL_TIME 50 /* seconds */
#else
#define MAX_ITERATIONS 100000000
#define MAX_WALL_TIME 10 /* seconds */
#endif

/*
 * ***************** LOGGING
 */

#define MAX_LOG_RECORDS 256

#define LOG_KIND_INT 0
#define LOG_KIND_STRING 1
#define LOG_KIND_FLOAT 2

typedef struct {
    char *name;
    int kind;
    int val_int;
    char *val_string;
    float val_float;
} log_record_t;

static log_record_t log_records[MAX_LOG_RECORDS];

static int num_log_records = 0;

static void log_int (char *name, int val) {
    log_records[num_log_records].name = name;
    log_records[num_log_records].kind = LOG_KIND_INT;
    log_records[num_log_records].val_int = val;
    num_log_records ++;
}

static void log_string (char *name, char *val) {
    log_records[num_log_records].name = name;
    log_records[num_log_records].kind = LOG_KIND_STRING;
    log_records[num_log_records].val_string = val;
    num_log_records ++;
}

static void log_float (char *name, float val) {
    log_records[num_log_records].name = name;
    log_records[num_log_records].kind = LOG_KIND_FLOAT;
    log_records[num_log_records].val_float = val;
    num_log_records ++;
}

static void dump_log (void) {
    int i;

    fprintf (stdout, "-------------------------------------------"
             "---------------------------\n");
    for (i = 0; i < num_log_records; i ++)
    {
        char padding[40];
        strcpy(padding, "                                        ");
        if (30-strlen(log_records[i].name) >= 0){
            padding[30-strlen(log_records[i].name)] = '\0';
        }
        fprintf (stdout, "%s%s = ", padding, log_records[i].name);
        {
            int kind = log_records [i].kind;
            if (kind == LOG_KIND_INT) {
                fprintf (stdout, "%d\n", log_records[i].val_int);
            } else if (kind == LOG_KIND_STRING) {
                fprintf (stdout, "%s\n", log_records[i].val_string);
            } else if (kind == LOG_KIND_FLOAT) {
                fprintf (stdout, "%.3f\n", log_records[i].val_float);
            } 
        }
    }
    fprintf (stdout, "-------------------------------------------"
             "---------------------------\n");

    for (i = 0; i < num_log_records; i ++)
    {
        int kind = log_records [i].kind;
        if (i != 0) { fprintf (stderr, " "); }
        if (kind == LOG_KIND_INT) {
            fprintf (stderr, "%d", log_records[i].val_int);
        } else if (kind == LOG_KIND_STRING) {
            fprintf (stderr, "%s", log_records[i].val_string);
        } else if (kind == LOG_KIND_FLOAT) {
            fprintf (stderr, "%.3f", log_records[i].val_float);
        } 
    }
    fprintf (stderr, " LOG\n");
}

/*
 * ************** END OF LOGGING
 */

#define TVAL(x) ((x.tv_sec * 1000000) + x.tv_usec)

/* Log tables. Written out at end-of-day. */
typedef struct log_st
{
    interval_t    start, end;
    unsigned int  key;
    void         *val, *old_val; /* @old_val used by update() and remove() */
} log_t;
#define SIZEOF_GLOBAL_LOG (num_threads*MAX_ITERATIONS*sizeof(log_t))
static log_t *global_log;
static interval_t interval = 0;

static bool_t go = FALSE;
static int threads_initialised1 = 0, max_key, log_max_key;
static int threads_initialised2 = 0;
static int threads_initialised3 = 0;
int num_threads;

static unsigned long proportion;

static struct timeval start_time, done_time;
static struct tms start_tms, done_tms;

static int successes[MAX_THREADS];

#ifdef SPARC
static int processors[MAX_THREADS];
#endif

/* All the variables accessed in the critical main loop. */
static struct {
    CACHE_PAD(0);
    bool_t alarm_time;
    CACHE_PAD(1);
    set_t *set;
    CACHE_PAD(2);
} shared;

#define nrand(_r) (((_r) = (_r) * 1103515245) + 12345)

static void alarm_handler( int arg)
{
    shared.alarm_time = 1;
}

/*int cntr[MAX_THREADS] = { 0 };*/

static void *thread_start(void *arg)
{
    unsigned long k;
    int i;
    void *ov, *v;
    int id = (int)arg;
#ifdef DO_WRITE_LOG
    log_t *log = global_log + id*MAX_ITERATIONS;
    interval_t my_int;
#endif
    unsigned long r = ((unsigned long)arg)+3; /*RDTICK();*/
    unsigned int prop = proportion;
    unsigned int _max_key = max_key;

#ifdef SPARC
    i = processor_bind(P_LWPID, P_MYID, processors[id], NULL);
    if ( i != 0 )
    {
        printf("Failed to bind to processor %d! (%d)\n", processors[id], i);
        abort();
    }
#endif

    if ( id == 0 )
    {
        _init_ptst_subsystem();
        _init_gc_subsystem();
        _init_set_subsystem();
        shared.set = set_alloc();
    }

    /* BARRIER FOR ALL THREADS */
    {
        int n_id, id = threads_initialised1;
        while ( (n_id = CASIO(&threads_initialised1, id, id+1)) != id )
            id = n_id;
    }
    while ( threads_initialised1 != num_threads ) MB();

#ifndef DO_WRITE_LOG
    /* Start search structure off with a well-distributed set of inital keys */
    for ( i = (_max_key / num_threads); i != 0; i >>= 1 )
    {
        for ( k = i >> 1; k < (_max_key / num_threads); k += i )
        {
            set_update(shared.set, 
                       k + id * (_max_key / num_threads), 
                       (void *)0xdeadbee0, 1);
        }
    }
#endif

    {
        int n_id, id = threads_initialised2;
        while ( (n_id = CASIO(&threads_initialised2, id, id+1)) != id )
            id = n_id;
    }
    while ( threads_initialised2 != num_threads ) MB();

    if ( id == 0 )
    {
        (void)signal(SIGALRM, &alarm_handler);
        (void)alarm(MAX_WALL_TIME);
        WMB();
        gettimeofday(&start_time, NULL);
        times(&start_tms);
        go = TRUE;
        WMB();
    } 
    else 
    {
        while ( !go ) MB();
    }

#ifdef DO_WRITE_LOG
    get_interval(my_int);
#endif
    for ( i = 0; (i < MAX_ITERATIONS) && !shared.alarm_time; i++ )
    {
        /* O-3: ignore ; 4-11: proportion ; 12: ins/del */
        k = (nrand(r) >> 4) & (_max_key - 1);
        nrand(r);
#ifdef DO_WRITE_LOG
        log->start = my_int;
#endif
        if ( ((r>>4)&255) < prop )
        {
            ov = v = set_lookup(shared.set, k);
        }
        else if ( ((r>>12)&1) )
        {
            v = (void *)((r&~7)|0x8);
            ov = set_update(shared.set, k, v, 1);
        }
        else
        {
            v = NULL;
            ov = set_remove(shared.set, k);
        }

#ifdef DO_WRITE_LOG
        get_interval(my_int);
        log->key = k;
        log->val = v;
        log->old_val = ov;
        log->end = my_int;
        log++;
#endif
    }

    /* BARRIER FOR ALL THREADS */
    {
        int n_id, id = threads_initialised3;
        while ( (n_id = CASIO(&threads_initialised3, id, id+1)) != id ) 
            id = n_id;
    }
    while ( threads_initialised3 != num_threads ) MB();

#if 0
    if ( id == 0 )
    {
        extern void check_tree(set_t *);
        check_tree(shared.set);
    }
#endif

    if ( id == num_threads - 1 )
    {
        gettimeofday(&done_time, NULL);
        times(&done_tms);
        WMB();
        _destroy_gc_subsystem();
    } 

    successes[id] = i;
  
    return(NULL);
}

#define THREAD_TEST thread_start
#define THREAD_FLAGS THR_BOUND

#ifdef PPC
static pthread_attr_t attr;
#endif

static void test_multithreaded (void)
{
    int                 i, fd;
    pthread_t            thrs[MAX_THREADS];
    int num_successes;
    int min_successes, max_successes;
    int ticksps = sysconf(_SC_CLK_TCK);
    float wall_time, user_time, sys_time;

    if ( num_threads == 1 ) goto skip_thread_creation;

#ifdef PPC
    i = pthread_attr_init (&attr);
    if (i !=0) {
        fprintf (stderr, "URK!  pthread_attr_init rc=%d\n", i);
    }
    i = pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
    if (i !=0) {
        fprintf (stderr, "URK!  pthread_attr_setscope rc=%d\n", i);
    }
#endif

#ifdef MIPS
    pthread_setconcurrency(num_threads + 1);
#else
    pthread_setconcurrency(num_threads);
#endif

    for (i = 0; i < num_threads; i ++)
    {
        MB();
#ifdef PPC
        pthread_create (&thrs[i], &attr, THREAD_TEST, (void *)i);
#else
        pthread_create (&thrs[i], NULL, THREAD_TEST, (void *)i);
#endif
    }

 skip_thread_creation:
    if ( num_threads == 1 )
    {
        thread_start(0);
    }
    else
    {
        for (i = 0; i < num_threads; i ++)
        {
            (void)pthread_join (thrs[i], NULL);
        }
    }

    wall_time = (float)(TVAL(done_time) - TVAL(start_time))/ 1000000;
    user_time = ((float)(done_tms.tms_utime - start_tms.tms_utime))/ticksps;
    sys_time  = ((float)(done_tms.tms_stime - start_tms.tms_stime))/ticksps;

    log_float ("wall_time_s", wall_time);
    log_float ("user_time_s", user_time);
    log_float ("system_time_s", sys_time);

    num_successes = 0;
    min_successes = INT_MAX;
    max_successes = INT_MIN;
    for ( i = 0; i < num_threads; i++ )
    {
        num_successes += successes[i];
        if ( successes[i] < min_successes ) min_successes = successes[i];
        if ( successes[i] > max_successes ) max_successes = successes[i];
    }

    log_int ("min_successes", min_successes);
    log_int ("max_successes", max_successes);
    log_int ("num_successes", num_successes);

    log_float("us_per_success", (num_threads*wall_time*1000000.0)/num_successes);

    log_int("log max key", log_max_key);
}

#if defined(INTEL)
static void tstp_handler(int sig, siginfo_t *info, ucontext_t *uc)
{
    static unsigned int sem = 0;
    unsigned long *esp = (unsigned long *)(uc->uc_mcontext.gregs[7]);
    int pid = getpid();

    while ( CASIO(&sem, 0, 1) != 0 ) sched_yield();

    printf("Signal %d for pid %d\n", sig, pid);
    printf("%d: EIP=%08x  EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x\n", pid,
           uc->uc_mcontext.gregs[14], uc->uc_mcontext.gregs[11],
           uc->uc_mcontext.gregs[ 8], uc->uc_mcontext.gregs[10],
           uc->uc_mcontext.gregs[ 9]);
    printf("%d: ESP=%08x  EBP=%08x  ESI=%08x  EDI=%08x  EFL=%08x\n", pid,
           uc->uc_mcontext.gregs[ 7], uc->uc_mcontext.gregs[ 6],
           uc->uc_mcontext.gregs[ 5], uc->uc_mcontext.gregs[ 4],
           uc->uc_mcontext.gregs[16]);
    printf("\n");

    sem = 0;

    for ( ; ; ) sched_yield();
}
#endif

int main (int argc, char **argv)
{
#ifdef DO_WRITE_LOG
    int fd;
    unsigned long log_header[] = { 0, MAX_ITERATIONS, 0 };

    if ( argc != 5 )
    {
        printf("%s <num_threads> <read_proportion> <key power> <log name>\n"
               "(0 <= read_proportion <= 256)\n", argv[0]);
        exit(1);
    }
#else
    if ( argc != 4 )
    {
        printf("%s <num_threads> <read_proportion> <key power>\n"
               "(0 <= read_proportion <= 256)\n", argv[0]);
        exit(1);
    }
#endif

    memset(&shared, 0, sizeof(shared));

    num_threads = atoi(argv[1]);
    log_int ("num_threads", num_threads);

    proportion = atoi(argv[2]);
    log_float ("frac_reads", (float)proportion/256.0);

    log_max_key = atoi(argv[3]);
    max_key = 1 << atoi(argv[3]);
    log_int("max_key", max_key);

    log_int ("max_iterations", MAX_ITERATIONS);

    log_int ("wall_time_limit_s", MAX_WALL_TIME);

#ifdef SPARC
    {
        int st, maxcpu = sysconf(_SC_CPUID_MAX), i, j=0;

        /* Favour processors that don't handle I/O interrupts. */
        for ( i = 0; i <= maxcpu; i++ )
        {
            st = p_online(i, P_STATUS);
            if ( st == P_NOINTR )
            {
                if ( j == num_threads ) break;
                processors[j++] = i;
                if ( j == num_threads ) break;
            }
        }

        /* Fall back to the system quads if necessary. */
        for ( i = 0; i <= maxcpu; i++ )
        {
            st = p_online(i, P_STATUS);
            if ( st == P_ONLINE )
            {
                if ( j == num_threads ) break;
                processors[j++] = i;
                if ( j == num_threads ) break;
            }
        }

        if ( j != num_threads )
        {
            printf("Urk! Not enough CPUs for threads (%d < %d)\n", 
                   j, num_threads);
            abort();
        }
    }
#endif

#ifdef DO_WRITE_LOG
    log_header[0] = num_threads;
    log_header[2] = max_key;
    global_log = malloc(SIZEOF_GLOBAL_LOG);
#endif

#if defined(INTEL)
    {
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = (void *)tstp_handler;
        act.sa_flags = SA_SIGINFO;
        sigaction(SIGTSTP, &act, NULL);
        sigaction(SIGQUIT, &act, NULL);
        sigaction(SIGSEGV, &act, NULL);
    }
#endif

    test_multithreaded ();

    dump_log ();

#ifdef DO_WRITE_LOG
    printf("Writing log...\n");
    /* Write logs to data file */
    fd = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if ( fd == -1 )
    {
        fprintf(stderr, "Error writing log!\n");
        exit(-1);
    }

    if ( (write(fd, log_header, sizeof(log_header)) != sizeof(log_header)) ||
         (write(fd, global_log, SIZEOF_GLOBAL_LOG) != SIZEOF_GLOBAL_LOG) )
    {
        fprintf(stderr, "Log write truncated or erroneous\n");
        close(fd);
        exit(-1);
    }

    close(fd);
#endif

    exit(0);
}
