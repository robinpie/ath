/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 robinpie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* ath_sylladex.c -- implementation of !~ATH 2.0 sylladices. */
#include "ath_sylladex.h"
#include "ath_value.h"
#include "ath_entity.h"
#include "ath_error.h"
#include "ath_builtins.h"
#include "ath_eventloop.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===== Helpers ===== */

static AthSylladex *syl_alloc(AthSylladexKind kind) {
    AthSylladex *s = (AthSylladex *)calloc(1, sizeof(AthSylladex));
    if (!s) ath_fatal("out of memory");
    s->refcount = 1;
    s->kind = kind;
    return s;
}

static AthValue *alloc_void_slots(int n) {
    int i;
    AthValue *slots;
    if (n <= 0) return NULL;
    slots = (AthValue *)malloc(sizeof(AthValue) * (size_t)n);
    if (!slots) ath_fatal("out of memory");
    for (i = 0; i < n; i++) slots[i] = ath_void();
    return slots;
}

static void decref_slots(AthValue *slots, int n) {
    int i;
    if (!slots) return;
    for (i = 0; i < n; i++) ath_value_decref(slots[i]);
    free(slots);
}

/* ===== Constructors ===== */

AthSylladex *ath_syl_stack_new(int n) {
    AthSylladex *s;
    if (n < 0) ath_runtime_error("STACK size must be non-negative", 0, 0);
    s = syl_alloc(ATH_SYL_STACK);
    s->as.stack.size  = n;
    s->as.stack.slots = alloc_void_slots(n);
    return s;
}

AthSylladex *ath_syl_queue_new(int n) {
    AthSylladex *s;
    if (n < 0) ath_runtime_error("QUEUE size must be non-negative", 0, 0);
    s = syl_alloc(ATH_SYL_QUEUE);
    s->as.queue.size  = n;
    s->as.queue.slots = alloc_void_slots(n);
    return s;
}

AthSylladex *ath_syl_tree_new(int balance) {
    AthSylladex *s = syl_alloc(ATH_SYL_TREE);
    s->as.tree.root    = NULL;
    s->as.tree.balance = balance ? 1 : 0;
    s->as.tree.count   = 0;
    return s;
}

AthSylladex *ath_syl_hashmap_new(int n, struct AthRite *hash_rite_or_NULL) {
    AthSylladex *s;
    int i;
    if (n <= 0) ath_runtime_error("HASHMAP size must be positive", 0, 0);
    s = syl_alloc(ATH_SYL_HASHMAP);
    s->as.hashmap.size = n;
    s->as.hashmap.keys = (struct AthString **)calloc(n, sizeof(struct AthString *));
    s->as.hashmap.vals = (AthValue *)malloc(sizeof(AthValue) * n);
    if (!s->as.hashmap.keys || !s->as.hashmap.vals) ath_fatal("out of memory");
    for (i = 0; i < n; i++) s->as.hashmap.vals[i] = ath_void();
    s->as.hashmap.hash_rite = hash_rite_or_NULL;
    if (hash_rite_or_NULL) ath_rite_incref(hash_rite_or_NULL);
    return s;
}

AthSylladex *ath_syl_ouija_new(int n) {
    AthSylladex *s;
    if (n <= 0) ath_runtime_error("OUIJA size must be positive", 0, 0);
    s = syl_alloc(ATH_SYL_OUIJA);
    s->as.ouija.size  = n;
    s->as.ouija.slots = alloc_void_slots(n);
    return s;
}

AthSylladex *ath_syl_bottle_new(int n) {
    AthSylladex *s;
    if (n <= 0) ath_runtime_error("BOTTLE size must be positive", 0, 0);
    s = syl_alloc(ATH_SYL_BOTTLE);
    s->as.bottle.size  = n;
    s->as.bottle.slots = alloc_void_slots(n);
    s->as.bottle.state = (unsigned char *)calloc(n, 1); /* all empty */
    if (!s->as.bottle.state) ath_fatal("out of memory");
    return s;
}

AthSylladex *ath_syl_techhop_new(int g, int s_dim,
                                 struct AthRite *gp, struct AthRite *sp) {
    AthSylladex *s;
    int total, i;
    if (g <= 0 || s_dim <= 0)
        ath_runtime_error("TECHHOP dimensions must be positive", 0, 0);
    if (!gp || !sp)
        ath_runtime_error("TECHHOP requires two predicate rites", 0, 0);
    s = syl_alloc(ATH_SYL_TECHHOP);
    s->as.techhop.grooves = g;
    s->as.techhop.shades  = s_dim;
    total = g * s_dim;
    s->as.techhop.cells = (AthValue *)malloc(sizeof(AthValue) * total);
    if (!s->as.techhop.cells) ath_fatal("out of memory");
    for (i = 0; i < total; i++) s->as.techhop.cells[i] = ath_void();
    s->as.techhop.groove_pred = gp;
    s->as.techhop.shade_pred  = sp;
    ath_rite_incref(gp);
    ath_rite_incref(sp);
    return s;
}

AthSylladex *ath_syl_juju_new(int n) {
    AthSylladex *s;
    if (n <= 0) ath_runtime_error("JUJU size must be positive", 0, 0);
    s = syl_alloc(ATH_SYL_JUJU);
    s->as.juju.size     = n;
    s->as.juju.slots    = alloc_void_slots(n);
    s->as.juju.occupied = (unsigned char *)calloc(n, 1);
    s->as.juju.writers  = (struct AthEntity **)calloc(n, sizeof(struct AthEntity *));
    s->as.juju.participants = NULL;
    s->as.juju.p_count = 0;
    s->as.juju.p_cap   = 0;
    s->as.juju.dead    = 0;
    if (!s->as.juju.occupied || !s->as.juju.writers)
        ath_fatal("out of memory");
    return s;
}

/* ===== Tree internals (AVL) ===== */

static int tree_height(AthTreeNode *n) { return n ? n->height : 0; }

