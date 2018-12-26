/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Main entry point and controlling the virtual machine
 * COPYRIGHT:   Copyright 2008-2009 Christoph von Wittich <christoph_vw@reactos.org>
 *              Copyright 2009 Colin Finck <colin@reactos.org>
 *              Copyright 2012-2015 Pierre Schweitzer <pierre@reactos.org>
 */

#include "sysreg.h"
#include "machine.h"

const char DefaultOutputPath[] = "output-i386";
const char* OutputPath;
Settings AppSettings;
ModuleListEntry* ModuleList;
Machine * TestMachine = 0;

/* Wrapper for C code */
bool BreakToDebugger(void)
{
    /* We need a machine started */
    if (TestMachine == 0)
    {
        return false;
    }

    /* Call the C++ method */
    return TestMachine->BreakToDebugger();
}

int main(int argc, char **argv)
{
    int Ret = EXIT_DONT_CONTINUE;
    char console[50];
    unsigned int Retries;
    unsigned int Stage;

    /* Get the output path to the built ReactOS files */
    OutputPath = getenv("ROS_OUTPUT");
    if(!OutputPath)
        OutputPath = DefaultOutputPath;

    InitializeModuleList();

    SysregPrintf("sysreg2 %s starting\n", gGitCommit);

    if (!LoadSettings(argc > 1 ? argv[1] : "sysreg.xml"))
    {
        SysregPrintf("Cannot load configuration file\n");
        goto cleanup;
    }

    /* Allocate proper machine */
    switch (AppSettings.VMType)
    {
        case TYPE_KVM:
            TestMachine = new KVM();
            break;

        case TYPE_VMWARE_PLAYER:
            TestMachine = new VMWarePlayer();
            break;

        case TYPE_VIRTUALBOX:
            TestMachine = new VirtualBox();
            break;
    }

    /* Don't go any further if connection failed */
    if (!TestMachine->IsConnected())
    {
        SysregPrintf("Error: failed to connect to test machine.\n");
        goto cleanup;
    }

    /* Shutdown the machine if already running */
    if (TestMachine->IsMachineRunning(AppSettings.Name, true))
    {
        SysregPrintf("Error: Test Machine is still running.\n");
        goto cleanup;
    }

    /* Initialize disk if needed */
    TestMachine->InitializeDisk();

    for(Stage = 0; Stage < NUM_STAGES; Stage++)
    {
        /* Execute hook command before stage if any */
        if (AppSettings.Stage[Stage].HookCommand[0] != 0)
        {
            SysregPrintf("Applying hook: %s\n", AppSettings.Stage[Stage].HookCommand);
            int out = Execute(AppSettings.Stage[Stage].HookCommand);
            if (out < 0)
            {
                SysregPrintf("Hook command failed!\n");
                goto cleanup;
            }
        }

        for(Retries = 0; Retries < AppSettings.MaxRetries; Retries++)
        {
            struct timeval StartTime, EndTime, ElapsedTime;

            if (!TestMachine->LaunchMachine(AppSettings.Filename,
                                            AppSettings.Stage[Stage].BootDevice))
            {
                SysregPrintf("LaunchMachine failed!\n");
                goto cleanup;
            }

            printf("\n\n\n");
            SysregPrintf("Running stage %d...\n", Stage + 1);
            SysregPrintf("Domain %s started.\n", TestMachine->GetMachineName());

            gettimeofday(&StartTime, NULL);

            if (!TestMachine->GetConsole(console))
            {
                SysregPrintf("GetConsole failed!\n");
                goto cleanup;
            }
            Ret = ProcessDebugData(console, AppSettings.Timeout, Stage);

            gettimeofday(&EndTime, NULL);

            TestMachine->ShutdownMachine();

            timersub(&EndTime, &StartTime, &ElapsedTime);
            SysregPrintf("Stage took: %ld.%06ld seconds\n", ElapsedTime.tv_sec, ElapsedTime.tv_usec);

            usleep(1000);

            /* If we have a checkpoint to reach for success, assume that
               the application used for running the tests (probably "rosautotest")
               continues with the next test after a VM restart. */
            if (Ret == EXIT_CONTINUE && *AppSettings.Stage[Stage].Checkpoint)
                SysregPrintf("Rebooting machine (retry %d)\n", Retries + 1);
            else
                break;
        }

        if (Retries == AppSettings.MaxRetries)
        {
            SysregPrintf("Maximum number of allowed retries exceeded, aborting!\n");
            break;
        }

        if (Ret == EXIT_DONT_CONTINUE)
            break;
    }


cleanup:
    xmlCleanupParser();

    CleanModuleList();

    switch (Ret)
    {
        case EXIT_CHECKPOINT_REACHED:
            SysregPrintf("Status: Reached the checkpoint!\n");
            break;

        case EXIT_CONTINUE:
            SysregPrintf("Status: Failed to reach the checkpoint!\n");
            break;

        case EXIT_DONT_CONTINUE:
            SysregPrintf("Status: Testing process aborted!\n");
            break;
    }

    delete TestMachine;

    return Ret;
}
