/*
 * Copyright (c) 2012, Chris Andrews. All rights reserved.
 */

#include "usdt_internal.h"

void funcStart();
void funcEnd();

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
//        int fd;
//        char tmp[20] = "/tmp/libusdtXXXXXX";
//
//        if ((fd = mkstemp(tmp)) < 0)
//                return (-1);
//        if (unlink(tmp) < 0)
//                return (-1);
//        if (write(fd, "\0", FUNC_SIZE) < FUNC_SIZE)
//                return (-1);
//
//        probe->isenabled_addr = (int (*)())mmap(NULL, FUNC_SIZE,
//                                                PROT_READ | PROT_WRITE | PROT_EXEC,
//                                                MAP_PRIVATE, fd, 0);
// FIXME - is valloc working? Above code is meant for linux, but may work?
        probe->_fire = (int (*)())valloc(FUNC_SIZE); // <<-- Orig for Darwin?

        if (probe->_fire == NULL)
                return (-1);

        /* ensure that the tracepoints will fit the heap we're allocating */
        size = (unsigned long long)funcEnd - (unsigned long long)funcStart;
        assert(size < FUNC_SIZE);

        probe->_fire = funcStart;

        memcpy((void *)probe->_fire,
               (const void *)funcStart, FUNC_SIZE);

        mprotect((void *)probe->_fire, FUNC_SIZE, // << - Meant for linux?
                 PROT_READ | PROT_EXEC);
//        mprotect((void *)probe->isenabled_addr, FUNC_SIZE, << - Orig for Darwin
//                 PROT_READ | PROT_WRITE | PROT_EXEC);

        return (0);
}

void
usdt_free_tracepoints(usdt_probe_t *probe)
{
        (void) munmap(probe->_fire, FUNC_SIZE);
        // free(probe->isenabled_addr); << - Orig for darwin
}
