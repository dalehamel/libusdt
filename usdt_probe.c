/*
 * Copyright (c) 2012, Chris Andrews. All rights reserved.
 */

#include "usdt_internal.h"

uint64_t
usdt_probe_offset(usdt_probe_t *probe, char *dof, uint8_t argc)
{
        uint64_t offset;
        offset = ((uint64_t) probe->probe_addr - (uint64_t) dof + 2);
        return (offset);
}

uint64_t
usdt_is_enabled_offset(usdt_probe_t *probe, char *dof) 
{
        uint64_t offset;
        offset = ((uint64_t) probe->isenabled_addr - (uint64_t) dof + 6);
        return (offset);
}

int
usdt_create_tracepoints(usdt_probe_t *probe)
{
        /* Prepare the tracepoints - for each probe, a separate chunk
         * of memory with the tracepoint code copied into it, to give
         * us unique addresses for each tracepoint.
         *
         * On Oracle Linux, this must be an mmapped file because USDT
         * probes there are implemented as uprobes, which are
         * addressed by inode and offset. The file used is a small
         * mkstemp'd file we immediately unlink.
         *
         * Elsewhere, we can use the heap directly because USDT will
         * instrument any memory mapped by the process.
         */

        size_t size;
        int fd;
        char tmp[20] = "/tmp/libusdtXXXXXX";

        if ((fd = mkstemp(tmp)) < 0)
                return (-1);
        if (unlink(tmp) < 0)
                return (-1);
        if (write(fd, "\0", FUNC_SIZE) < FUNC_SIZE)
                return (-1);

        probe->isenabled_addr = (int (*)())mmap(NULL, FUNC_SIZE,
                                                PROT_READ | PROT_WRITE | PROT_EXEC,
                                                MAP_PRIVATE, fd, 0);
// FIXME - is valloc working? Above code is meant for linux, but may work?
//        probe->isenabled_addr = (int (*)())valloc(FUNC_SIZE); // <<-- Orig for Darwin?

        if (probe->isenabled_addr == NULL)
                return (-1);

        /* ensure that the tracepoints will fit the heap we're allocating */
        size = ((char *)usdt_tracepoint_end - (char *)usdt_tracepoint_isenabled);
        assert(size < FUNC_SIZE);

        size = ((char *)usdt_tracepoint_probe - (char *)usdt_tracepoint_isenabled);
        probe->probe_addr = (char *)probe->isenabled_addr + size;

        memcpy((void *)probe->isenabled_addr,
               (const void *)usdt_tracepoint_isenabled, FUNC_SIZE);

        mprotect((void *)probe->isenabled_addr, FUNC_SIZE, // << - Meant for linux?
                 PROT_READ | PROT_EXEC);
//        mprotect((void *)probe->isenabled_addr, FUNC_SIZE, << - Orig for Darwin
//                 PROT_READ | PROT_WRITE | PROT_EXEC);

        return (0);
}

void
usdt_free_tracepoints(usdt_probe_t *probe)
{
        (void) munmap(probe->isenabled_addr, FUNC_SIZE);
        // free(probe->isenabled_addr); << - Orig for darwin
}
