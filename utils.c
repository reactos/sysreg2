/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Various auxiliary functions
 * COPYRIGHT:   Copyright 2008-2009 Christoph von Wittich <christoph_vw@reactos.org>
 *              Copyright 2009 Colin Finck <colin@reactos.org>
 *              Copyright 2013-2015 Pierre Schweitzer <pierre@reactos.org>
 */

#include "sysreg.h"

ssize_t safewriteex(int fd, const void *buf, size_t count, int timeout)
{
    struct pollfd fds[] = {
        { fd, POLLOUT, 0 },
    };
    size_t nwritten = 0;

    while (count > 0) {
        /* Ensure we can write to the fd */
        int got = poll(fds, (sizeof(fds) / sizeof(struct pollfd)), timeout);
        /* Timeout, signal it to caller */
        if (got == 0) {
            errno = EWOULDBLOCK;
            return -1;
        }
        /* Error, return, errno is set */
        else if (got < 0) {
            return -1;
        }

        ssize_t r = write(fd, buf, count);

        if (r < 0 && errno == EINTR)
            continue;
        if (r < 0)
            return r;
        if (r == 0)
            return nwritten;
        buf = (const char *)buf + r;
        count -= r;
        nwritten += r;
    }
    return nwritten;
}

char * ReadFile (const char *filename)
{
    char *buffer = NULL, *oldbuffer;
    int len = 0, fd;
    char b[1024];

    fd = open (filename, O_RDONLY);
    if (fd == -1)
        return NULL;

    for (;;) {
        int r = read (fd, b, sizeof b);
        if (r == -1)
        {
          if (buffer) free (buffer);
          close(fd);
          return NULL;
        }
        if (r == 0) break;  /* end of file. */
        oldbuffer = buffer;
        buffer = realloc (buffer, len+r);
        if (buffer == NULL) {
            /* out of memory */
            free(oldbuffer);
            close(fd);
            return NULL;
        }
        memcpy (buffer+len, b, r);
        len += r;
    }

    oldbuffer = buffer;
    buffer = realloc (buffer, len+1);
    if (buffer == NULL)
    {
        /* out of memory */
        free(oldbuffer);
        close(fd);
        return NULL;
    }
    buffer[len] = '\0';
    close (fd);
    return buffer;
}

void SysregPrintf(const char* format, ...)
{
    va_list args;

    printf("[SYSREG] ");

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

int Execute(const char * command)
{
    FILE* in;
    char out[255];

    in = popen(command, "r");
    if (in == NULL)
    {
        SysregPrintf("popen() failed: %d\n", errno);
        return -errno;
    }

    while (feof(in) == 0)
    {
        if (fgets(out, 255, in) != NULL)
            SysregPrintf("%s", out);
    }

    return pclose(in);
}

bool CreateLocalSocket(void)
{
    struct sockaddr_un addr;

    if ((AppSettings.Specific.VMwarePlayer.Socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        SysregPrintf("Failed creating socket\n");
        return false;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, AppSettings.Specific.VMwarePlayer.Path, sizeof(addr.sun_path) - 1);

    /* Safety measure */
    unlink(AppSettings.Specific.VMwarePlayer.Path);

    if (bind(AppSettings.Specific.VMwarePlayer.Socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        SysregPrintf("Failed binding\n");
        return false;
    }

    if (listen(AppSettings.Specific.VMwarePlayer.Socket, 5) < 0)
    {
        SysregPrintf("Failed listening\n");
        return false;
    }

    return true;
}
