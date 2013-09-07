/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Loading the utility settings
 * COPYRIGHT:   Copyright 2008-2009 Christoph von Wittich <christoph_vw@reactos.org>
 *              Copyright 2009 Colin Finck <colin@reactos.org>
 *              Copyright 2012-2013 Pierre Schweitzer <pierre@reactos.org>
 */

#include "sysreg.h"

bool LoadSettings(const char* XmlConfig)
{
    xmlDocPtr xml = NULL;
    xmlXPathObjectPtr obj = NULL;
    xmlXPathContextPtr ctxt = NULL;
    char TempStr[255];
    int Stage;
    const char* StageNames[] = {
        "firststage",
        "secondstage",
        "thirdstage"
    };

    xml = xmlReadFile(XmlConfig, NULL, 0);
    if (!xml)
        return false;
    ctxt = xmlXPathNewContext(xml);
    if (!ctxt)
    {
        xmlFreeDoc(xml);
        return false;
    }

    obj = xmlXPathEval(BAD_CAST"string(/settings/@file)",ctxt);
    if ((obj != NULL) && ((obj->type == XPATH_STRING) &&
                    (obj->stringval != NULL) && (obj->stringval[0] != 0)))
    {
        strncpy(AppSettings.Filename, (char *)obj->stringval, 254);
    }
    if (obj)
        xmlXPathFreeObject(obj);

    obj = xmlXPathEval(BAD_CAST"string(/settings/@vm)",ctxt);
    if ((obj != NULL) && ((obj->type == XPATH_STRING) &&
                     (obj->stringval != NULL) && (obj->stringval[0] != 0)))
    {
        strncpy(AppSettings.Name, (char *)obj->stringval, 79);
    }
    if (obj)
        xmlXPathFreeObject(obj);

    obj = xmlXPathEval(BAD_CAST"string(/settings/general/vm/@type)",ctxt);
    if ((obj != NULL) && (obj->type == XPATH_STRING))
    {
        if (xmlStrcasecmp(obj->stringval, BAD_CAST"vmwareplayer") == 0)
            AppSettings.VMType = TYPE_VMWARE_PLAYER;
        else if (xmlStrcasecmp(obj->stringval, BAD_CAST"vmwaregsx") == 0)
            AppSettings.VMType = TYPE_VMWARE_GSX;
        else if (xmlStrcasecmp(obj->stringval, BAD_CAST"vmwareesx") == 0)
            AppSettings.VMType = TYPE_VMWARE_ESX;
    }
    if (obj)
        xmlXPathFreeObject(obj);

    /* Get ids & domain */
    if (AppSettings.VMType == TYPE_VMWARE_GSX ||
        AppSettings.VMType == TYPE_VMWARE_ESX)
    {
        obj = xmlXPathEval(BAD_CAST"string(/settings/general/vm/@domain)",ctxt);
        if ((obj != NULL) && (obj->type == XPATH_STRING) && obj->stringval[0] != 0)
        {
            AppSettings.Specific.VMwareESX.Domain = malloc(xmlStrlen(obj->stringval) + 1);
            if (AppSettings.Specific.VMwareESX.Domain)
            {
                strcpy(AppSettings.Specific.VMwareESX.Domain, (char *)obj->stringval);
            }
        }

        if (obj)
            xmlXPathFreeObject(obj);

        obj = xmlXPathEval(BAD_CAST"string(/settings/general/vm/@username)",ctxt);
        if ((obj != NULL) && (obj->type == XPATH_STRING) && obj->stringval[0] != 0)
        {
            AppSettings.Specific.VMwareESX.Username = malloc(xmlStrlen(obj->stringval) + 1);
            if (AppSettings.Specific.VMwareESX.Username)
            {
                strcpy(AppSettings.Specific.VMwareESX.Username, (char *)obj->stringval);
            }
        }

        if (obj)
            xmlXPathFreeObject(obj);

        /* Get password only if there is an user name */
        if (AppSettings.Specific.VMwareESX.Username)
        {
            obj = xmlXPathEval(BAD_CAST"string(/settings/general/vm/@password)",ctxt);
            if ((obj != NULL) && (obj->type == XPATH_STRING) && obj->stringval[0] != 0)
            {
                AppSettings.Specific.VMwareESX.Password = malloc(xmlStrlen(obj->stringval) + 1);
                if (AppSettings.Specific.VMwareESX.Password)
                {
                    strcpy(AppSettings.Specific.VMwareESX.Password, (char *)obj->stringval);
                }
            }

            if (obj)
                xmlXPathFreeObject(obj);
        }
    }
    else if (AppSettings.VMType == TYPE_VMWARE_PLAYER)
    {
        obj = xmlXPathEval(BAD_CAST"string(/settings/general/vm/@serial)",ctxt);
        if ((obj != NULL) && (obj->type == XPATH_STRING) && obj->stringval[0] != 0)
        {
            strcpy(AppSettings.Specific.VMwarePlayer.Path, (char *)obj->stringval);
        }

        if (obj)
            xmlXPathFreeObject(obj);
    }

    obj = xmlXPathEval(BAD_CAST"number(/settings/general/timeout/@ms)",ctxt);
    if ((obj != NULL) && (obj->type == XPATH_NUMBER))
    {
        /* when no value is set - return value is negative
         * which means infinite */
        AppSettings.Timeout = (int)obj->floatval;
    }
    if (obj)
        xmlXPathFreeObject(obj);

    obj = xmlXPathEval(BAD_CAST"number(/settings/general/maxcachehits/@value)",ctxt);
    if ((obj != NULL) && (obj->type == XPATH_NUMBER))
    {
        AppSettings.MaxCacheHits = (unsigned int)obj->floatval;
    }
    if (obj)
        xmlXPathFreeObject(obj);

    obj = xmlXPathEval(BAD_CAST"number(/settings/general/maxretries/@value)",ctxt);
    if ((obj != NULL) && (obj->type == XPATH_NUMBER))
    {
        AppSettings.MaxRetries = (unsigned int)obj->floatval;
    }
    if (obj)
        xmlXPathFreeObject(obj);

    obj = xmlXPathEval(BAD_CAST"number(/settings/general/maxconts/@value)",ctxt);
    if ((obj != NULL) && (obj->type == XPATH_NUMBER))
    {
        AppSettings.MaxConts = (unsigned int)obj->floatval;
    }
    if (obj)
        xmlXPathFreeObject(obj);

    obj = xmlXPathEval(BAD_CAST"number(/settings/general/hdd/@size)",ctxt);
    if ((obj != NULL) && (obj->type == XPATH_NUMBER))
    {
        if (obj->floatval <= 0)
            AppSettings.ImageSize = 512;
        else
            AppSettings.ImageSize = (int)obj->floatval;
    }
    if (obj)
        xmlXPathFreeObject(obj);

    for (Stage = 0; Stage < NUM_STAGES; Stage++)
    {
        strcpy(TempStr, "string(/settings/");
        strcat(TempStr, StageNames[Stage]);
        strcat(TempStr, "/@bootdevice)");
        obj = xmlXPathEval((xmlChar*) TempStr,ctxt);
        if ((obj != NULL) && ((obj->type == XPATH_STRING) &&
                (obj->stringval != NULL) && (obj->stringval[0] != 0)))
        {
            strncpy(AppSettings.Stage[Stage].BootDevice, (char *)obj->stringval, 7);
        }
        if (obj)
            xmlXPathFreeObject(obj);

        strcpy(TempStr, "string(/settings/");
        strcat(TempStr, StageNames[Stage]);
        strcat(TempStr, "/@hookcommand)");
        obj = xmlXPathEval((xmlChar*) TempStr,ctxt);
        if ((obj != NULL) && ((obj->type == XPATH_STRING) &&
                (obj->stringval != NULL) && (obj->stringval[0] != 0)))
        {
            strncpy(AppSettings.Stage[Stage].HookCommand, (char *)obj->stringval, 254);
        }
        if (obj)
            xmlXPathFreeObject(obj);

        strcpy(TempStr, "string(/settings/");
        strcat(TempStr, StageNames[Stage]);
        strcat(TempStr, "/success/@on)");
        obj = xmlXPathEval((xmlChar*) TempStr,ctxt);
        if ((obj != NULL) && ((obj->type == XPATH_STRING) &&
            (obj->stringval != NULL) && (obj->stringval[0] != 0)))
        {
            strncpy(AppSettings.Stage[Stage].Checkpoint, (char *)obj->stringval, 79);
        }
        if (obj)
            xmlXPathFreeObject(obj);
    }
    xmlFreeDoc(xml);
    xmlXPathFreeContext(ctxt);

    xml = xmlReadFile(AppSettings.Filename, NULL, 0);
    if (!xml)
        return false;
    ctxt = xmlXPathNewContext(xml);
    if (!ctxt)
    {
        xmlFreeDoc(xml);
        return false;
    }

    obj = xmlXPathEval(BAD_CAST"string(/domain/devices/disk[@device='disk']/source/@file)",ctxt);
    if ((obj != NULL) && ((obj->type == XPATH_STRING) &&
                     (obj->stringval != NULL) && (obj->stringval[0] != 0)))
    {
        strncpy(AppSettings.HardDiskImage, (char *)obj->stringval, 254);
    }
    if (obj)
        xmlXPathFreeObject(obj);

    xmlFreeDoc(xml);
    xmlXPathFreeContext(ctxt);
    return true;
}
