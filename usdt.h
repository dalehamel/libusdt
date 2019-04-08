/*
 * Copyright (c) 2012, Chris Andrews. All rights reserved.
 */

#ifndef __LIB_USDT_USDT_H__
#define __LIB_USDT_USDT_H__

#include <stdint.h>
#include <unistd.h>

#define USDT_ARG_MAX 6

typedef enum usdt_error {
        USDT_ERROR_MALLOC = 0,
        USDT_ERROR_VALLOC,
        USDT_ERROR_NOPROBES,
        USDT_ERROR_LOADDOF,
        USDT_ERROR_ALREADYENABLED,
        USDT_ERROR_UNLOADDOF,
        USDT_ERROR_DUP_PROBE,
        USDT_ERROR_REMOVE_PROBE
} usdt_error_t;

typedef struct usdt_probe {
        const char *name;
        const char *function;
        size_t argc;
	char *types[USDT_ARG_MAX];
        void *_fire;
        struct usdt_probe *next;
        int refcnt;
} usdt_probe_t;

int usdt_is_enabled(usdt_probe_t *probe);
void usdt_fire_probe(usdt_probe_t *probe, ...);
usdt_probe_t *usdt_create_probe(const char *func, const char *name,
                                   size_t argc, ...);
void usdt_probe_release(usdt_probe_t *probe);

typedef struct usdt_provider {
        const char *name;
        const char *module;
        usdt_probe_t *probes;
        char *error;
        int enabled;
        void *file;
} usdt_provider_t;

usdt_provider_t *usdt_create_provider(const char *name, const char *module);
int usdt_provider_add_probe(usdt_provider_t *provider, usdt_probe_t *probe);
int usdt_provider_remove_probe(usdt_provider_t *provider, usdt_probe_t *probe);
int usdt_provider_enable(usdt_provider_t *provider);
int usdt_provider_disable(usdt_provider_t *provider);
void usdt_provider_free(usdt_provider_t *provider);

void usdt_error(usdt_provider_t *provider, usdt_error_t error, ...);
char *usdt_errstr(usdt_provider_t *provider);

#endif // __LIB_USDT_USDT_H__
