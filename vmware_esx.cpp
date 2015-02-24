/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Support for VMware ESX machines through libvirt - stubbed
 * COPYRIGHT:   Copyright 2013 Pierre Schweitzer <pierre@reactos.org>
 */

#include "machine.h"

static int AuthCreds[] = {
    VIR_CRED_PASSPHRASE,
};

VMWareESX::VMWareESX()
{
    char libvirt_cmdline[300];
    virConnectAuth vAuth;

    strcpy(libvirt_cmdline, "esx://");
    if (AppSettings.Specific.VMwareESX.Username)
    {
        strcat(libvirt_cmdline, AppSettings.Specific.VMwareESX.Username);
        strcat(libvirt_cmdline, "@");
    }

    if (AppSettings.Specific.VMwareESX.Domain)
        strcat(libvirt_cmdline, AppSettings.Specific.VMwareESX.Domain);
    else
        strcat(libvirt_cmdline, "localhost");

    if (AppSettings.Specific.VMwareESX.Password)
    {
        vAuth.credtype = AuthCreds;
        vAuth.ncredtype = sizeof(AuthCreds)/sizeof(int);
        vAuth.cb = AuthToVMware;
        vAuth.cbdata = NULL;
    }
    else
        vAuth = *virConnectAuthPtrDefault;

    vConn = virConnectOpenAuth(libvirt_cmdline, &vAuth, 0);
}

bool VMWareESX::GetConsole(char* console)
{
    /* Unimplemented */
    console[0] = 0;
    return false;
}

int VMWareESX::AuthToVMware(virConnectCredentialPtr cred, unsigned int ncred, void *cbdata)
{
    (void)cbdata;

    for (unsigned int i = 0; i < ncred; i++)
    {
        if (cred[i].type == VIR_CRED_PASSPHRASE)
        {
            cred[i].result = strdup(AppSettings.Specific.VMwareESX.Password);
            if (cred[i].result == NULL)
                return -1;
            cred[i].resultlen = strlen(cred[i].result);
        }
    }

    return 0;
}
