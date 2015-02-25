/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Support for VMWare Player machines through libvirt
 * COPYRIGHT:   Copyright 2013 Pierre Schweitzer <pierre@reactos.org>
 */

#include "machine.h"

VMWarePlayer::VMWarePlayer()
{
    vConn = virConnectOpen("vmwareplayer:///session");
}

bool VMWarePlayer::GetConsole(char* console)
{
    console[0] = 0;
    return true;
}

bool VMWarePlayer::PrepareSerialPort()
{
    return CreateLocalSocket();
}
