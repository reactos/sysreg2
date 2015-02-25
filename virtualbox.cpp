/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Support for VirtualBox machines through libvirt
 * COPYRIGHT:   Copyright 2015 Pierre Schweitzer <pierre@reactos.org>
 */

#include "machine.h"

VirtualBox::VirtualBox()
{
    vConn = virConnectOpen("vbox:///session");
}

bool VirtualBox::GetConsole(char* console)
{
    console[0] = 0;
    return true;
}

void VirtualBox::InitializeDisk()
{
    char vboxmanage_cmdline[300];

    /* Make sure the previous disk was removed from VBox to prevent UUID issues */
    sprintf(vboxmanage_cmdline, "VBoxManage closemedium disk %s --delete", AppSettings.HardDiskImage);
    Execute(vboxmanage_cmdline);

    /* Call main creation */
    LibVirt::InitializeDisk();
}

bool VirtualBox::StartListeningSocket(void)
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

