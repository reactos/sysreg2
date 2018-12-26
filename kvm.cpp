/*
 * PROJECT:     ReactOS System Regression Testing Utility
 * LICENSE:     GNU GPLv2 or any later version as published by the Free Software Foundation
 * PURPOSE:     Support for KVM machines through libvirt
 * COPYRIGHT:   Copyright 2008-2009 Christoph von Wittich <christoph_vw@reactos.org>
 *              Copyright 2009 Colin Finck <colin@reactos.org>
 *              Copyright 2012-2013 Pierre Schweitzer <pierre@reactos.org>
 */

#include "machine.h"

KVM::KVM()
{
    vConn = virConnectOpen("qemu:///session");
}

bool KVM::GetConsole(char* console)
{
    xmlDocPtr xml = NULL;
    xmlXPathObjectPtr obj = NULL;
    xmlXPathContextPtr ctxt = NULL;
    char* XmlDoc;
    bool RetVal = false;

    XmlDoc = virDomainGetXMLDesc(vDom, 0);
    if (!XmlDoc)
        return false;

    xml = xmlReadDoc((const xmlChar *) XmlDoc, "domain.xml", NULL,
            XML_PARSE_NOENT | XML_PARSE_NONET |
            XML_PARSE_NOWARNING);
    free(XmlDoc);
    if (!xml)
        return false;

    ctxt = xmlXPathNewContext(xml);
    if (!ctxt)
    {
        xmlFreeDoc(xml);
        return false;
    }

    obj = xmlXPathEval(BAD_CAST "string(/domain/devices/console/@tty)", ctxt);
    if ((obj != NULL) && ((obj->type == XPATH_STRING) &&
                         (obj->stringval != NULL) && (obj->stringval[0] != 0)))
    {
        strcpy(console, (char *)obj->stringval);
        RetVal = true;
    }
    if (obj)
        xmlXPathFreeObject(obj);

    if (!RetVal)
    {
        obj = xmlXPathEval(BAD_CAST "string(/domain/devices/console/source/@path)", ctxt);
        if ((obj != NULL) && ((obj->type == XPATH_STRING) &&
                             (obj->stringval != NULL) && (obj->stringval[0] != 0)))
        {
            strcpy(console, (char *)obj->stringval);
            RetVal = true;
        }
        if (obj)
            xmlXPathFreeObject(obj);
    }

    xmlFreeDoc(xml);
    xmlXPathFreeContext(ctxt);
    return RetVal;
}