static int int_max(int a, int b) { return a > b ? a : b; }

static AthTreeNode *tree_node_new(AthValue v) {
    AthTreeNode *n = (AthTreeNode *)malloc(sizeof(AthTreeNode));
    if (!n) ath_fatal("out of memory");
    n->value = v;
    ath_value_incref(v);
    n->height = 1;
    n->left = NULL;
    n->right = NULL;
    return n;
}

static void tree_node_free(AthTreeNode *n) {
    if (!n) return;
    tree_node_free(n->left);
    tree_node_free(n->right);
    ath_value_decref(n->value);
    free(n);
}

static void tree_update_height(AthTreeNode *n) {
    n->height = 1 + int_max(tree_height(n->left), tree_height(n->right));
}

static AthTreeNode *tree_rotate_right(AthTreeNode *y) {
    AthTreeNode *x = y->left;
    AthTreeNode *t2 = x->right;
    x->right = y;
    y->left = t2;
    tree_update_height(y);
    tree_update_height(x);
    return x;
}

static AthTreeNode *tree_rotate_left(AthTreeNode *x) {
    AthTreeNode *y = x->right;
    AthTreeNode *t2 = y->left;
    y->left = x;
    x->right = t2;
    tree_update_height(x);
    tree_update_height(y);
    return y;
}

static int tree_balance_factor(AthTreeNode *n) {
    if (!n) return 0;
    return tree_height(n->left) - tree_height(n->right);
}

/* Returns < 0 if STRING(a) < STRING(b), 0 if equal, > 0 otherwise. */
static int tree_str_cmp(AthValue a, AthValue b) {
    char *sa = ath_stringify(a);
    char *sb = ath_stringify(b);
    int r = strcmp(sa, sb);
    free(sa);
    free(sb);
    return r;
}

static AthTreeNode *tree_insert(AthTreeNode *node, AthValue v, int balance) {
    int bf;
    if (!node) return tree_node_new(v);
    /* Equal-valued items go right per spec. */
    if (tree_str_cmp(v, node->value) < 0)
        node->left = tree_insert(node->left, v, balance);
    else
        node->right = tree_insert(node->right, v, balance);
    tree_update_height(node);
    if (!balance) return node;
    bf = tree_balance_factor(node);
    /* AVL rotations. */
    if (bf > 1 && tree_str_cmp(v, node->left->value) < 0)
        return tree_rotate_right(node);
    if (bf < -1 && tree_str_cmp(v, node->right->value) >= 0)
        return tree_rotate_left(node);
    if (bf > 1 && tree_str_cmp(v, node->left->value) >= 0) {
        node->left = tree_rotate_left(node->left);
        return tree_rotate_right(node);
    }
    if (bf < -1 && tree_str_cmp(v, node->right->value) < 0) {
        node->right = tree_rotate_right(node->right);
        return tree_rotate_left(node);
    }
    return node;
}

static void tree_inorder_collect(AthTreeNode *n, AthValue *out, int *idx) {
    if (!n) return;
    tree_inorder_collect(n->left, out, idx);
    out[*idx] = n->value;
    ath_value_incref(n->value);
    (*idx)++;
    tree_inorder_collect(n->right, out, idx);
}

/* Max-depth walk, returns leftmost leaf at max depth. */
static AthTreeNode *tree_find_leftmost_max_leaf(AthTreeNode *root,
                                                AthTreeNode **parent_out,
                                                int *is_left_child) {
    /* BFS to find deepest level; among deepest nodes, return leftmost leaf. */
    AthTreeNode *queue[1024];
    AthTreeNode *parents[1024];
    int        parent_dirs[1024]; /* 0=left,1=right,-1=root */
    int        depths[1024];
    int head = 0, tail = 0, max_depth = 0, max_idx = -1, i;
    queue[tail] = root;       parents[tail] = NULL;
    parent_dirs[tail] = -1;   depths[tail] = 1; tail++;
    while (head < tail) {
        AthTreeNode *n = queue[head];
        int d = depths[head];
        int is_leaf = (n->left == NULL && n->right == NULL);
        if (is_leaf && (d > max_depth || max_idx < 0)) {
            max_depth = d;
            max_idx = head;
        }
        if (n->left) {
            queue[tail] = n->left; parents[tail] = n;
            parent_dirs[tail] = 0; depths[tail] = d + 1; tail++;
        }
        if (n->right) {
            queue[tail] = n->right; parents[tail] = n;
            parent_dirs[tail] = 1; depths[tail] = d + 1; tail++;
        }
        head++;
    }
    if (max_idx < 0) {
        /* No leaf? Root itself is a leaf only if root && both children NULL,
           which was handled. Fall through to root case. */
        for (i = 0; i < tail; i++) {
            if (queue[i]->left == NULL && queue[i]->right == NULL) {
                max_idx = i; break;
            }
        }
    }
    if (max_idx < 0) return NULL;
    *parent_out = parents[max_idx];
    *is_left_child = parent_dirs[max_idx] == 0 ? 1 : 0;
    return queue[max_idx];
}

/* ===== Refcount / free ===== */

static void hashmap_free_contents(AthSylladex *s) {
    int i, n = s->as.hashmap.size;
    for (i = 0; i < n; i++) {
        if (s->as.hashmap.keys[i]) {
            ath_string_decref(s->as.hashmap.keys[i]);
            ath_value_decref(s->as.hashmap.vals[i]);
        }
    }
    free(s->as.hashmap.keys);
    free(s->as.hashmap.vals);
    if (s->as.hashmap.hash_rite) ath_rite_decref(s->as.hashmap.hash_rite);
}

static void juju_free_contents(AthSylladex *s) {
    int i, n = s->as.juju.size;
    for (i = 0; i < n; i++) {
        if (s->as.juju.occupied[i]) {
            ath_value_decref(s->as.juju.slots[i]);
            if (s->as.juju.writers[i]) ath_entity_decref(s->as.juju.writers[i]);
        }
    }
    free(s->as.juju.slots);
    free(s->as.juju.occupied);
    free(s->as.juju.writers);
    for (i = 0; i < s->as.juju.p_count; i++)
        ath_entity_decref(s->as.juju.participants[i]);
    free(s->as.juju.participants);
}

