// SPDX-License-Identifier: GPL-2.0-only
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

/* ath_entity.c */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200112L
#endif
#include "ath_entity.h"
#include "ath_eventloop.h"
#include "ath_error.h"
#include "ath_session.h"
#include "ath_relic.h"
#include "ath_buffer.h"
#include "ath_builtins.h"  /* ath_call_sync */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <dlfcn.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#endif

/* ===== pollable entity registry ===== */

#define MAX_POLLABLE 1024
static AthEntity *_pollable[MAX_POLLABLE];
static int _pollable_count = 0;

void ath_entity_register_pollable(AthEntity *e) {
    if (_pollable_count < MAX_POLLABLE)
        _pollable[_pollable_count++] = e;
}

void ath_entity_unregister_pollable(AthEntity *e) {
    int i;
    for (i = 0; i < _pollable_count; i++) {
        if (_pollable[i] == e) {
            _pollable[i] = _pollable[--_pollable_count];
            return;
        }
    }
}

void ath_entity_poll_all(unsigned long now_ms) {
    int i;
    for (i = 0; i < _pollable_count; i++) {
        if (!_pollable[i]->is_dead)
            ath_entity_poll(_pollable[i], now_ms);
    }
}

/* Number of live process/connection/watcher entities still being polled.
   The event loop must not terminate while this is non-zero, even when the
   timer heap is empty. */
int ath_entity_pending_count(void) {
    int i, n = 0;
    for (i = 0; i < _pollable_count; i++) {
        if (!_pollable[i]->is_dead)
            n++;
    }
    return n;
}

/* ===== common helpers ===== */

static AthEntity *ath_entity_alloc(AthEntityKind kind, const char *name) {
    AthEntity *e = (AthEntity *)calloc(1, sizeof(AthEntity));
    if (!e) ath_fatal("out of memory");
    e->refcount = 1;
    e->kind     = kind;
    e->name     = name ? name : "";
    e->is_dead  = 0;
    e->waiters  = NULL;
    return e;
}

void ath_entity_incref(AthEntity *e) { if (e) e->refcount++; }

void ath_entity_decref(AthEntity *e) {
    if (!e) return;
    if (--e->refcount <= 0) {
        /* free waiter list */
        AthWaiter *w = e->waiters;
        while (w) { AthWaiter *n = w->next; free(w); w = n; }
        if (e->kind == ATH_ENTITY_WATCHER) free(e->filepath);
        if (e->kind == ATH_ENTITY_AND || e->kind == ATH_ENTITY_OR ||
            e->kind == ATH_ENTITY_NOT) {
            if (e->left)  ath_entity_decref(e->left);
            if (e->right) ath_entity_decref(e->right);
        }
        free(e);
    }
}

void ath_entity_die(AthEntity *e) {
    AthWaiter *w;
    if (!e || e->is_dead) return;

    /* Sessions defer their actual teardown to the event loop top: mark the
       session dying (blocks new FFI calls) and schedule the teardown
       continuation. The continuation walks destructors LIFO, dlcloses
       (orderly only), marks is_dead, and fires waiters -- see
       ath_session_schedule_teardown. */
    if (e->kind == ATH_ENTITY_SESSION && e->session) {
        if (!e->session->dying) {
            e->session->dying = 1;
            ath_session_schedule_teardown(e->session);
        }
        return;
    }

    e->is_dead = 1;
    /* Schedule all waiters via event loop (never call directly) */
    w = e->waiters;
    e->waiters = NULL;
    while (w) {
        AthWaiter *next = w->next;
        ath_eventloop_schedule(w->cont, ath_void());
        free(w);
        w = next;
    }
    if (e->kind == ATH_ENTITY_PROCESS ||
        e->kind == ATH_ENTITY_CONNECTION ||
        e->kind == ATH_ENTITY_WATCHER) {
        ath_entity_unregister_pollable(e);
    }
}

void ath_entity_on_death(AthEntity *e, struct AthCont *k) {
    if (e->is_dead) {
        /* already dead: schedule immediately */
        ath_eventloop_schedule(k, ath_void());
    } else {
        AthWaiter *w = (AthWaiter *)malloc(sizeof(AthWaiter));
        if (!w) ath_fatal("out of memory");
        w->cont = k;
        w->next = e->waiters;
        e->waiters = w;
    }
}

