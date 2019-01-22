/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Processing the incoming debugging data
 * COPYRIGHT:   Copyright 2008-2009 Christoph von Wittich <christoph_vw@reactos.org>
 *              Copyright 2009 Colin Finck <colin@reactos.org>
 *              Copyright 2012-2013 Pierre Schweitzer <pierre@reactos.org>
 */

#include "sysreg.h"
#define BUFFER_SIZE         512

int ProcessDebugData(const char* tty, int timeout, int stage )
{
    char Buffer[BUFFER_SIZE];
    char CacheBuffer[BUFFER_SIZE];
    char Raddr2LineBuffer[BUFFER_SIZE];
    char* bp = Buffer;
    int got;
    int Ret = EXIT_DONT_CONTINUE;
    int ttyfd;
    struct termios ttyattr, rawattr;
    unsigned int CacheHits = 0;
    unsigned int i;
    unsigned int KdbgHit = 0;
    unsigned int Cont = 0;
    bool AlreadyBooted = false;
    bool Prompt = false;
    bool CheckpointReached = false;
    bool BrokeToDebugger = false;
    bool MonitorStdin = false;

    /* Initialize CacheBuffer with an empty string */
    *CacheBuffer = 0;

    if (AppSettings.VMType == TYPE_VMWARE_PLAYER || AppSettings.VMType == TYPE_VIRTUALBOX)
    {
        /* Wait for VMware connection */
        if ((ttyfd = accept(AppSettings.Specific.VMwarePlayer.Socket, NULL, NULL)) < 0)
        {
            SysregPrintf("error getting socket\n");
            return Ret;
        }

        /* Set non blocking */
        if (fcntl(ttyfd, F_SETFL, O_NONBLOCK) < 0)
        {
            SysregPrintf("error setting flag\n");
            close(ttyfd);
            return Ret;
        }
    }
    else
    {
        /* ttyfd is the file descriptor of the virtual COM port */
        if ((ttyfd = open(tty, O_NOCTTY | O_RDWR | O_NONBLOCK)) < 0)
        {
            SysregPrintf("error opening tty\n");
            return Ret;
        }
    }

    /* We also monitor STDIN_FILENO, so a user can cancel the process with ESC */
    if (tcgetattr(STDIN_FILENO, &ttyattr) >= 0)
    {
        rawattr = ttyattr;
        rawattr.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                                         | IGNCR | ICRNL | IXON);
        rawattr.c_lflag &= ~(ICANON | ECHO | ECHONL);
        rawattr.c_oflag &= ~OPOST;
        rawattr.c_cflag &= ~(CSIZE | PARENB);
        rawattr.c_cflag |= CS8;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawattr) >= 0)
        {
            MonitorStdin = true;
        }
    }

    if (!MonitorStdin)
    {
        SysregPrintf("No STDIN, sysreg2 won't monitor it\n");
    }

    for(;;)
    {
        struct pollfd fds[] = {
            { ttyfd, POLLIN | POLLHUP | POLLERR, 0 },
            { STDIN_FILENO, POLLIN, 0 }, /* Always keep it as the end of the FDs */
        };

        nfds_t nfds = (sizeof(fds) / sizeof(struct pollfd));
        if (!MonitorStdin)
            --nfds;

        got = poll(fds, nfds, timeout);
        if (got < 0)
        {
            /* Just try it again on simple errors */
            if (errno == EINTR || errno == EAGAIN)
                continue;

            SysregPrintf("poll failed with error %d\n", errno);
            goto cleanup;
        }
        else if (got == 0)
        {
            /* timeout - only break once then, quit */
            if (!BreakToDebugger() || BrokeToDebugger)
            {
                SysregPrintf("timeout\n");
                Ret = EXIT_CONTINUE;
                goto cleanup;
            }
            else
            {
                BrokeToDebugger = true;
            }
        }

        /* Check for global timeout */
        if (time(0) >= AppSettings.GlobalTimeout)
        {
            /* global timeout */
            SysregPrintf("global timeout\n");
            Ret = EXIT_DONT_CONTINUE;
            goto cleanup;
        }

        for (i = 0; i < nfds; i++)
        {
            if ((fds[i].fd == ttyfd) && (
                (fds[i].revents & POLLHUP) ||
                (fds[i].revents & POLLERR)))
            {
                /* This might indicate VM shutdown (KVM), so continue and move to next stage */
                Ret = EXIT_CONTINUE;
                goto cleanup;
            }

            /* Wait till we get some input from the fd */
            if (!(fds[i].revents & POLLIN))
                continue;

            /* Reset buffer only when we read complete line */
            if (bp != Buffer && (*bp == '\n' || (bp - Buffer >= (BUFFER_SIZE - 1))))
                bp = Buffer;

            /* Read one line or a maximum of 511 bytes into a buffer (leave space for the null character) */
            while (bp - Buffer < (BUFFER_SIZE - 1))
            {
                got = read(fds[i].fd, bp, 1);

                if (got < 0)
                {
                    /* Give it another chance */
                    if (errno == EINTR)
                        continue;

                    /* There's nothing more to read */
                    else if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    SysregPrintf("read failed with error %d\n", errno);
                    goto cleanup;
                }
                else if (got == 0)
                {
                    /* No more data */
                    break;
                }

                if (fds[i].fd == STDIN_FILENO)
                {
                    /* break on ESC */
                    if (*bp == '\33')
                        goto cleanup;
                }
                else
                {
                    /* Null-terminate the line */
                    bp[1] = 0;

                    /* Break on newlines or in case of KDBG messages (which aren't terminated by newlines) */
                    if(*bp == '\n')
                        break;

                    if (strstr(Buffer, "kdb:>") || strstr(Buffer, "--- Press q to abort, any other key to continue ---"))
                    {
                        /* Set EOL */
                        ++bp;
                        *bp = '\n';
                        bp[1] = 0;
                        break;
                    }

                    ++bp;
                }
            }

            /* The rest of this logic is just about processing the serial output */
            if(fds[i].fd == STDIN_FILENO)
                continue;

            /* Check whether the message is of zero length */
            if (bp == Buffer && got == 0)
            {
                /* This can happen when the machine shut down (like after 1st or 2nd stage)
                   or after we got a Kdbg backtrace. */
                Ret = EXIT_CONTINUE;
                goto cleanup;
            }

            /* Only process complete lines */
            if (*bp != '\n' && (bp - Buffer < (BUFFER_SIZE - 1)))
                continue;

            /* Hackish way to detect reboot under VMware... */
            if (((AppSettings.VMType == TYPE_VMWARE_PLAYER) || (AppSettings.VMType == TYPE_VIRTUALBOX)) &&
                strstr(Buffer, "-----------------------------------------------------"))
            {
                if (AlreadyBooted)
                {
                    Ret = EXIT_CONTINUE;
                    goto cleanup;
                }
                else
                {
                    AlreadyBooted = true;
                    BrokeToDebugger = false;
                }
            }

            /* Detect whether the same line appears over and over again.
               If that is the case, cancel this test after a specified number of repetitions. */
            if(!strcmp(Buffer, CacheBuffer))
            {
                ++CacheHits;

                if(CacheHits > AppSettings.MaxCacheHits)
                {
                    SysregPrintf("Test seems to be stuck in an endless loop, canceled!\n");
                    Ret = EXIT_CONTINUE;
                    goto cleanup;
                }
            }
            else
            {
                CacheHits = 0;
                memcpy(CacheBuffer, Buffer, bp - Buffer + 1);
                CacheBuffer[bp - Buffer + 1] = 0;
            }

            /* Output the line, raddr2line the included addresses if necessary */
            if (KdbgHit == 1 && ResolveAddressFromFile(Raddr2LineBuffer, sizeof(Raddr2LineBuffer), Buffer))
                printf("%s", Raddr2LineBuffer);
            else
                printf("%s", Buffer);

            /* Check for "magic" sequences */
            if (strstr(Buffer, "kdb:>"))
            {
                ++KdbgHit;

                if (KdbgHit == 1)
                {
                    /* If we have a call to RtlAssert(),  break once
                     * Otherwise we hit Kdbg for the first time, get a backtrace for the log
                     */
                    if (safewriteex(ttyfd, (Prompt ? "o\r" : "bt\r"), (Prompt ? 2 : 3), timeout) < 0
                        && errno == EWOULDBLOCK)
                    {
                        /* timeout */
                        SysregPrintf("timeout\n");
                        Ret = EXIT_CONTINUE;
                        goto cleanup;
                        /* No need to reset Prompt here, we will quit */
                    }

                    if (Prompt)
                    {
                        /* We're not prompted afterwards, so reset */
                        Prompt = false;
                        /* On next hit, we'll have broken once, so prepare for bt */
                        KdbgHit = 0;
                    }

                    continue;
                }
                else
                {
                    ++Cont;

                    /* We won't cont if we reached max tries */
                    if (Cont <= AppSettings.MaxConts || BrokeToDebugger)
                    {
                        KdbgHit = 0;

                        /* Try to continue */
                        if (safewrite(ttyfd, "cont\r", timeout) < 0 && errno == EWOULDBLOCK)
                        {
                            /* timeout */
                            SysregPrintf("timeout\n");
                            Ret = EXIT_CONTINUE;
                            goto cleanup;
                        }

                        /* Reduce timeout to let ROS properly shutdown (if possible) */
                        if (BrokeToDebugger)
                        {
                            timeout = 5000;
                        }

                        continue;
                    }
                    else
                    {
                        /* We tried to continue too many times - abort */
                        printf("\n");
                        Ret = EXIT_CONTINUE;
                        goto cleanup;
                    }

                }
            }
            else if (strstr(Buffer, "--- Press q"))
            {
                /* Send Return to get more data from Kdbg */
                if (safewrite(ttyfd, "\r", timeout) < 0 && errno == EWOULDBLOCK)
                {
                    /* timeout */
                    SysregPrintf("timeout\n");
                    Ret = EXIT_CONTINUE;
                    goto cleanup;
                }
                continue;
            }
            else if (strstr(Buffer, "Break repea"))
            {
                /* This is a call to DbgPrompt, next kdb prompt will be for selecting behavior */
                Prompt = true;
            }
            else if (strstr(Buffer, "SYSREG_ROSAUTOTEST_FAILURE"))
            {
                /* rosautotest itself has problems, so there's no reason to continue */
                goto cleanup;
            }
            else if (*AppSettings.Stage[stage].Checkpoint && strstr(Buffer, AppSettings.Stage[stage].Checkpoint))
            {
                /* We reached a checkpoint, so return success */
                CheckpointReached = true;
            }
        }
    }


cleanup:
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ttyattr);
    close(ttyfd);

    return (CheckpointReached ? EXIT_CHECKPOINT_REACHED : Ret);
}