void ath_syl_incref(AthSylladex *s) {
    if (s) s->refcount++;
}

void ath_syl_decref(AthSylladex *s) {
    if (!s) return;
    if (--s->refcount > 0) return;
    switch (s->kind) {
    case ATH_SYL_STACK:  decref_slots(s->as.stack.slots, s->as.stack.size); break;
    case ATH_SYL_QUEUE:  decref_slots(s->as.queue.slots, s->as.queue.size); break;
    case ATH_SYL_TREE:   tree_node_free(s->as.tree.root); break;
    case ATH_SYL_HASHMAP: hashmap_free_contents(s); break;
    case ATH_SYL_OUIJA:  decref_slots(s->as.ouija.slots, s->as.ouija.size); break;
    case ATH_SYL_BOTTLE:
        decref_slots(s->as.bottle.slots, s->as.bottle.size);
        free(s->as.bottle.state);
        break;
    case ATH_SYL_TECHHOP:
        decref_slots(s->as.techhop.cells,
                     s->as.techhop.grooves * s->as.techhop.shades);
        if (s->as.techhop.groove_pred) ath_rite_decref(s->as.techhop.groove_pred);
        if (s->as.techhop.shade_pred)  ath_rite_decref(s->as.techhop.shade_pred);
        break;
    case ATH_SYL_JUJU:   juju_free_contents(s); break;
    }
    free(s);
}

/* ===== Inspect ===== */

const char *ath_syl_typeof_str(AthSylladex *s) {
    if (!s) return "SYLLADEX";
    switch (s->kind) {
    case ATH_SYL_STACK:   return "STACK";
    case ATH_SYL_QUEUE:   return "QUEUE";
    case ATH_SYL_TREE:    return "TREE";
    case ATH_SYL_HASHMAP: return "HASHMAP";
    case ATH_SYL_OUIJA:   return "OUIJA";
    case ATH_SYL_BOTTLE:  return "BOTTLE";
    case ATH_SYL_TECHHOP: return "TECHHOP";
    case ATH_SYL_JUJU:    return "JUJU";
    }
    return "SYLLADEX";
}

static int count_non_void(AthValue *slots, int n) {
    int i, c = 0;
    for (i = 0; i < n; i++) if (slots[i].type != ATH_VOID) c++;
    return c;
}

static int hashmap_occupied_count(AthSylladex *s) {
    int i, c = 0, n = s->as.hashmap.size;
    for (i = 0; i < n; i++) if (s->as.hashmap.keys[i]) c++;
    return c;
}

static int bottle_occupied_count(AthSylladex *s) {
    int i, c = 0, n = s->as.bottle.size;
    for (i = 0; i < n; i++) if (s->as.bottle.state[i] == 1) c++;
    return c;
}

static int juju_occupied_count(AthSylladex *s) {
    int i, c = 0, n = s->as.juju.size;
    for (i = 0; i < n; i++) if (s->as.juju.occupied[i]) c++;
    return c;
}

long ath_syl_count(AthSylladex *s) {
    if (!s) return 0;
    switch (s->kind) {
    case ATH_SYL_STACK:   return (long)count_non_void(s->as.stack.slots, s->as.stack.size);
    case ATH_SYL_QUEUE:   return (long)count_non_void(s->as.queue.slots, s->as.queue.size);
    case ATH_SYL_TREE:    return (long)s->as.tree.count;
    case ATH_SYL_HASHMAP: return (long)hashmap_occupied_count(s);
    case ATH_SYL_OUIJA:   return (long)count_non_void(s->as.ouija.slots, s->as.ouija.size);
    case ATH_SYL_BOTTLE:  return (long)bottle_occupied_count(s);
    case ATH_SYL_TECHHOP: return (long)count_non_void(s->as.techhop.cells,
                                       s->as.techhop.grooves * s->as.techhop.shades);
    case ATH_SYL_JUJU:    return s->as.juju.dead ? 0 : (long)juju_occupied_count(s);
    }
    return 0;
}

int ath_syl_is_truthy(AthSylladex *s) {
    if (!s) return 0;
    if (s->kind == ATH_SYL_JUJU && s->as.juju.dead) return 0;
    return ath_syl_count(s) > 0;
}

/* ===== Stringify ===== */

/* Render a value, but quote strings to match spec output. */
static char *stringify_quoted(AthValue v) {
    if (v.type == ATH_STRING) {
        int len = v.as.string->length;
        char *buf = (char *)malloc(len + 3);
        buf[0] = '"';
        memcpy(buf + 1, v.as.string->data, len);
        buf[len + 1] = '"';
        buf[len + 2] = '\0';
        return buf;
    }
    return ath_stringify(v);
}

/* Append src to *out at offset *off, growing *cap as needed. */
static void str_append(char **out, int *off, int *cap, const char *src) {
    int n = (int)strlen(src);
    while (*off + n + 1 > *cap) {
        *cap = (*cap) * 2;
        if (*cap < *off + n + 1) *cap = *off + n + 16;
        *out = (char *)realloc(*out, *cap);
        if (!*out) ath_fatal("out of memory");
    }
    memcpy(*out + *off, src, n);
    *off += n;
    (*out)[*off] = '\0';
}

static char *stringify_slots_quoted(const char *prefix,
                                    AthValue *slots, int n) {
    int cap = 64, off = 0, i;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    str_append(&out, &off, &cap, prefix);
    str_append(&out, &off, &cap, "[");
    for (i = 0; i < n; i++) {
        char *p;
        if (i > 0) str_append(&out, &off, &cap, ", ");
        p = stringify_quoted(slots[i]);
        str_append(&out, &off, &cap, p);
        free(p);
    }
    str_append(&out, &off, &cap, "]");
    return out;
}