/* ===== THIS ===== */

AthEntity *ath_entity_this_new(void) {
    return ath_entity_alloc(ATH_ENTITY_THIS, "THIS");
}

/* ===== Timer ===== */

AthEntity *ath_entity_timer_new(const char *name, unsigned long ms) {
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_TIMER, name);
    if (ms < 1) ms = 1;
    e->deadline_ms = ath_eventloop_now_ms() + ms;
    ath_eventloop_schedule_at_entity(e);
    return e;
}

/* ===== Branch ===== */

AthEntity *ath_entity_branch_new(const char *name) {
    return ath_entity_alloc(ATH_ENTITY_BRANCH, name);
}

/* ===== Composite ===== */

/* Waiter installed on the children of AND/OR */
typedef struct CompositeWaiter {
    AthCont  base;   /* must be first */
    AthEntity *parent;
    int       is_right;  /* for AND tracking */
} CompositeWaiter;

static void composite_and_child_died(AthCont *self, AthValue unused) {
    CompositeWaiter *cw = (CompositeWaiter *)self;
    AthEntity *parent = cw->parent;
    (void)unused;
    if (cw->is_right) parent->and_right_dead = 1;
    else              parent->and_left_dead  = 1;
    if (parent->and_left_dead && parent->and_right_dead)
        ath_entity_die(parent);
    ath_entity_decref(parent);
    free(cw);
}

static void composite_or_child_died(AthCont *self, AthValue unused) {
    CompositeWaiter *cw = (CompositeWaiter *)self;
    AthEntity *parent = cw->parent;
    (void)unused;
    ath_entity_die(parent);
    ath_entity_decref(parent);
    free(cw);
}

static void composite_not_immediate(AthCont *self, AthValue unused) {
    CompositeWaiter *cw = (CompositeWaiter *)self;
    (void)unused;
    ath_entity_die(cw->parent);
    ath_entity_decref(cw->parent);
    free(cw);
}

AthEntity *ath_entity_and_new(AthEntity *a, AthEntity *b) {
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_AND, "AND");
    CompositeWaiter *wl, *wr;
    e->left  = a; ath_entity_incref(a);
    e->right = b; ath_entity_incref(b);
    e->and_left_dead  = 0;
    e->and_right_dead = 0;
    wl = (CompositeWaiter *)malloc(sizeof(CompositeWaiter));
    if (!wl) ath_fatal("out of memory");
    wl->base.resume = composite_and_child_died;
    wl->base.next   = NULL;
    wl->base.refcount = 1;
    wl->parent = e; ath_entity_incref(e);
    wl->is_right = 0;
    ath_entity_on_death(a, (AthCont*)wl);
    wr = (CompositeWaiter *)malloc(sizeof(CompositeWaiter));
    if (!wr) ath_fatal("out of memory");
    wr->base.resume = composite_and_child_died;
    wr->base.next   = NULL;
    wr->base.refcount = 1;
    wr->parent = e; ath_entity_incref(e);
    wr->is_right = 1;
    ath_entity_on_death(b, (AthCont*)wr);
    return e;
}

AthEntity *ath_entity_or_new(AthEntity *a, AthEntity *b) {
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_OR, "OR");
    CompositeWaiter *wl, *wr;
    e->left  = a; ath_entity_incref(a);
    e->right = b; ath_entity_incref(b);
    wl = (CompositeWaiter *)malloc(sizeof(CompositeWaiter));
    if (!wl) ath_fatal("out of memory");
    wl->base.resume = composite_or_child_died;
    wl->base.next   = NULL;
    wl->base.refcount = 1;
    wl->parent = e; ath_entity_incref(e);
    wl->is_right = 0;
    ath_entity_on_death(a, (AthCont*)wl);
    wr = (CompositeWaiter *)malloc(sizeof(CompositeWaiter));
    if (!wr) ath_fatal("out of memory");
    wr->base.resume = composite_or_child_died;
    wr->base.next   = NULL;
    wr->base.refcount = 1;
    wr->parent = e; ath_entity_incref(e);
    wr->is_right = 1;
    ath_entity_on_death(b, (AthCont*)wr);
    return e;
}

/* ===== Universal DIE / BANISH dispatch ===== */

