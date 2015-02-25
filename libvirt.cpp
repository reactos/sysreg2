/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Support for libvirt VMs - generic code only
 * COPYRIGHT:   Copyright 2008-2009 Christoph von Wittich <christoph_vw@reactos.org>
 *              Copyright 2009 Colin Finck <colin@reactos.org>
 *              Copyright 2012-2015 Pierre Schweitzer <pierre@reactos.org>
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
    bool Ret;
    virDomainPtr vDomPtr = NULL;

    vDomPtr = virDomainLookupByName(vConn, name);
    if (vDomPtr == 0)
        return false;

    Ret = (virDomainIsActive(vDomPtr) == 1);
    if (!destroy)
    {
        virDomainFree(vDomPtr);
        return Ret;
    }

    if (Ret)
        Ret = (virDomainDestroy(vDomPtr) == 0);
    virDomainUndefine(vDomPtr);

    virDomainFree(vDomPtr);

    return Ret;
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
        sprintf(qemu_img_cmdline, "qemu-img create -f raw %s %dM",
                AppSettings.HardDiskImage, AppSettings.ImageSize);
    }
    else if (AppSettings.VMType == TYPE_VMWARE_PLAYER)
    {
        sprintf(qemu_img_cmdline, "qemu-img create -f vmdk %s %dM",
                AppSettings.HardDiskImage, AppSettings.ImageSize);
    }
    else if (AppSettings.VMType == TYPE_VIRTUALBOX)
    {
        sprintf(qemu_img_cmdline, "qemu-img create -f vdi %s %dM",
                AppSettings.HardDiskImage, AppSettings.ImageSize);
    }

    Execute(qemu_img_cmdline);
}

bool LibVirt::LaunchMachine(const char* XmlFileName, const char* BootDevice)
{
    xmlDocPtr xml = NULL;
    xmlXPathObjectPtr obj = NULL;
    xmlXPathContextPtr ctxt = NULL;
    char* buffer;
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

    if (!PrepareSerialPort())
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
            const char *name = virDomainGetName(vDom);
            char *domname = strdup(name);
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

    for (unsigned int i = 0; i < 12; ++i)
    {
        if (virDomainUndefine(vDom) == 0)
            break;

        sleep(5);
    }
    virDomainFree(vDom);

    CloseSerialPort();
}

bool LibVirt::PrepareSerialPort()
{
    // Do nothing
    return true;
}

void LibVirt::CloseSerialPort()
{
    // Do nothing
    return;
}