static char *stringify_tree(AthSylladex *s) {
    int cap = 64, off = 0, i;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    if (!s->as.tree.root) {
        str_append(&out, &off, &cap, "TREE[]");
        return out;
    }
    /* Collect in-order. The bracketed list uses ath_stringify (unquoted),
       matching the existing array stringification behavior. */
    {
        AthValue *vals = (AthValue *)malloc(sizeof(AthValue) * s->as.tree.count);
        int idx = 0;
        char *root_str;
        tree_inorder_collect(s->as.tree.root, vals, &idx);
        root_str = stringify_quoted(s->as.tree.root->value);
        str_append(&out, &off, &cap, "TREE[");
        str_append(&out, &off, &cap, root_str);
        str_append(&out, &off, &cap, ": [");
        free(root_str);
        for (i = 0; i < idx; i++) {
            char *p;
            if (i > 0) str_append(&out, &off, &cap, ", ");
            p = ath_stringify(vals[i]);
            str_append(&out, &off, &cap, p);
            free(p);
            ath_value_decref(vals[i]);
        }
        free(vals);
        str_append(&out, &off, &cap, "]]");
    }
    return out;
}

static char *stringify_hashmap(AthSylladex *s) {
    int cap = 64, off = 0, i, n = s->as.hashmap.size;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    str_append(&out, &off, &cap, "HASHMAP[");
    for (i = 0; i < n; i++) {
        if (i > 0) str_append(&out, &off, &cap, ", ");
        if (!s->as.hashmap.keys[i]) {
            str_append(&out, &off, &cap, "VOID");
        } else {
            char *vp;
            str_append(&out, &off, &cap, "\"");
            {
                char *kbuf = (char *)malloc(s->as.hashmap.keys[i]->length + 1);
                memcpy(kbuf, s->as.hashmap.keys[i]->data,
                       s->as.hashmap.keys[i]->length);
                kbuf[s->as.hashmap.keys[i]->length] = '\0';
                str_append(&out, &off, &cap, kbuf);
                free(kbuf);
            }
            str_append(&out, &off, &cap, "\"->");
            vp = stringify_quoted(s->as.hashmap.vals[i]);
            str_append(&out, &off, &cap, vp);
            free(vp);
        }
    }
    str_append(&out, &off, &cap, "]");
    return out;
}

static char *stringify_bottle(AthSylladex *s) {
    int cap = 64, off = 0, i, n = s->as.bottle.size;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    str_append(&out, &off, &cap, "BOTTLE[");
    for (i = 0; i < n; i++) {
        if (i > 0) str_append(&out, &off, &cap, ", ");
        if (s->as.bottle.state[i] == 2) {
            str_append(&out, &off, &cap, "DEAD");
        } else if (s->as.bottle.state[i] == 1) {
            char *p = stringify_quoted(s->as.bottle.slots[i]);
            str_append(&out, &off, &cap, p);
            free(p);
        } else {
            str_append(&out, &off, &cap, "VOID");
        }
    }
    str_append(&out, &off, &cap, "]");
    return out;
}

static char *stringify_techhop(AthSylladex *s) {
    int cap = 64, off = 0, g, sh;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    str_append(&out, &off, &cap, "TECHHOP[");
    for (g = 0; g < s->as.techhop.grooves; g++) {
        if (g > 0) str_append(&out, &off, &cap, ", ");
        str_append(&out, &off, &cap, "[");
        for (sh = 0; sh < s->as.techhop.shades; sh++) {
            char *p;
            if (sh > 0) str_append(&out, &off, &cap, ", ");
            p = stringify_quoted(s->as.techhop.cells[g * s->as.techhop.shades + sh]);
            str_append(&out, &off, &cap, p);
            free(p);
        }
        str_append(&out, &off, &cap, "]");
    }
    str_append(&out, &off, &cap, "]");
    return out;
}

static char *stringify_juju(AthSylladex *s) {
    int cap = 64, off = 0, i, n = s->as.juju.size;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    str_append(&out, &off, &cap, "JUJU[");
    for (i = 0; i < n; i++) {
        if (i > 0) str_append(&out, &off, &cap, ", ");
        if (!s->as.juju.occupied[i]) {
            str_append(&out, &off, &cap, "VOID");
        } else {
            char *p = stringify_quoted(s->as.juju.slots[i]);
            str_append(&out, &off, &cap, p);
            if (s->as.juju.writers[i] && s->as.juju.writers[i]->name) {
                str_append(&out, &off, &cap, "<-#");
                str_append(&out, &off, &cap, s->as.juju.writers[i]->name);
            }
            free(p);
        }
    }
    str_append(&out, &off, &cap, "]");
    return out;
}

char *ath_syl_stringify(AthSylladex *s) {
    if (!s) {
        char *o = (char *)malloc(11);
        strcpy(o, "SYLLADEX[]");
        return o;
    }
    switch (s->kind) {
    case ATH_SYL_STACK:   return stringify_slots_quoted("STACK", s->as.stack.slots, s->as.stack.size);
    case ATH_SYL_QUEUE:   return stringify_slots_quoted("QUEUE", s->as.queue.slots, s->as.queue.size);
    case ATH_SYL_TREE:    return stringify_tree(s);
    case ATH_SYL_HASHMAP: return stringify_hashmap(s);
    case ATH_SYL_OUIJA:   return stringify_slots_quoted("OUIJA", s->as.ouija.slots, s->as.ouija.size);
    case ATH_SYL_BOTTLE:  return stringify_bottle(s);
    case ATH_SYL_TECHHOP: return stringify_techhop(s);
    case ATH_SYL_JUJU:    return stringify_juju(s);
    }
    {
        char *o = (char *)malloc(11);
        strcpy(o, "SYLLADEX[]");
        return o;
    }
}

/* ===== Equality (structural) ===== */

