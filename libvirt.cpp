/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Support for libvirt VMs - generic code only
 * COPYRIGHT:   Copyright 2013 Pierre Schweitzer <pierre@reactos.org>
 */

#include "machine.h"

LibVirt::LibVirt()
{
    vConn = NULL;
}

LibVirt::~LibVirt()
{
    if (vConn)
        virConnectClose(vConn);
}

bool LibVirt::IsMachineRunning(const char* name, bool destroy)
{
    int* ids = NULL;
    int numids;
    int maxids = 0;
    const char* domname;
    virDomainPtr vDomPtr = NULL;

    maxids = virConnectNumOfDomains(vConn);
    if (maxids < 0)
        return false;

    ids = new (std::nothrow) int[maxids];
    if (!ids)
        return false;

    numids = virConnectListDomains(vConn, &ids[0], maxids);
    if (numids > -1)
    {
        int i;
        for(i=0; i<numids; i++)
        {
            vDomPtr = virDomainLookupByID(vConn, ids[i]);
            domname = virDomainGetName(vDomPtr);
            if (strcasecmp(name, domname) == 0)
            {
                bool Ret = true;
                /* If asked to destroy the machine, try to do so */
                if (destroy)
                {
                    Ret = (virDomainDestroy(vDomPtr) == -1);
                    if (!Ret) virDomainUndefine(vDomPtr);
                }
                virDomainFree(vDomPtr);
                free(ids);
                return Ret;
            }
            virDomainFree(vDomPtr);
        }
    }
    delete ids;
    return false;
}

void LibVirt::InitializeDisk()
{
    FILE* file;
    char qemu_img_cmdline[300];

    /* If the HD image already exists, delete it */
    if ((file = fopen(AppSettings.HardDiskImage, "r")))
    {
        fclose(file);
        remove(AppSettings.HardDiskImage);
    }

    /* Create a new HD image */
    if (AppSettings.VMType == TYPE_KVM)
    {
        sprintf(qemu_img_cmdline, "qemu-img create -f qcow2 %s %dM",
                AppSettings.HardDiskImage, AppSettings.ImageSize);
    }
    else if (AppSettings.VMType == TYPE_VMWARE_PLAYER)
    {
        sprintf(qemu_img_cmdline, "qemu-img create -f vmdk %s %dM",
                AppSettings.HardDiskImage, AppSettings.ImageSize);
    }

    FILE* p = popen(qemu_img_cmdline, "r");
    char buf[100];
    while(feof(p)==0)
    {
        fgets(buf,100,p);
        SysregPrintf("%s\n",buf);
    }
    pclose(p);
}

bool LibVirt::LaunchMachine(const char* XmlFileName, const char* BootDevice)
{
    xmlDocPtr xml = NULL;
    xmlXPathObjectPtr obj = NULL;
    xmlXPathContextPtr ctxt = NULL;
    char* buffer;
    const char* name;
    char* domname;
    int len = 0;

    buffer = ReadFile(XmlFileName);
    if (buffer == NULL)
        return false;

    xml = xmlReadDoc((const xmlChar *) buffer, "domain.xml", NULL,
                      XML_PARSE_NOENT | XML_PARSE_NONET |
                      XML_PARSE_NOWARNING);
    if (!xml)
        return false;

    ctxt = xmlXPathNewContext(xml);
    if (!ctxt)
        return false;

    obj = xmlXPathEval(BAD_CAST "/domain/os/boot", ctxt);
    if ((obj != NULL) && (obj->type == XPATH_NODESET)
            && (obj->nodesetval != NULL) && (obj->nodesetval->nodeTab != NULL))
    {
        xmlSetProp(obj->nodesetval->nodeTab[0], BAD_CAST"dev", BAD_CAST BootDevice);
    }
    if (obj)
        xmlXPathFreeObject(obj);

    free(buffer);
    xmlDocDumpMemory(xml, (xmlChar**) &buffer, &len);
    xmlFreeDoc(xml);
    xmlXPathFreeContext(ctxt);

    if (AppSettings.VMType == TYPE_VMWARE_PLAYER && !(dynamic_cast<VMWarePlayer *>(this)->StartListeningSocket()))
    {
        return false;
    }

    vDom = virDomainDefineXML(vConn, buffer);
    xmlFree((xmlChar*)buffer);
    if (vDom)
    {
        if (virDomainCreate(vDom) != 0)
        {
            virDomainUndefine(vDom);
            virDomainFree(vDom);
            vDom = NULL;
            return false;
        }
        else
        {
            /* workaround a bug in libvirt */
            name = virDomainGetName(vDom);
            domname = strdup(name);
            virDomainFree(vDom);
            vDom = virDomainLookupByName(vConn, domname);
            free(domname);
            return true;
        }
    }
    else
        return false;
}

const char * LibVirt::GetMachineName() const
{
    return virDomainGetName(vDom);
}

void LibVirt::ShutdownMachine()
{
    virDomainInfo info;

    /* Get VM info in order to shutdown.
     * NB: In case the VM was properly shutdown by ReactOS,
     * This will display an error in output.
     * It can be safely ignored, all fine.
     * Error message with KVM:
     * libvir: QEMU error : Requested operation is not valid: domain is not running
     */
    virDomainGetInfo(vDom, &info);

    /* Kill the VM - if running */
    if (info.state != VIR_DOMAIN_SHUTOFF)
        virDomainDestroy(vDom);

    virDomainUndefine(vDom);
    virDomainFree(vDom);

    if (AppSettings.VMType == TYPE_VMWARE_PLAYER)
    {
        close(AppSettings.Specific.VMwarePlayer.Socket);
        unlink(AppSettings.Specific.VMwarePlayer.Path);
    }
}
