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
