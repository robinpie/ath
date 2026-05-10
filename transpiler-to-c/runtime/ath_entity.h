/* ath_entity.h — entity lifecycle (timer, process, connection, watcher, branch, composite) */
#ifndef ATH_ENTITY_H
#define ATH_ENTITY_H

#include "ath_value.h"

/* forward */
struct AthCont;

typedef enum {
    ATH_ENTITY_THIS       = 0,
    ATH_ENTITY_TIMER      = 1,
    ATH_ENTITY_BRANCH     = 2,
    ATH_ENTITY_PROCESS    = 3,
    ATH_ENTITY_CONNECTION = 4,
    ATH_ENTITY_WATCHER    = 5,
    ATH_ENTITY_AND        = 6,
    ATH_ENTITY_OR         = 7,
    ATH_ENTITY_NOT        = 8
} AthEntityKind;

typedef struct AthWaiter {
    struct AthCont   *cont;
    struct AthWaiter *next;
} AthWaiter;

typedef struct AthEntity {
    int            refcount;
    AthEntityKind  kind;
    const char    *name;    /* owned by scope or static string */
    int            is_dead;
    AthWaiter     *waiters;
    /* timer */
    unsigned long  deadline_ms;
    /* process (POSIX) */
    int            pid;
    /* connection */
    int            sockfd;
    /* watcher */
    char          *filepath;
    unsigned long  next_poll_ms;
    /* composite */
    struct AthEntity *left;
    struct AthEntity *right;
    int             and_left_dead;  /* for AND: track which side died */
    int             and_right_dead;
} AthEntity;

AthEntity *ath_entity_this_new(void);
AthEntity *ath_entity_timer_new(const char *name, unsigned long ms);
AthEntity *ath_entity_branch_new(const char *name);
AthEntity *ath_entity_process_new(const char *name, const char *cmd, char *const argv[]);
AthEntity *ath_entity_connection_new(const char *name, const char *host, int port);
AthEntity *ath_entity_watcher_new(const char *name, const char *filepath);
AthEntity *ath_entity_and_new(AthEntity *a, AthEntity *b);
AthEntity *ath_entity_or_new(AthEntity *a, AthEntity *b);
AthEntity *ath_entity_not_new(AthEntity *inner);

void ath_entity_die(AthEntity *e);
void ath_entity_on_death(AthEntity *e, struct AthCont *k);
void ath_entity_incref(AthEntity *e);
void ath_entity_decref(AthEntity *e);

/* called by the event loop each tick to check process/connection/watcher */
void ath_entity_poll(AthEntity *e, unsigned long now_ms);

/* global list of entities needing polling (process/connection/watcher) */
void ath_entity_register_pollable(AthEntity *e);
void ath_entity_unregister_pollable(AthEntity *e);
void ath_entity_poll_all(unsigned long now_ms);

#endif /* ATH_ENTITY_H */
