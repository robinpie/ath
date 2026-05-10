/* ath_scope.h — lexical scope chain */
#ifndef ATH_SCOPE_H
#define ATH_SCOPE_H

#include "ath_value.h"

typedef struct AthBinding {
    const char *name;  /* interned C string literal or malloc'd — owned by entry */
    AthValue    value;
    int         is_const;
} AthBinding;

typedef struct AthScope {
    int           refcount;
    struct AthScope *parent;
    int           count;
    int           capacity;
    AthBinding   *bindings;
} AthScope;

AthScope *ath_scope_new(AthScope *parent);
void      ath_scope_incref(AthScope *s);
void      ath_scope_decref(AthScope *s);
void      ath_scope_define(AthScope *s, const char *name, AthValue v, int is_const);
AthValue  ath_scope_get(AthScope *s, const char *name);
void      ath_scope_set(AthScope *s, const char *name, AthValue v);
int       ath_scope_has_local(AthScope *s, const char *name);

#endif /* ATH_SCOPE_H */
