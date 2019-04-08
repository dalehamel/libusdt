/*
 * Copyright (c) 2012, Chris Andrews. All rights reserved.
 */

#include "usdt_internal.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

char *usdt_errors[] = {
  "failed to allocate memory",
  "failed to allocate page-aligned memory",
  "no probes defined",
  "failed to load DOF: %s",
  "provider is already enabled",
  "failed to unload DOF: %s",
  "probe named %s:%s:%s:%s already exists",
  "failed to remove probe %s:%s:%s:%s"
};

static void
free_probe(usdt_probe_t *p)
{
        switch (p->refcnt) {
        case 1:
                free((char *)p->function);
                free((char *)p->name);
                for (int i = 0; i < p->argc; i++)
                        free(p->types[i]);
                free(p);
                break;
        case 2:
                p->refcnt = 1;
                break;
        default:
                break;
        }
}

usdt_provider_t *
usdt_create_provider(const char *name, const char *module)
{
        usdt_provider_t *provider;

        if ((provider = malloc(sizeof *provider)) == NULL) 
                return NULL;

        provider->name = strdup(name);
        provider->module = strdup(module);
        provider->probes = NULL;
        provider->enabled = 0;

        return provider;
}

usdt_probe_t *
usdt_create_probe(const char *func, const char *name, size_t argc, ...)
{
        int i;
        va_list vl; 
        char *arg;
        va_start(vl, argc);
        usdt_probe_t *p;

        if (argc > USDT_ARG_MAX)
                argc = USDT_ARG_MAX;

        if ((p = malloc(sizeof *p)) == NULL)
                return (NULL);



        p->refcnt = 2;
        p->function = strdup(func);
        p->name = strdup(name);
        p->argc = argc;


        for(i=0; i < argc; i++) {
          arg = va_arg(vl, char*);
	  p->types[i] = strdup(arg);
        }

        //for(; i<USDT_ARG_MAX; i++) {
        //  p->types[i] = noarg;
        //}

        return (p);
}

void
usdt_probe_release(usdt_probe_t *probe)
{
        free_probe(probe);
}

int
usdt_provider_add_probe(usdt_provider_t *provider, usdt_probe_t *probe)
{
        usdt_probe_t *p;

        if (provider->probes != NULL) {
                for (p = provider->probes; (p != NULL); p = p->next) {
                if ((strcmp(p->name, probe->name) == 0) &&
                    (strcmp(p->function, probe->function) == 0)) {
                                usdt_error(provider, USDT_ERROR_DUP_PROBE,
                                           provider->name, provider->module,
                                           probe->function, probe->name);
                                return (-1);
                        }
                }
        }

        probe->next = NULL;
        if (provider->probes == NULL)
                provider->probes = probe;
        else {
                for (p = provider->probes; (p->next != NULL); p = p->next) ;
                p->next = probe;
        }

        return (0);
}

int
usdt_provider_remove_probe(usdt_provider_t *provider, usdt_probe_t *probe)
{
        usdt_probe_t *p, *prev_p = NULL;

        if (provider->probes == NULL) {
                usdt_error(provider, USDT_ERROR_NOPROBES);
                return (-1);
        }

        for (p = provider->probes; (p != NULL);
             prev_p = p, p = p->next) {

                if ((strcmp(p->name, probe->name) == 0) &&
                    (strcmp(p->function, probe->function) == 0)) {

                        if (prev_p == NULL)
                                provider->probes = p->next;
                        else
                                prev_p->next = p->next;

                        return (0);
                }
        }

        usdt_error(provider, USDT_ERROR_REMOVE_PROBE,
                   provider->name, provider->module,
                   probe->function, probe->name);
        return (-1);
}

