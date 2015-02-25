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

bool VirtualBox::PrepareSerialPort()
{
    return CreateLocalSocket();
}