void ath_die_value(AthValue v) {
    if (v.type == ATH_ENTITY) {
        ath_entity_die(v.as.entity);
        return;
    }
    if (v.type == ATH_SESSION) {
        if (v.as.session && v.as.session->entity)
            ath_entity_die(v.as.session->entity);
        return;
    }
    ath_runtime_error_fmt("cannot DIE on value of type %s",
                          ath_typeof_str(v));
}

AthEntity *ath_extract_entity(AthValue v) {
    if (v.type == ATH_ENTITY)  return v.as.entity;
    if (v.type == ATH_SESSION) return v.as.session ? v.as.session->entity : NULL;
    ath_runtime_error_fmt("expected entity (or session), got %s",
                          ath_typeof_str(v));
    return NULL;
}

void ath_banish_value(AthValue v) {
    if (v.type == ATH_RELIC) {
        AthRelic *r = v.as.relic;
        if (!r) return;
        if (r->cursed) return;
        if (r->destructor) {
            AthValue dv = ath_rite_val(r->destructor);
            AthValue arg = ath_relic_val(r);
            ath_value_incref(dv);
            ath_value_incref(arg);
            ath_call_sync(NULL, dv, 1, &arg);
            ath_value_decref(arg);
            ath_value_decref(dv);
        }
        ath_relic_curse(r);
        return;
    }
    if (v.type == ATH_BUFFER) {
        ath_buffer_release(v.as.buffer);
        return;
    }
    ath_runtime_error_fmt("BANISH: expected RELIC or BUFFER, got %s",
                          ath_typeof_str(v));
}

/* ===== Session ===== */

AthEntity *ath_entity_session_new(const char *name) {
    /* The entity is just a death-target. The owning AthSession (built by
       ath_session_create) holds the dlhandle, the rites map, and the relic
       registry, and links itself back here via e->session. */
    return ath_entity_alloc(ATH_ENTITY_SESSION, name);
}

AthEntity *ath_entity_not_new(AthEntity *inner) {
    /* NOT entity: dies immediately (next event loop tick) */
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_NOT, "NOT");
    CompositeWaiter *cw;
    (void)inner; /* NOT ignores inner's death; fires immediately */
    cw = (CompositeWaiter *)malloc(sizeof(CompositeWaiter));
    if (!cw) ath_fatal("out of memory");
    cw->base.resume = composite_not_immediate;
    cw->base.next   = NULL;
    cw->base.refcount = 1;
    cw->parent = e; ath_entity_incref(e);
    cw->is_right = 0;
    ath_eventloop_schedule((AthCont*)cw, ath_void());
    return e;
}

/* ===== Process ===== */

#ifdef _WIN32
AthEntity *ath_entity_process_new(const char *name, const char *cmd, char *const argv[]) {
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_PROCESS, name);
    char cmdline[4096];
    int i, pos = 0;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    pos += snprintf(cmdline+pos, sizeof(cmdline)-pos, "\"%s\"", cmd);
    for (i = 0; argv && argv[i]; i++)
        pos += snprintf(cmdline+pos, sizeof(cmdline)-pos, " \"%s\"", argv[i]);
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        ath_eventloop_schedule_entity_die(e);
    } else {
        e->pid = (intptr_t)pi.hProcess;  /* store HANDLE as intptr_t */
        ath_entity_register_pollable(e);
        CloseHandle(pi.hThread);
    }
    return e;
}
#else
AthEntity *ath_entity_process_new(const char *name, const char *cmd, char *const argv[]) {
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_PROCESS, name);
    pid_t pid = fork();
    if (pid < 0) {
        ath_eventloop_schedule_entity_die(e);
    } else if (pid == 0) {
        char **args;
        int argc = 0, i;
        while (argv && argv[argc]) argc++;
        args = (char**)malloc(sizeof(char*)*(argc+2));
        args[0] = (char*)cmd;
        for (i = 0; i < argc; i++) args[i+1] = (char*)argv[i];
        args[argc+1] = NULL;
        execvp(cmd, args);
        _exit(1);
    } else {
        e->pid = (intptr_t)pid;
        ath_entity_register_pollable(e);
    }
    return e;
}
#endif

/* ===== Connection ===== */