static int slots_eq(AthValue *a, AthValue *b, int n) {
    int i;
    for (i = 0; i < n; i++) {
        AthValue r = ath_eq(a[i], b[i]);
        if (!r.as.integer) return 0;
    }
    return 1;
}

static int tree_eq(AthTreeNode *a, AthTreeNode *b) {
    AthValue r;
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    r = ath_eq(a->value, b->value);
    if (!r.as.integer) return 0;
    return tree_eq(a->left, b->left) && tree_eq(a->right, b->right);
}

int ath_syl_eq(AthSylladex *a, AthSylladex *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    switch (a->kind) {
    case ATH_SYL_STACK:
        if (a->as.stack.size != b->as.stack.size) return 0;
        return slots_eq(a->as.stack.slots, b->as.stack.slots, a->as.stack.size);
    case ATH_SYL_QUEUE:
        if (a->as.queue.size != b->as.queue.size) return 0;
        return slots_eq(a->as.queue.slots, b->as.queue.slots, a->as.queue.size);
    case ATH_SYL_TREE:
        if (a->as.tree.count != b->as.tree.count) return 0;
        return tree_eq(a->as.tree.root, b->as.tree.root);
    case ATH_SYL_HASHMAP: {
        int i, n = a->as.hashmap.size;
        if (n != b->as.hashmap.size) return 0;
        if (a->as.hashmap.hash_rite != b->as.hashmap.hash_rite) return 0;
        for (i = 0; i < n; i++) {
            AthString *ka = a->as.hashmap.keys[i];
            AthString *kb = b->as.hashmap.keys[i];
            if (!ka && !kb) continue;
            if (!ka || !kb) return 0;
            if (ka->length != kb->length) return 0;
            if (memcmp(ka->data, kb->data, ka->length) != 0) return 0;
            {
                AthValue r = ath_eq(a->as.hashmap.vals[i], b->as.hashmap.vals[i]);
                if (!r.as.integer) return 0;
            }
        }
        return 1;
    }
    case ATH_SYL_OUIJA:
        if (a->as.ouija.size != b->as.ouija.size) return 0;
        return slots_eq(a->as.ouija.slots, b->as.ouija.slots, a->as.ouija.size);
    case ATH_SYL_BOTTLE: {
        int i, n = a->as.bottle.size;
        if (n != b->as.bottle.size) return 0;
        for (i = 0; i < n; i++) {
            if (a->as.bottle.state[i] != b->as.bottle.state[i]) return 0;
            if (a->as.bottle.state[i] == 1) {
                AthValue r = ath_eq(a->as.bottle.slots[i], b->as.bottle.slots[i]);
                if (!r.as.integer) return 0;
            }
        }
        return 1;
    }
    case ATH_SYL_TECHHOP: {
        int n;
        if (a->as.techhop.grooves != b->as.techhop.grooves) return 0;
        if (a->as.techhop.shades  != b->as.techhop.shades) return 0;
        if (a->as.techhop.groove_pred != b->as.techhop.groove_pred) return 0;
        if (a->as.techhop.shade_pred  != b->as.techhop.shade_pred) return 0;
        n = a->as.techhop.grooves * a->as.techhop.shades;
        return slots_eq(a->as.techhop.cells, b->as.techhop.cells, n);
    }
    case ATH_SYL_JUJU: {
        int i, n = a->as.juju.size;
        if (n != b->as.juju.size) return 0;
        if (a->as.juju.dead != b->as.juju.dead) return 0;
        if (a->as.juju.p_count != b->as.juju.p_count) return 0;
        for (i = 0; i < a->as.juju.p_count; i++)
            if (a->as.juju.participants[i] != b->as.juju.participants[i]) return 0;
        for (i = 0; i < n; i++) {
            if (a->as.juju.occupied[i] != b->as.juju.occupied[i]) return 0;
            if (a->as.juju.writers[i] != b->as.juju.writers[i]) return 0;
            if (a->as.juju.occupied[i]) {
                AthValue r = ath_eq(a->as.juju.slots[i], b->as.juju.slots[i]);
                if (!r.as.integer) return 0;
            }
        }
        return 1;
    }
    }
    return 0;
}

/* ===== Default hash (sum of UTF-8 bytes of the string-coerced key) ===== */

static long default_hash_str(const char *data, int len) {
    long h = 0;
    int i;
    for (i = 0; i < len; i++) h += (unsigned char)data[i];
    return h;
}

static int hashmap_index_for_key(AthSylladex *s, AthValue key) {
    char *kstr = ath_stringify(key);
    int klen = (int)strlen(kstr);
    long h;
    int idx;
    if (s->as.hashmap.hash_rite) {
        AthValue arg = ath_str_cstr(kstr);
        AthValue result;
        free(kstr);
        result = ath_call_sync(NULL, ath_rite_val(s->as.hashmap.hash_rite),
                               1, &arg);
        ath_value_decref(arg);
        if (result.type != ATH_INTEGER)
            ath_runtime_error("HASHMAP hash rite must return INTEGER", 0, 0);
        h = result.as.integer;
        ath_value_decref(result);
    } else {
        h = default_hash_str(kstr, klen);
        free(kstr);
    }
    if (h < 0) h = -h;
    idx = (int)(h % s->as.hashmap.size);
    return idx;
}

/* ===== JUJU participant tracking ===== */

static AthEntity *current_branch_or_error(const char *op) {
    AthEntity *cb = ath_eventloop_get_current_branch();
    if (!cb) ath_runtime_error_fmt("%s on JUJU requires a branch context", op);
    return cb;
}

static void juju_add_participant(AthSylladex *s, AthEntity *br) {
    int i;
    for (i = 0; i < s->as.juju.p_count; i++)
        if (s->as.juju.participants[i] == br) return;
    if (s->as.juju.p_count >= s->as.juju.p_cap) {
        int new_cap = s->as.juju.p_cap ? s->as.juju.p_cap * 2 : 4;
        s->as.juju.participants = (struct AthEntity **)realloc(
            s->as.juju.participants, sizeof(struct AthEntity *) * new_cap);
        if (!s->as.juju.participants) ath_fatal("out of memory");
        s->as.juju.p_cap = new_cap;
    }
    s->as.juju.participants[s->as.juju.p_count++] = br;
    ath_entity_incref(br);
}

