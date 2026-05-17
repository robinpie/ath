/* ath_sylladex.h — !~ATH 2.0 sylladex types (STACK/QUEUE/TREE/HASHMAP/
   OUIJA/BOTTLE/TECHHOP/JUJU). One AthSylladex struct with kind tag and
   per-kind payload union. */
#ifndef ATH_SYLLADEX_H
#define ATH_SYLLADEX_H

#include "ath_value.h"

typedef enum {
    ATH_SYL_STACK   = 0,
    ATH_SYL_QUEUE   = 1,
    ATH_SYL_TREE    = 2,
    ATH_SYL_HASHMAP = 3,
    ATH_SYL_OUIJA   = 4,
    ATH_SYL_BOTTLE  = 5,
    ATH_SYL_TECHHOP = 6,
    ATH_SYL_JUJU    = 7
} AthSylladexKind;

typedef enum {
    EJ_NONE         = 0,
    EJ_ROOT         = 1,
    EJ_LEAF         = 2,
    EJ_SLOT         = 3,
    EJ_GROOVE_SHADE = 4,
    EJ_KEY          = 5
} AthEjectMod;

typedef struct AthTreeNode {
    AthValue            value;
    int                 height; /* for AVL */
    struct AthTreeNode *left;
    struct AthTreeNode *right;
} AthTreeNode;

typedef struct AthSylladex {
    int             refcount;
    AthSylladexKind kind;
    union {
        struct { int size; AthValue *slots; } stack;
        struct { int size; AthValue *slots; } queue;
        struct {
            AthTreeNode *root;
            int          balance;   /* 1 = AVL rebalancing on insert */
            int          count;
        } tree;
        struct {
            int            size;
            struct AthString **keys;     /* per-slot key, or NULL */
            AthValue      *vals;
            struct AthRite *hash_rite;
        } hashmap;
        struct { int size; AthValue *slots; } ouija;
        struct {
            int             size;
            AthValue       *slots;
            unsigned char  *state;       /* 0 = empty, 1 = occupied, 2 = dead */
        } bottle;
        struct {
            int             grooves;
            int             shades;
            AthValue       *cells;       /* row-major, length grooves*shades */
            struct AthRite *groove_pred;
            struct AthRite *shade_pred;
        } techhop;
        struct {
            int                size;
            AthValue          *slots;
            unsigned char     *occupied; /* per-slot occupancy */
            struct AthEntity **writers;  /* per-slot writer branch (or NULL) */
            struct AthEntity **participants;
            int                p_count;
            int                p_cap;
            int                dead;
        } juju;
    } as;
} AthSylladex;

/* ---- Constructors ---- */
AthSylladex *ath_syl_stack_new(int n);
AthSylladex *ath_syl_queue_new(int n);
AthSylladex *ath_syl_tree_new(int balance);
AthSylladex *ath_syl_hashmap_new(int n, struct AthRite *hash_rite_or_NULL);
AthSylladex *ath_syl_ouija_new(int n);
AthSylladex *ath_syl_bottle_new(int n);
AthSylladex *ath_syl_techhop_new(int g, int s,
                                 struct AthRite *gp, struct AthRite *sp);
AthSylladex *ath_syl_juju_new(int n);

/* ---- Refcount ---- */
void ath_syl_incref(AthSylladex *s);
void ath_syl_decref(AthSylladex *s);

/* ---- Inspect ---- */
long  ath_syl_count(AthSylladex *s);
int   ath_syl_is_truthy(AthSylladex *s);
int   ath_syl_eq(AthSylladex *a, AthSylladex *b);
char *ath_syl_stringify(AthSylladex *s);              /* malloc'd */
const char *ath_syl_typeof_str(AthSylladex *s);

/* ---- Operations ----
   Per-kind modifier validity is enforced inside. Pass NULL for unused
   modifier args; the implementation will error if a required modifier is
   missing or a disallowed one is provided. */
void     ath_syl_captchalogue(AthSylladex *s,
                              AthValue value,
                              AthValue *with_key,    /* HASHMAP requires */
                              AthValue *slot_idx);   /* JUJU requires */

AthValue ath_syl_eject(AthSylladex *s,
                       AthEjectMod mod,
                       AthValue *a, AthValue *b);

#endif /* ATH_SYLLADEX_H */
