/* ath_eventloop.h — cooperative event loop: FIFO queue + timer min-heap */
#ifndef ATH_EVENTLOOP_H
#define ATH_EVENTLOOP_H

#include "ath_value.h"

/* ===== Continuation type ===== */
/* AthCont is the base for all continuation/frame structs.
   Every generated frame struct has AthCont as its first member. */

typedef void (*AthContFn)(struct AthCont *self, AthValue result);

typedef struct AthCont {
    AthContFn        resume;    /* function to call when this cont is ready */
    struct AthCont  *next;      /* caller's continuation (the "return address") */
    int              refcount;
} AthCont;

void ath_cont_resume(AthCont *k, AthValue result);
void ath_cont_incref(AthCont *k);
void ath_cont_decref(AthCont *k);

/* ===== Event loop API ===== */

void          ath_eventloop_init(void);
void          ath_eventloop_run(void);

/* Schedule a continuation to run immediately (FIFO order) */
void          ath_eventloop_schedule(AthCont *cont, AthValue result);

/* Schedule a continuation to run at a specific absolute millisecond time */
void          ath_eventloop_schedule_at(AthCont *cont, AthValue result,
                                         unsigned long abs_ms);

/* Schedule an entity to fire (die) when its deadline passes;
   used by timer entity creation */
void          ath_eventloop_schedule_at_entity(struct AthEntity *e);

/* Schedule an entity to die on the next event loop tick */
void          ath_eventloop_schedule_entity_die(struct AthEntity *e);

/* Wall clock in milliseconds */
unsigned long ath_eventloop_now_ms(void);

/* Register the program's THIS entity. When THIS dies, the event loop exits
   on its next iteration, regardless of pending timers. Per spec §"Program
   Termination": "The program terminates when THIS.DIE() is called". */
void          ath_eventloop_set_this(struct AthEntity *e);

#endif /* ATH_EVENTLOOP_H */