static void juju_check_alive(AthSylladex *s) {
    int i, alive = 0;
    for (i = 0; i < s->as.juju.p_count; i++)
        if (!s->as.juju.participants[i]->is_dead) alive++;
    /* A JUJU with zero or one participants is alive but inert. It dies only
       when it had multiple participants and now has <2 alive. */
    if (s->as.juju.p_count >= 2 && alive < 2) s->as.juju.dead = 1;
}

/* ===== CAPTCHALOGUE ===== */

static int int_arg(AthValue v, const char *what) {
    if (v.type != ATH_INTEGER)
        ath_runtime_error_fmt("%s must be INTEGER, got %s",
                              what, ath_typeof_str(v));
    return (int)v.as.integer;
}

static void cap_stack(AthSylladex *s, AthValue value) {
    int i, n = s->as.stack.size;
    if (n == 0) return; /* spec: discard */
    ath_value_decref(s->as.stack.slots[n - 1]);
    for (i = n - 1; i > 0; i--) s->as.stack.slots[i] = s->as.stack.slots[i - 1];
    s->as.stack.slots[0] = value;
    ath_value_incref(value);
}

static void cap_queue(AthSylladex *s, AthValue value) {
    int i, n = s->as.queue.size;
    if (n == 0) return;
    ath_value_decref(s->as.queue.slots[n - 1]);
    for (i = n - 1; i > 0; i--) s->as.queue.slots[i] = s->as.queue.slots[i - 1];
    s->as.queue.slots[0] = value;
    ath_value_incref(value);
}

static void cap_tree(AthSylladex *s, AthValue value) {
    s->as.tree.root = tree_insert(s->as.tree.root, value, s->as.tree.balance);
    s->as.tree.count++;
}

static void cap_hashmap(AthSylladex *s, AthValue value, AthValue key) {
    int idx = hashmap_index_for_key(s, key);
    char *kstr = ath_stringify(key);
    AthString *ks = ath_string_from_cstr(kstr);
    free(kstr);
    if (s->as.hashmap.keys[idx]) {
        ath_string_decref(s->as.hashmap.keys[idx]);
        ath_value_decref(s->as.hashmap.vals[idx]);
    }
    s->as.hashmap.keys[idx] = ks;
    s->as.hashmap.vals[idx] = value;
    ath_value_incref(value);
}

static void cap_ouija(AthSylladex *s, AthValue value) {
    int idx = rand() % s->as.ouija.size;
    ath_value_decref(s->as.ouija.slots[idx]);
    s->as.ouija.slots[idx] = value;
    ath_value_incref(value);
}

static void cap_bottle(AthSylladex *s, AthValue value) {
    int i, n = s->as.bottle.size;
    for (i = 0; i < n; i++) {
        if (s->as.bottle.state[i] == 0) {
            s->as.bottle.slots[i] = value;
            ath_value_incref(value);
            s->as.bottle.state[i] = 1;
            return;
        }
    }
    /* No empty slot -- discard. */
}

static void cap_techhop(AthSylladex *s, AthValue value) {
    int g, sh, G = s->as.techhop.grooves, S = s->as.techhop.shades;
    /* Compute valid grooves. */
    for (g = 0; g < G; g++) {
        AthValue argv[2];
        AthValue ok_g;
        int gp_ok;
        argv[0] = value;
        argv[1] = ath_int(g);
        ok_g = ath_call_sync(NULL, ath_rite_val(s->as.techhop.groove_pred),
                             2, argv);
        gp_ok = ath_is_truthy(ok_g);
        ath_value_decref(ok_g);
        ath_value_decref(argv[1]);
        if (!gp_ok) continue;
        for (sh = 0; sh < S; sh++) {
            AthValue sarg[2];
            AthValue ok_s;
            int sp_ok;
            int cell;
            sarg[0] = value;
            sarg[1] = ath_int(sh);
            ok_s = ath_call_sync(NULL, ath_rite_val(s->as.techhop.shade_pred),
                                 2, sarg);
            sp_ok = ath_is_truthy(ok_s);
            ath_value_decref(ok_s);
            ath_value_decref(sarg[1]);
            if (!sp_ok) continue;
            cell = g * S + sh;
            if (s->as.techhop.cells[cell].type == ATH_VOID) {
                s->as.techhop.cells[cell] = value;
                ath_value_incref(value);
                return;
            }
        }
    }
    /* No valid empty cell -- discard. */
}

static void cap_juju(AthSylladex *s, AthValue value, AthValue slot_idx) {
    int n;
    AthEntity *cb;
    cb = current_branch_or_error("CAPTCHALOGUE");
    if (s->as.juju.dead)
        ath_runtime_error("operation on dead JUJU", 0, 0);
    n = int_arg(slot_idx, "JUJU slot");
    if (n < 0 || n >= s->as.juju.size)
        ath_runtime_error("JUJU slot out of range", 0, 0);
    if (s->as.juju.occupied[n])
        ath_runtime_error("JUJU slot already occupied", 0, 0);
    juju_add_participant(s, cb);
    s->as.juju.slots[n] = value;
    ath_value_incref(value);
    s->as.juju.occupied[n] = 1;
    s->as.juju.writers[n] = cb;
    ath_entity_incref(cb);
    juju_check_alive(s);
}

