/* ath_eventloop.c — cooperative event loop with FIFO queue and timer min-heap */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200112L
#endif
#include "ath_eventloop.h"
#include "ath_entity.h"
#include "ath_error.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
static unsigned long get_now_ms(void) { return (unsigned long)GetTickCount(); }
static void platform_sleep_ms(unsigned long ms) { Sleep((DWORD)ms); }
#else
#include <sys/time.h>
#include <time.h>
static unsigned long get_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
}
static void platform_sleep_ms(unsigned long ms) {
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000L);
    nanosleep(&ts, NULL);
}
#endif

unsigned long ath_eventloop_now_ms(void) { return get_now_ms(); }

/* ===== Continuation ===== */

void ath_cont_resume(AthCont *k, AthValue result) {
    if (k && k->resume) k->resume(k, result);
}

void ath_cont_incref(AthCont *k) { if (k) k->refcount++; }
void ath_cont_decref(AthCont *k) {
    /* Frames manage their own memory; this is a no-op for basic conts */
    (void)k;
}

/* ===== FIFO queue ===== */

typedef struct FifoTask {
    AthCont         *cont;
    AthValue         result;
    struct FifoTask *next;
} FifoTask;

static FifoTask *_fifo_head = NULL;
static FifoTask *_fifo_tail = NULL;

static void fifo_enqueue(AthCont *cont, AthValue result) {
    FifoTask *t = (FifoTask *)malloc(sizeof(FifoTask));
    if (!t) ath_fatal("out of memory");
    t->cont   = cont;
    t->result = result;
    t->next   = NULL;
    if (_fifo_tail) _fifo_tail->next = t;
    else            _fifo_head = t;
    _fifo_tail = t;
}

static int fifo_dequeue(AthCont **cont, AthValue *result) {
    FifoTask *t;
    if (!_fifo_head) return 0;
    t = _fifo_head;
    _fifo_head = t->next;
    if (!_fifo_head) _fifo_tail = NULL;
    *cont   = t->cont;
    *result = t->result;
    free(t);
    return 1;
}

static int fifo_empty(void) { return _fifo_head == NULL; }

/* ===== Timer min-heap ===== */

typedef struct TimerEntry {
    unsigned long abs_ms;
    AthCont      *cont;
    AthValue      result;
    int           is_entity;       /* if 1, entity_ptr is set */
    struct AthEntity *entity_ptr;
} TimerEntry;

#define HEAP_MAX 65536
static TimerEntry _heap[HEAP_MAX];
static int        _heap_size = 0;

static void heap_swap(int i, int j) {
    TimerEntry tmp = _heap[i];
    _heap[i] = _heap[j];
    _heap[j] = tmp;
}

static void heap_push(TimerEntry e) {
    int i;
    if (_heap_size >= HEAP_MAX) ath_fatal("timer heap overflow");
    i = _heap_size++;
    _heap[i] = e;
    while (i > 0) {
        int parent = (i-1)/2;
        if (_heap[parent].abs_ms <= _heap[i].abs_ms) break;
        heap_swap(i, parent);
        i = parent;
    }
}

static TimerEntry heap_pop(void) {
    int i = 0;
    TimerEntry top = _heap[0];
    _heap[0] = _heap[--_heap_size];
    for (;;) {
        int l = 2*i+1, r = 2*i+2, smallest = i;
        if (l < _heap_size && _heap[l].abs_ms < _heap[smallest].abs_ms) smallest=l;
        if (r < _heap_size && _heap[r].abs_ms < _heap[smallest].abs_ms) smallest=r;
        if (smallest == i) break;
        heap_swap(i, smallest);
        i = smallest;
    }
    return top;
}

static int heap_empty(void) { return _heap_size == 0; }

/* ===== Entity-die wrapper (for process/connection/watcher immediate die) ===== */

typedef struct EntityDieCont {
    AthCont       base;
    struct AthEntity *entity;
} EntityDieCont;

static void entity_die_cont_resume(AthCont *self, AthValue unused) {
    EntityDieCont *ec = (EntityDieCont *)self;
    (void)unused;
    ath_entity_die(ec->entity);
    free(ec);
}

void ath_eventloop_schedule_entity_die(struct AthEntity *e) {
    EntityDieCont *ec = (EntityDieCont *)malloc(sizeof(EntityDieCont));
    if (!ec) ath_fatal("out of memory");
    ec->base.resume   = entity_die_cont_resume;
    ec->base.next     = NULL;
    ec->base.refcount = 1;
    ec->entity = e;
    fifo_enqueue((AthCont*)ec, ath_void());
}

/* ===== Timer entity die ===== */

typedef struct TimerEntityCont {
    AthCont        base;
    struct AthEntity *entity;
} TimerEntityCont;

static void timer_entity_cont_resume(AthCont *self, AthValue unused) {
    TimerEntityCont *tc = (TimerEntityCont *)self;
    (void)unused;
    ath_entity_die(tc->entity);
    free(tc);
}

void ath_eventloop_schedule_at_entity(struct AthEntity *e) {
    TimerEntry entry;
    TimerEntityCont *tc = (TimerEntityCont *)malloc(sizeof(TimerEntityCont));
    if (!tc) ath_fatal("out of memory");
    tc->base.resume   = timer_entity_cont_resume;
    tc->base.next     = NULL;
    tc->base.refcount = 1;
    tc->entity = e;
    entry.abs_ms     = e->deadline_ms;
    entry.cont       = (AthCont*)tc;
    entry.result     = ath_void();
    entry.is_entity  = 0;
    entry.entity_ptr = NULL;
    heap_push(entry);
}

/* ===== Public schedule functions ===== */

void ath_eventloop_init(void) {
    _fifo_head  = NULL;
    _fifo_tail  = NULL;
    _heap_size  = 0;
}

void ath_eventloop_schedule(AthCont *cont, AthValue result) {
    fifo_enqueue(cont, result);
}

void ath_eventloop_schedule_at(AthCont *cont, AthValue result, unsigned long abs_ms) {
    TimerEntry entry;
    entry.abs_ms    = abs_ms;
    entry.cont      = cont;
    entry.result    = result;
    entry.is_entity = 0;
    entry.entity_ptr= NULL;
    heap_push(entry);
}

/* ===== Main event loop ===== */

void ath_eventloop_run(void) {
    for (;;) {
        /* 1. Drain FIFO (run-to-completion) */
        while (!fifo_empty()) {
            AthCont  *cont;
            AthValue  result;
            if (fifo_dequeue(&cont, &result))
                cont->resume(cont, result);
        }

        /* Check if anything remains */
        if (heap_empty()) break;

        /* 2. Poll process/connection/watcher entities */
        {
            unsigned long now = get_now_ms();
            ath_entity_poll_all(now);
        }

        /* Drain FIFO again after polling (entities may have died) */
        if (!fifo_empty()) continue;

        /* 3. Sleep until next timer deadline */
        if (!heap_empty()) {
            unsigned long now = get_now_ms();
            unsigned long next = _heap[0].abs_ms;
            if (next <= now) {
                /* Timer fired: move to FIFO */
                TimerEntry e = heap_pop();
                fifo_enqueue(e.cont, e.result);
            } else {
                unsigned long sleep_ms = next - now;
                /* Cap sleep at 100ms to allow polling */
                if (sleep_ms > 100) sleep_ms = 100;
                platform_sleep_ms(sleep_ms);
            }
        }
    }
}
