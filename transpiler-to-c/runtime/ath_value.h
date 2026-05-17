/* ath_value.h — tagged-union value type for the !~ATH runtime */
#ifndef ATH_VALUE_H
#define ATH_VALUE_H

#include <stddef.h>

/* Forward declarations */
struct AthString;
struct AthArray;
struct AthMap;
struct AthEntity;
struct AthRite;
struct AthScope;
struct AthCont;
struct AthSylladex;

typedef enum {
    ATH_VOID     = 0,
    ATH_BOOLEAN  = 1,
    ATH_INTEGER  = 2,
    ATH_FLOAT    = 3,
    ATH_STRING   = 4,
    ATH_ARRAY    = 5,
    ATH_MAP      = 6,
    ATH_ENTITY   = 7,
    ATH_RITE     = 8,
    ATH_MODULE   = 9, /* like MAP but TYPEOF returns "MODULE" */
    ATH_SYLLADEX = 10 /* sylladex (kind tag carried internally) */
} AthType;

typedef struct AthValue {
    AthType type;
    union {
        long             integer;
        double           float_;
        struct AthString *string;
        struct AthArray  *array;
        struct AthMap    *map;
        struct AthEntity *entity;
        struct AthRite   *rite;
        struct AthSylladex *sylladex;
    } as;
} AthValue;

/* ---- String ---- */
typedef struct AthString {
    int  refcount;
    int  length;  /* byte length, not codepoint count */
    char data[1]; /* flexible array (C89: [1] trick) */
} AthString;

AthString *ath_string_new(const char *data, int len);
AthString *ath_string_from_cstr(const char *s);
AthString *ath_string_concat(AthString *a, AthString *b);
AthString *ath_string_from_long(long v);
AthString *ath_string_from_double(double v);
void       ath_string_incref(AthString *s);
void       ath_string_decref(AthString *s);
const char *ath_string_cstr(AthString *s); /* returns s->data */

/* ---- Array ---- */
typedef struct AthArray {
    int       refcount;
    int       length;
    int       capacity;
    AthValue *data;
} AthArray;

AthArray *ath_array_new(int capacity);
AthArray *ath_array_copy(AthArray *src);
void      ath_array_free(AthArray *a);
void      ath_array_incref(AthArray *a);
void      ath_array_decref(AthArray *a);

/* ---- Map ---- */
typedef struct AthMapEntry {
    AthString *key;
    AthValue   value;
    int        used;
} AthMapEntry;

typedef struct AthMap {
    int          refcount;
    int          count;
    int          capacity;
    AthMapEntry *entries;
} AthMap;

AthMap   *ath_map_new(int capacity);
AthMap   *ath_map_copy(AthMap *src);
void      ath_map_free(AthMap *m);
void      ath_map_incref(AthMap *m);
void      ath_map_decref(AthMap *m);
AthValue  ath_map_get(AthMap *m, const char *key);
void      ath_map_set_str(AthMap *m, AthString *key, AthValue val);
void      ath_map_set(AthMap *m, const char *key, AthValue val);
void      ath_map_delete(AthMap *m, const char *key);
int       ath_map_has(AthMap *m, const char *key);
/* returns sorted array of AthString* keys (new array, caller owns) */
AthArray *ath_map_keys(AthMap *m);
AthArray *ath_map_values(AthMap *m);

/* ---- Rite (closure) ---- */
typedef AthValue (*AthRiteSyncFn)(struct AthScope *scope, int argc, AthValue *argv);
typedef void     (*AthRiteAsyncFn)(struct AthScope *scope, struct AthCont *k,
                                   int argc, AthValue *argv);

typedef struct AthRite {
    int           refcount;
    struct AthScope *closure;
    int           is_async;
    int           arity;  /* expected arg count, -1 = variadic */
    union {
        AthRiteSyncFn  sync;
        AthRiteAsyncFn async;
    } fn;
} AthRite;

AthRite *ath_rite_new_sync(struct AthScope *closure, AthRiteSyncFn fn, int arity);
AthRite *ath_rite_new_async(struct AthScope *closure, AthRiteAsyncFn fn, int arity);
void     ath_rite_incref(AthRite *r);
void     ath_rite_decref(AthRite *r);

/* ---- Value constructors ---- */
#define ATH_VOID_VAL    (ath_void())
#define ATH_TRUE_VAL    (ath_bool(1))
#define ATH_FALSE_VAL   (ath_bool(0))
#define ATH_INT_VAL(v)  (ath_int(v))

AthValue ath_void(void);
AthValue ath_bool(int b);
AthValue ath_int(long v);
AthValue ath_float(double v);
AthValue ath_str_val(AthString *s);
AthValue ath_str_cstr(const char *s);
AthValue ath_array_val(AthArray *a);
AthValue ath_map_val(AthMap *m);
AthValue ath_module_val(AthMap *m);
AthValue ath_entity_val(struct AthEntity *e);
AthValue ath_rite_val(AthRite *r);
AthValue ath_sylladex_val(struct AthSylladex *s);

/* ---- Refcount management ---- */
void ath_value_incref(AthValue v);
void ath_value_decref(AthValue v);
AthValue ath_value_copy(AthValue v); /* deep copy for mutable types */

/* ---- Operators ---- */
AthValue ath_add(AthValue a, AthValue b);
AthValue ath_sub(AthValue a, AthValue b);
AthValue ath_mul(AthValue a, AthValue b);
AthValue ath_div(AthValue a, AthValue b);
AthValue ath_mod(AthValue a, AthValue b);
AthValue ath_neg(AthValue a);
AthValue ath_band(AthValue a, AthValue b);
AthValue ath_bor(AthValue a, AthValue b);
AthValue ath_bxor(AthValue a, AthValue b);
AthValue ath_bnot(AthValue a);
AthValue ath_lshift(AthValue a, AthValue b);
AthValue ath_rshift(AthValue a, AthValue b);
AthValue ath_eq(AthValue a, AthValue b);
AthValue ath_ne(AthValue a, AthValue b);
AthValue ath_lt(AthValue a, AthValue b);
AthValue ath_gt(AthValue a, AthValue b);
AthValue ath_le(AthValue a, AthValue b);
AthValue ath_ge(AthValue a, AthValue b);
AthValue ath_index(AthValue obj, AthValue idx);
AthValue ath_member(AthValue obj, const char *member);
void     ath_index_set(AthValue obj, AthValue idx, AthValue val);
void     ath_member_set(AthValue obj, const char *member, AthValue val);

/* ---- Type checks / conversions ---- */
int      ath_is_truthy(AthValue v);
char    *ath_stringify(AthValue v);   /* malloc'd; caller frees */
const char *ath_typeof_str(AthValue v);

#endif /* ATH_VALUE_H */