#ifdef _WIN32
static int _wsa_initialized = 0;
static void _wsa_init(void) {
    WSADATA wd;
    if (!_wsa_initialized) { WSAStartup(MAKEWORD(2,2), &wd); _wsa_initialized = 1; }
}
AthEntity *ath_entity_connection_new(const char *name, const char *host, int port) {
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_CONNECTION, name);
    SOCKET sock;
    struct sockaddr_in sa;
    struct hostent *he;
    unsigned long mode;
    _wsa_init();
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) { ath_eventloop_schedule_entity_die(e); return e; }
    he = gethostbyname(host);
    if (!he) { closesocket(sock); ath_eventloop_schedule_entity_die(e); return e; }
    memset(&sa,0,sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((unsigned short)port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
    if (connect(sock,(struct sockaddr*)&sa,sizeof(sa)) != 0) {
        closesocket(sock); ath_eventloop_schedule_entity_die(e); return e;
    }
    /* Set non-blocking for polling */
    mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
    e->sockfd = (intptr_t)sock;  /* store SOCKET as intptr_t */
    ath_entity_register_pollable(e);
    return e;
}
#else
AthEntity *ath_entity_connection_new(const char *name, const char *host, int port) {
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_CONNECTION, name);
    int sock;
    struct hostent *he;
    struct sockaddr_in sa;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { ath_eventloop_schedule_entity_die(e); return e; }
    he = gethostbyname(host);
    if (!he) { close(sock); ath_eventloop_schedule_entity_die(e); return e; }
    memset(&sa,0,sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((unsigned short)port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
    if (connect(sock,(struct sockaddr*)&sa,(socklen_t)sizeof(sa)) < 0) {
        close(sock); ath_eventloop_schedule_entity_die(e); return e;
    }
    fcntl(sock, F_SETFL, fcntl(sock,F_GETFL,0) | O_NONBLOCK);
    e->sockfd = (intptr_t)sock;
    ath_entity_register_pollable(e);
    return e;
}
#endif

/* ===== Watcher ===== */

AthEntity *ath_entity_watcher_new(const char *name, const char *filepath) {
    AthEntity *e = ath_entity_alloc(ATH_ENTITY_WATCHER, name);
    struct stat st;
    e->filepath = (char*)malloc(strlen(filepath)+1);
    strcpy(e->filepath, filepath);
    e->next_poll_ms = ath_eventloop_now_ms() + 100;
    if (stat(filepath, &st) != 0) {
        /* file doesn't exist: die immediately */
        ath_eventloop_schedule_entity_die(e);
    } else {
        ath_entity_register_pollable(e);
    }
    return e;
}

/* ===== Poll ===== */

void ath_entity_poll(AthEntity *e, unsigned long now_ms) {
    if (e->is_dead) return;
    switch (e->kind) {
    case ATH_ENTITY_PROCESS: {
#ifdef _WIN32
        HANDLE h = (HANDLE)e->pid;
        if (WaitForSingleObject(h, 0) == WAIT_OBJECT_0) {
            CloseHandle(h);
            e->pid = 0;
            ath_entity_die(e);
        }
#else
        int status;
        if (waitpid((pid_t)e->pid, &status, WNOHANG) > 0)
            ath_entity_die(e);
#endif
        break;
    }
    case ATH_ENTITY_CONNECTION: {
#ifdef _WIN32
        fd_set fds;
        struct timeval tv;
        char buf[1];
        SOCKET sock = (SOCKET)e->sockfd;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        tv.tv_sec = 0; tv.tv_usec = 0;
        if (select(0 /* ignored on Windows */, &fds, NULL, NULL, &tv) > 0) {
            int n = (int)recv(sock, buf, 1, MSG_PEEK);
            if (n == 0) {
                closesocket(sock);
                ath_entity_die(e);
            }
        }
#else
        fd_set fds;
        struct timeval tv;
        char buf[1];
        int fd = (int)e->sockfd;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 0; tv.tv_usec = 0;
        if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
            int n = (int)recv(fd, buf, 1, MSG_PEEK);
            if (n == 0) {
                close(fd);
                ath_entity_die(e);
            }
        }
#endif
        break;
    }
    case ATH_ENTITY_WATCHER: {
        struct stat st;
        if (now_ms < e->next_poll_ms) break;
        e->next_poll_ms = now_ms + 100;
        if (stat(e->filepath, &st) != 0)
            ath_entity_die(e);
        break;
    }
    default: break;
    }
}