int
usdt_provider_enable(usdt_provider_t *provider)
{
        usdt_strtab_t strtab;
        usdt_dof_file_t *file;
        int i;
        size_t size;
        usdt_dof_section_t sects[5];

        if (provider->enabled == 1) {
                usdt_error(provider, USDT_ERROR_ALREADYENABLED);
                return (0); /* not fatal */
        }

        if (provider->probes == NULL) {
                usdt_error(provider, USDT_ERROR_NOPROBES);
                return (-1);
        }

        if ((usdt_strtab_init(&strtab, 0)) < 0) {
                usdt_error(provider, USDT_ERROR_MALLOC);
                return (-1);
        }

        if ((usdt_strtab_add(&strtab, provider->name)) == 0) {
                usdt_error(provider, USDT_ERROR_MALLOC);
                return (-1);
        }

        if ((usdt_dof_probes_sect(&sects[0], provider, &strtab)) < 0)
                return (-1);
        if ((usdt_dof_prargs_sect(&sects[1], provider)) < 0)
                return (-1);

        size = usdt_provider_dof_size(provider, &strtab);
        if ((file = usdt_dof_file_init(provider, size)) == NULL)
                return (-1);

        if ((usdt_dof_proffs_sect(&sects[2], provider, file->dof)) < 0)
                return (-1);
        if ((usdt_dof_prenoffs_sect(&sects[3], provider, file->dof)) < 0)
                return (-1);
        if ((usdt_dof_provider_sect(&sects[4], provider)) < 0)
                return (-1);

        for (i = 0; i < 5; i++)
                usdt_dof_file_append_section(file, &sects[i]);

        usdt_dof_file_generate(file, &strtab);

        usdt_dof_section_free((usdt_dof_section_t *)&strtab);
        for (i = 0; i < 5; i++)
                usdt_dof_section_free(&sects[i]);

        if ((usdt_dof_file_load(file, provider->module)) < 0) {
                usdt_error(provider, USDT_ERROR_LOADDOF, strerror(errno));
                return (-1);
        }

        provider->enabled = 1;
        provider->file = file;

        return (0);
}

int
usdt_provider_disable(usdt_provider_t *provider)
{
        if (provider->enabled == 0)
                return (0);

        if ((usdt_dof_file_unload((usdt_dof_file_t *)provider->file)) < 0) {
                usdt_error(provider, USDT_ERROR_UNLOADDOF, strerror(errno));
                return (-1);
        }

        usdt_dof_file_free(provider->file);
        provider->file = NULL;
        provider->enabled = 0;

        return (0);
}

void
usdt_provider_free(usdt_provider_t *provider)
{
        usdt_probe_t *p, *next;

        for (p = provider->probes; p != NULL; p = next) {
                next = p->next;
                free_probe(p);
        }

        free((char *)provider->name);
        free((char *)provider->module);
        free(provider);
}

int
usdt_is_enabled(usdt_probe_t *probe)
{
  if(probe->_fire == NULL) {
    return 0;
  }
  if(((*(char *)probe->_fire) & 0x90) == 0x90) {
    return 0;
  }
  return 1;
}

void
usdt_fire_probe(usdt_probe_t *probe, ...)
{
  if(probe->_fire == NULL) {
    return;
  }
  va_list vl;
  va_start(vl, probe);
  uint64_t arg[6] = {0};
  for(int i=0; i < probe->argc; i++) {
    arg[i] = va_arg(vl, uint64_t);
  }

  switch(probe->argc) {
    case 0:
      ((void (*)())probe->_fire) ();
      return;
    case 1:
      ((void (*)())probe->_fire) (arg[0]);
      return;
    case 2:
      ((void (*)())probe->_fire) (arg[0], arg[1]);
      return;
    case 3:
      ((void (*)())probe->_fire) (arg[0], arg[1], arg[2]);
      return;
    case 4:
      ((void (*)())probe->_fire) (arg[0], arg[1], arg[2], arg[3]);
      return;
    case 5:
      ((void (*)())probe->_fire) (arg[0], arg[1], arg[2], arg[3], arg[4]);
      return;
    case 6:
      ((void (*)())probe->_fire) (arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);
      return;
    default:
      ((void (*)())probe->_fire) ();
      return;
  }
}

static void
usdt_verror(usdt_provider_t *provider, usdt_error_t error, va_list argp)
{
        vasprintf(&provider->error, usdt_errors[error], argp);
}

void
usdt_error(usdt_provider_t *provider, usdt_error_t error, ...)
{
        va_list argp;

        va_start(argp, error);
        usdt_verror(provider, error, argp);
        va_end(argp);
}

char *
usdt_errstr(usdt_provider_t *provider)
{
        return (provider->error);
}