void ath_syl_captchalogue(AthSylladex *s, AthValue value,
                          AthValue *with_key, AthValue *slot_idx) {
    if (!s) ath_runtime_error("CAPTCHALOGUE target is not a sylladex", 0, 0);
    switch (s->kind) {
    case ATH_SYL_STACK:
        if (with_key || slot_idx)
            ath_runtime_error("STACK CAPTCHALOGUE takes no modifier", 0, 0);
        cap_stack(s, value); return;
    case ATH_SYL_QUEUE:
        if (with_key || slot_idx)
            ath_runtime_error("QUEUE CAPTCHALOGUE takes no modifier", 0, 0);
        cap_queue(s, value); return;
    case ATH_SYL_TREE:
        if (with_key || slot_idx)
            ath_runtime_error("TREE CAPTCHALOGUE takes no modifier", 0, 0);
        cap_tree(s, value); return;
    case ATH_SYL_HASHMAP:
        if (!with_key)
            ath_runtime_error("HASHMAP CAPTCHALOGUE requires WITH key", 0, 0);
        if (slot_idx)
            ath_runtime_error("HASHMAP CAPTCHALOGUE does not take SLOT", 0, 0);
        cap_hashmap(s, value, *with_key); return;
    case ATH_SYL_OUIJA:
        if (with_key || slot_idx)
            ath_runtime_error("OUIJA CAPTCHALOGUE takes no modifier", 0, 0);
        cap_ouija(s, value); return;
    case ATH_SYL_BOTTLE:
        if (with_key || slot_idx)
            ath_runtime_error("BOTTLE CAPTCHALOGUE takes no modifier", 0, 0);
        cap_bottle(s, value); return;
    case ATH_SYL_TECHHOP:
        if (with_key || slot_idx)
            ath_runtime_error("TECHHOP CAPTCHALOGUE takes no modifier", 0, 0);
        cap_techhop(s, value); return;
    case ATH_SYL_JUJU:
        if (with_key)
            ath_runtime_error("JUJU CAPTCHALOGUE does not take WITH", 0, 0);
        if (!slot_idx)
            ath_runtime_error("JUJU CAPTCHALOGUE requires SLOT", 0, 0);
        cap_juju(s, value, *slot_idx); return;
    }
    ath_runtime_error("CAPTCHALOGUE: unknown sylladex kind", 0, 0);
}

/* ===== EJECT ===== */

static AthValue eject_stack(AthSylladex *s) {
    AthValue v;
    int i, n = s->as.stack.size;
    if (n == 0) return ath_void();
    v = s->as.stack.slots[0];
    for (i = 0; i < n - 1; i++) s->as.stack.slots[i] = s->as.stack.slots[i + 1];
    s->as.stack.slots[n - 1] = ath_void();
    return v; /* caller owns */
}

static AthValue eject_queue(AthSylladex *s) {
    AthValue v;
    int i, n = s->as.queue.size;
    if (n == 0) return ath_void();
    v = s->as.queue.slots[n - 1];
    for (i = n - 1; i > 0; i--) s->as.queue.slots[i] = s->as.queue.slots[i - 1];
    s->as.queue.slots[0] = ath_void();
    return v;
}

static AthValue eject_ouija(AthSylladex *s) {
    int idx = rand() % s->as.ouija.size;
    AthValue v = s->as.ouija.slots[idx];
    s->as.ouija.slots[idx] = ath_void();
    return v;
}

static AthValue eject_tree_root(AthSylladex *s) {
    AthArray *a;
    int idx = 0;
    int total = s->as.tree.count;
    AthValue v;
    a = ath_array_new(total < 1 ? 1 : total);
    if (s->as.tree.root) {
        AthValue *tmp = (AthValue *)malloc(sizeof(AthValue) * total);
        int i;
        tree_inorder_collect(s->as.tree.root, tmp, &idx);
        /* tmp has incref'd values; transfer into array. */
        for (i = 0; i < idx; i++) {
            if (a->length >= a->capacity) {
                a->capacity *= 2;
                a->data = (AthValue *)realloc(a->data,
                                              sizeof(AthValue) * a->capacity);
                if (!a->data) ath_fatal("out of memory");
            }
            a->data[a->length++] = tmp[i];
        }
        free(tmp);
        tree_node_free(s->as.tree.root);
        s->as.tree.root = NULL;
        s->as.tree.count = 0;
    }
    v = ath_array_val(a);
    return v;
}

static AthValue eject_tree_leaf(AthSylladex *s) {
    AthTreeNode *parent = NULL, *leaf;
    int is_left = 0;
    AthValue v;
    if (!s->as.tree.root) return ath_void();
    leaf = tree_find_leftmost_max_leaf(s->as.tree.root, &parent, &is_left);
    if (!leaf) return ath_void();
    v = leaf->value;
    ath_value_incref(v);
    if (!parent) {
        /* root is the leaf */
        tree_node_free(s->as.tree.root);
        s->as.tree.root = NULL;
    } else {
        if (is_left) parent->left = NULL; else parent->right = NULL;
        ath_value_decref(leaf->value);
        free(leaf);
    }
    s->as.tree.count--;
    return v;
}

static AthValue eject_hashmap_key(AthSylladex *s, AthValue key) {
    int idx = hashmap_index_for_key(s, key);
    char *kstr;
    AthString *stored;
    AthValue v;
    if (!s->as.hashmap.keys[idx]) return ath_void();
    stored = s->as.hashmap.keys[idx];
    kstr = ath_stringify(key);
    if ((int)strlen(kstr) == stored->length &&
        memcmp(kstr, stored->data, stored->length) == 0) {
        free(kstr);
        v = s->as.hashmap.vals[idx];
        ath_string_decref(stored);
        s->as.hashmap.keys[idx] = NULL;
        s->as.hashmap.vals[idx] = ath_void();
        return v;
    }
    free(kstr);
    return ath_void();
}

static AthValue eject_hashmap_slot(AthSylladex *s, AthValue slot) {
    int n = int_arg(slot, "HASHMAP slot");
    AthValue v;
    if (n < 0 || n >= s->as.hashmap.size)
        ath_runtime_error("HASHMAP slot out of range", 0, 0);
    if (!s->as.hashmap.keys[n]) return ath_void();
    ath_string_decref(s->as.hashmap.keys[n]);
    s->as.hashmap.keys[n] = NULL;
    v = s->as.hashmap.vals[n];
    s->as.hashmap.vals[n] = ath_void();
    return v;
}

static AthValue eject_bottle_lowest(AthSylladex *s) {
    int i, n = s->as.bottle.size;
    AthValue v;
    for (i = 0; i < n; i++) {
        if (s->as.bottle.state[i] != 2) {
            if (s->as.bottle.state[i] == 1) {
                v = s->as.bottle.slots[i];
                s->as.bottle.slots[i] = ath_void();
            } else {
                v = ath_void();
            }
            s->as.bottle.state[i] = 2;
            return v;
        }
    }
    ath_runtime_error("BOTTLE has no usable slots", 0, 0);
    return ath_void();
}

static AthValue eject_bottle_slot(AthSylladex *s, AthValue slot) {
    int n = int_arg(slot, "BOTTLE slot");
    AthValue v;
    if (n < 0 || n >= s->as.bottle.size)
        ath_runtime_error("BOTTLE slot out of range", 0, 0);
    if (s->as.bottle.state[n] == 2)
        ath_runtime_error("BOTTLE slot is dead", 0, 0);
    if (s->as.bottle.state[n] == 1) {
        v = s->as.bottle.slots[n];
        s->as.bottle.slots[n] = ath_void();
    } else {
        v = ath_void();
    }
    s->as.bottle.state[n] = 2;
    return v;
}

static AthValue eject_techhop(AthSylladex *s, AthValue groove, AthValue shade) {
    int g = int_arg(groove, "TECHHOP groove");
    int sh = int_arg(shade, "TECHHOP shade");
    int idx;
    AthValue v;
    if (g < 0 || g >= s->as.techhop.grooves)
        ath_runtime_error("TECHHOP groove out of range", 0, 0);
    if (sh < 0 || sh >= s->as.techhop.shades)
        ath_runtime_error("TECHHOP shade out of range", 0, 0);
    idx = g * s->as.techhop.shades + sh;
    v = s->as.techhop.cells[idx];
    s->as.techhop.cells[idx] = ath_void();
    return v;
}

static AthValue eject_juju(AthSylladex *s, AthValue slot) {
    int n;
    AthValue v;
    AthEntity *cb = current_branch_or_error("EJECT");
    if (s->as.juju.dead)
        ath_runtime_error("operation on dead JUJU", 0, 0);
    n = int_arg(slot, "JUJU slot");
    if (n < 0 || n >= s->as.juju.size)
        ath_runtime_error("JUJU slot out of range", 0, 0);
    juju_add_participant(s, cb);
    if (!s->as.juju.occupied[n]) {
        juju_check_alive(s);
        return ath_void();
    }
    if (s->as.juju.writers[n] == cb)
        ath_runtime_error("JUJU: branch cannot read slot it wrote", 0, 0);
    v = s->as.juju.slots[n];
    s->as.juju.slots[n] = ath_void();
    s->as.juju.occupied[n] = 0;
    if (s->as.juju.writers[n]) ath_entity_decref(s->as.juju.writers[n]);
    s->as.juju.writers[n] = NULL;
    juju_check_alive(s);
    return v;
}

AthValue ath_syl_eject(AthSylladex *s, AthEjectMod mod,
                       AthValue *a, AthValue *b) {
    if (!s) ath_runtime_error("EJECT target is not a sylladex", 0, 0);
    switch (s->kind) {
    case ATH_SYL_STACK:
        if (mod != EJ_NONE)
            ath_runtime_error("STACK EJECT takes no modifier", 0, 0);
        return eject_stack(s);
    case ATH_SYL_QUEUE:
        if (mod != EJ_NONE)
            ath_runtime_error("QUEUE EJECT takes no modifier", 0, 0);
        return eject_queue(s);
    case ATH_SYL_OUIJA:
        if (mod != EJ_NONE)
            ath_runtime_error("OUIJA EJECT takes no modifier", 0, 0);
        return eject_ouija(s);
    case ATH_SYL_TREE:
        if (mod == EJ_ROOT) return eject_tree_root(s);
        if (mod == EJ_LEAF) return eject_tree_leaf(s);
        ath_runtime_error("TREE EJECT requires ROOT or LEAF", 0, 0);
        return ath_void();
    case ATH_SYL_HASHMAP:
        if (mod == EJ_SLOT) {
            if (!a) ath_runtime_error("HASHMAP SLOT requires index", 0, 0);
            return eject_hashmap_slot(s, *a);
        }
        if (mod == EJ_KEY) {
            if (!a) ath_runtime_error("HASHMAP key required", 0, 0);
            return eject_hashmap_key(s, *a);
        }
        ath_runtime_error("HASHMAP EJECT requires SLOT n or a key expr", 0, 0);
        return ath_void();
    case ATH_SYL_BOTTLE:
        if (mod == EJ_NONE) return eject_bottle_lowest(s);
        if (mod == EJ_SLOT) {
            if (!a) ath_runtime_error("BOTTLE SLOT requires index", 0, 0);
            return eject_bottle_slot(s, *a);
        }
        ath_runtime_error("BOTTLE EJECT takes no modifier or SLOT n", 0, 0);
        return ath_void();
    case ATH_SYL_TECHHOP:
        if (mod != EJ_GROOVE_SHADE)
            ath_runtime_error("TECHHOP EJECT requires GROOVE g SHADE s", 0, 0);
        if (!a || !b)
            ath_runtime_error("TECHHOP EJECT requires GROOVE and SHADE", 0, 0);
        return eject_techhop(s, *a, *b);
    case ATH_SYL_JUJU:
        if (mod != EJ_SLOT)
            ath_runtime_error("JUJU EJECT requires SLOT n", 0, 0);
        if (!a) ath_runtime_error("JUJU SLOT requires index", 0, 0);
        return eject_juju(s, *a);
    }
    ath_runtime_error("EJECT: unknown sylladex kind", 0, 0);
    return ath_void();
}
