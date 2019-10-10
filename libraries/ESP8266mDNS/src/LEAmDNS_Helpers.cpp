/*
    LEAmDNS_Helpers.cpp

    License (MIT license):
      Permission is hereby granted, free of charge, to any person obtaining a copy
      of this software and associated documentation files (the "Software"), to deal
      in the Software without restriction, including without limitation the rights
      to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
      copies of the Software, and to permit persons to whom the Software is
      furnished to do so, subject to the following conditions:

      The above copyright notice and this permission notice shall be included in
      all copies or substantial portions of the Software.

      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
      IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
      FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
      AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
      LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
      OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
      THE SOFTWARE.

*/

#include <Schedule.h>

#include "lwip/netif.h"
#include "lwip/igmp.h"
#include "lwip/mld6.h"

#include "LEAmDNS_lwIPdefs.h"
#include "LEAmDNS_Priv.h"


namespace
{

/*
    strrstr (static)

    Backwards search for p_pcPattern in p_pcString
    Based on: https://stackoverflow.com/a/1634398/2778898

*/
const char* strrstr(const char*__restrict p_pcString, const char*__restrict p_pcPattern)
{

    const char* pcResult = 0;

    size_t      stStringLength = (p_pcString ? strlen(p_pcString) : 0);
    size_t      stPatternLength = (p_pcPattern ? strlen(p_pcPattern) : 0);

    if ((stStringLength) &&
            (stPatternLength) &&
            (stPatternLength <= stStringLength))
    {
        // Pattern is shorter or has the same length tham the string

        for (const char* s = (p_pcString + stStringLength - stPatternLength); s >= p_pcString; --s)
        {
            if (0 == strncmp(s, p_pcPattern, stPatternLength))
            {
                pcResult = s;
                break;
            }
        }
    }
    return pcResult;
}


} // anonymous





namespace esp8266
{

/*
    LEAmDNS
*/
namespace MDNSImplementation
{

/**
    HELPERS
*/

/*
    MDNSResponder::indexDomain (static)

    Updates the given domain 'p_rpcHostname' by appending a delimiter and an index number.

    If the given domain already hasa numeric index (after the given delimiter), this index
    incremented. If not, the delimiter and index '2' is added.

    If 'p_rpcHostname' is empty (==0), the given default name 'p_pcDefaultHostname' is used,
    if no default is given, 'esp8266' is used.

*/
/*static*/ bool MDNSResponder::indexDomain(char*& p_rpcDomain,
        const char* p_pcDivider /*= "-"*/,
        const char* p_pcDefaultDomain /*= 0*/)
{

    bool    bResult = false;

    // Ensure a divider exists; use '-' as default
    const char*   pcDivider = (p_pcDivider ? : "-");

    if (p_rpcDomain)
    {
        const char* pFoundDivider = strrstr(p_rpcDomain, pcDivider);
        if (pFoundDivider)          // maybe already extended
        {
            char*         pEnd = 0;
            unsigned long ulIndex = strtoul((pFoundDivider + strlen(pcDivider)), &pEnd, 10);
            if ((ulIndex) &&
                    ((pEnd - p_rpcDomain) == (ptrdiff_t)strlen(p_rpcDomain)) &&
                    (!*pEnd))           // Valid (old) index found
            {

                char    acIndexBuffer[16];
                sprintf(acIndexBuffer, "%lu", (++ulIndex));
                size_t  stLength = ((pFoundDivider - p_rpcDomain + strlen(pcDivider)) + strlen(acIndexBuffer) + 1);
                char*   pNewHostname = new char[stLength];
                if (pNewHostname)
                {
                    memcpy(pNewHostname, p_rpcDomain, (pFoundDivider - p_rpcDomain + strlen(pcDivider)));
                    pNewHostname[pFoundDivider - p_rpcDomain + strlen(pcDivider)] = 0;
                    strcat(pNewHostname, acIndexBuffer);

                    delete[] p_rpcDomain;
                    p_rpcDomain = pNewHostname;

                    bResult = true;
                }
                else
                {
                    DEBUG_EX_ERR(DEBUG_OUTPUT.println(F("[MDNSResponder] indexDomain: FAILED to alloc new hostname!")););
                }
            }
            else
            {
                pFoundDivider = 0;  // Flag the need to (base) extend the hostname
            }
        }

        if (!pFoundDivider)         // not yet extended (or failed to increment extension) -> start indexing
        {
            size_t    stLength = strlen(p_rpcDomain) + (strlen(pcDivider) + 1 + 1);   // Name + Divider + '2' + '\0'
            char*     pNewHostname = new char[stLength];
            if (pNewHostname)
            {
                sprintf(pNewHostname, "%s%s2", p_rpcDomain, pcDivider);

                delete[] p_rpcDomain;
                p_rpcDomain = pNewHostname;

                bResult = true;
            }
            else
            {
                DEBUG_EX_ERR(DEBUG_OUTPUT.println(F("[MDNSResponder] indexDomain: FAILED to alloc new hostname!")););
            }
        }
    }
    else
    {
        // No given host domain, use base or default
        const char* cpcDefaultName = (p_pcDefaultDomain ? : "esp8266");

        size_t      stLength = strlen(cpcDefaultName) + 1;   // '\0'
        p_rpcDomain = new char[stLength];
        if (p_rpcDomain)
        {
            strncpy(p_rpcDomain, cpcDefaultName, stLength);
            bResult = true;
        }
        else
        {
            DEBUG_EX_ERR(DEBUG_OUTPUT.println(F("[MDNSResponder] indexDomain: FAILED to alloc new hostname!")););
        }
    }
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] indexDomain: %s\n"), p_rpcDomain););
    return bResult;
}


/*
    MDNSResponder::setStationHostname (static)

    Sets the staion hostname

*/
/*static*/ bool MDNSResponder::setStationHostname(const char* p_pcHostname)
{

    if (p_pcHostname)
    {
        WiFi.hostname(p_pcHostname);
        DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] MDNSResponder::setStationHostname host name: %s!\n"), p_pcHostname););
    }
    return true;
}


/*
    MDNSResponder::_attachNetIf
*/
bool MDNSResponder::_attachNetIf(netif* p_pNetIf)
{

    bool	bResult = false;

    if ((p_pNetIf) &&
            (!m_pNetIf))
    {

        bResult = true;

        // Set instance's netif
        m_pNetIf = p_pNetIf;
        DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] MDNSResponder::_attachNetIf: Set m_pNetIf %u (%s)!\n"), netif_get_index(m_pNetIf), IPAddress(netif_ip4_addr(m_pNetIf)).toString().c_str()););

        if (STATION_IF == m_pNetIf->num)
        {
            m_GotIPHandler = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP & pEvent)
            {
                (void) pEvent;
                // Ensure that _restart() runs in USER context
                schedule_function([this]()
                {
                    MDNSResponder::_restart();
                });
            });
            m_DisconnectedHandler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected & pEvent)
            {
                (void) pEvent;
                // Ensure that _restart() runs in USER context
                schedule_function([this]()
                {
                    MDNSResponder::_restart();
                });
            });
        }

        // Join multicast group(s)
#ifdef MDNS_IPV4_SUPPORT
        ip_addr_t   multicast_addr_V4 = DNS_MQUERY_IPV4_GROUP_INIT;
        if (!(m_pNetIf->flags & NETIF_FLAG_IGMP))
        {
            DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _attachNetIf: Setting flag: flags & NETIF_FLAG_IGMP\n")););
            m_pNetIf->flags |= NETIF_FLAG_IGMP;

            if (ERR_OK != igmp_start(m_pNetIf))
            {
                DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _attachNetIf: igmp_start FAILED!\n")););
            }
        }
        /*  else {
            DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _attachNetIf: NETIF_FLAG_IGMP flag set\n")); );
            }*/

        bResult = ((bResult) &&
                   (ERR_OK == igmp_joingroup_netif(m_pNetIf, ip_2_ip4(&multicast_addr_V4))));
        DEBUG_EX_ERR(if (!bResult)
    {
        DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _attachNetIf: igmp_joingroup_netif(%s) FAILED!\n"), IPAddress(multicast_addr_V4).toString().c_str());
        });
#endif

#ifdef MDNS_IPV6_SUPPORT
        ip_addr_t   multicast_addr_V6 = DNS_MQUERY_IPV6_GROUP_INIT;
        bResult = ((bResult) &&
                   (ERR_OK == mld6_joingroup_netif(m_pNetIf, ip_2_ip6(&multicast_addr_V6))));
        DEBUG_EX_ERR(if (!bResult)
    {
        DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _attachNetIf: mld6_joingroup_netif FAILED!\n"));
        });
#endif

        DEBUG_EX_ERR(if (!bResult)
    {
        DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _attachNetIf: FAILED!\n"));
        });
    }
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] MDNSResponder::_attachNetIf: Attaching to netif %s!\n"), (bResult ? "succeeded" : "FAILED")););
    return bResult;
}

/*
    MDNSResponder::_detachNetIf
*/
bool MDNSResponder::_detachNetIf(void)
{

    bool	bResult = false;

    if (m_pNetIf)
    {

        // Leave multicast group(s)
        bResult = true;
#ifdef MDNS_IPV4_SUPPORT
        ip_addr_t   multicast_addr_V4 = DNS_MQUERY_IPV4_GROUP_INIT;
        bResult = ((bResult) &&
                   (ERR_OK == igmp_leavegroup_netif(m_pNetIf, ip_2_ip4(&multicast_addr_V4)/*(const struct ip4_addr *)&multicast_addr_V4.u_addr.ip4*/)));
#endif
#ifdef MDNS_IPV6_SUPPORT
        ip_addr_t   multicast_addr_V6 = DNS_MQUERY_IPV6_GROUP_INIT;
        bResult = ((bResult) &&
                   (ERR_OK == mld6_leavegroup_netif(m_pNetIf, ip_2_ip6(&multicast_addr_V6)/*&(multicast_addr_V6.u_addr.ip6)*/)));
#endif

        if (m_pNetIf->num = STATION_IF)
        {
            // Reset WiFi event callbacks
            m_GotIPHandler.reset();
            m_DisconnectedHandler.reset();
        }

        // Remove instance's netif
        m_pNetIf = 0;
    }
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] MDNSResponder::_detachNetIf: Detaching from netif %s!\n"), (bResult ? "succeeded" : "FAILED")););
    return bResult;
}


/*
    UDP CONTEXT
*/

bool MDNSResponder::_callProcess(void)
{
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _callProcess (%lu)\n"), millis()););

    return _process(false);
}

/*
    MDNSResponder::_allocUDPContext

    (Re-)Creates the one-and-only UDP context for the MDNS responder.
    The context listens to the MDNS port (5353).
    The travel-distance for multicast messages is set to 1/255 (via MDNS_MULTICAST_TTL).
    Messages are received via the MDNSResponder '_callProcess' function. CAUTION: This function
    is called from the WiFi stack side of the ESP stack system.

*/
bool MDNSResponder::_allocUDPContext(void)
{
    DEBUG_EX_INFO(DEBUG_OUTPUT.println("[MDNSResponder] _allocUDPContext"););

    bool    bResult = false;

    _releaseUDPContext();

    if ((!m_pUDPContext) &&
            (m_pNetIf) &&
            (netif_is_link_up(m_pNetIf)))
    {

        bool	bHasAnyIPAddress = false;
#ifdef MDNS_IPV4_SUPPORT
        bHasAnyIPAddress |= _getResponderIPAddress(enuIPProtocolType::V4).isSet();
#endif
#ifdef MDNS_IPV6_SUPPORT
        bHasAnyIPAddress |= _getResponderIPAddress(enuIPProtocolType::V6).isSet();
#endif
        if (bHasAnyIPAddress)
        {
            m_pUDPContext = new UdpContext;
            if (m_pUDPContext)
            {
                m_pUDPContext->ref();

                if (m_pUDPContext->listen(IP_ANY_TYPE, DNS_MQUERY_PORT))
                {
                    m_pUDPContext->setMulticastInterface(m_pNetIf);
                    m_pUDPContext->setMulticastTTL(MDNS_MULTICAST_TTL);
                    m_pUDPContext->onRx(std::bind(&MDNSResponder::_callProcess, this));

                    bResult = m_pUDPContext->connect(IP_ANY_TYPE, DNS_MQUERY_PORT);
                }
                if (!bResult)
                {
                    _releaseUDPContext();
                }
                else
                {
                    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _allocUDPContext: Succeeded to alloc UDPContext!\n")););
                }
            }
            else
            {
                DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _allocUDPContext: FAILED to alloc UDPContext!\n")););
            }
        }
        else
        {
            DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _allocUDPContext: NO IP address assigned yet!")););
        }
    }
    return bResult;
}

/*
    MDNSResponder::_releaseUDPContext
*/
bool MDNSResponder::_releaseUDPContext(void)
{

    if (m_pUDPContext)
    {
        m_pUDPContext->unref();
        m_pUDPContext = 0;
    }
    return true;
}


/*
    QUERIES
*/

/*
    MDNSResponder::_allocQuery
*/
MDNSResponder::stcMDNSQuery* MDNSResponder::_allocQuery(MDNSResponder::stcMDNSQuery::enuQueryType p_QueryType)
{

    stcMDNSQuery*    pQuery = new stcMDNSQuery(p_QueryType);
    if (pQuery)
    {
        // Link to query list
        pQuery->m_pNext = m_pQueries;
        m_pQueries = pQuery;
    }
    return m_pQueries;
}

/*
    MDNSResponder::_removeQuery
*/
bool MDNSResponder::_removeQuery(MDNSResponder::stcMDNSQuery* p_pQuery)
{

    bool    bResult = false;

    if (p_pQuery)
    {
        stcMDNSQuery*    pPred = m_pQueries;
        while ((pPred) &&
                (pPred->m_pNext != p_pQuery))
        {
            pPred = pPred->m_pNext;
        }
        if (pPred)
        {
            pPred->m_pNext = p_pQuery->m_pNext;
            delete p_pQuery;
            bResult = true;
        }
        else    // No predecesor
        {
            if (m_pQueries == p_pQuery)
            {
                m_pQueries = p_pQuery->m_pNext;
                delete p_pQuery;
                bResult = true;
            }
            else
            {
                DEBUG_EX_ERR(DEBUG_OUTPUT.println("[MDNSResponder] _releaseQuery: INVALID query!"););
            }
        }
    }
    return bResult;
}

/*
    MDNSResponder::_removeLegacyQuery
*/
bool MDNSResponder::_removeLegacyQuery(void)
{

    stcMDNSQuery*    pLegacyQuery = _findLegacyQuery();
    return (pLegacyQuery ? _removeQuery(pLegacyQuery) : true);
}

/*
    MDNSResponder::_findQuery

    'Convert' hMDNSQuery to stcMDNSQuery* (ensure existance)

*/
MDNSResponder::stcMDNSQuery* MDNSResponder::_findQuery(MDNSResponder::hMDNSQuery p_hQuery)
{

    stcMDNSQuery*    pQuery = m_pQueries;
    while (pQuery)
    {
        if ((hMDNSQuery)pQuery == p_hQuery)
        {
            break;
        }
        pQuery = pQuery->m_pNext;
    }
    return pQuery;
}

/*
    MDNSResponder::_findLegacyQuery
*/
MDNSResponder::stcMDNSQuery* MDNSResponder::_findLegacyQuery(void)
{

    stcMDNSQuery*    pQuery = m_pQueries;
    while (pQuery)
    {
        if (pQuery->m_bLegacyQuery)
        {
            break;
        }
        pQuery = pQuery->m_pNext;
    }
    return pQuery;
}

/*
    MDNSResponder::_releaseQueries
*/
bool MDNSResponder::_releaseQueries(void)
{

    while (m_pQueries)
    {
        stcMDNSQuery*    pNext = m_pQueries->m_pNext;
        delete m_pQueries;
        m_pQueries = pNext;
    }
    return true;
}

/*
    MDNSResponder::_findNextQueryByDomain
*/
MDNSResponder::stcMDNSQuery* MDNSResponder::_findNextQueryByDomain(const stcMDNS_RRDomain& p_Domain,
        const MDNSResponder::stcMDNSQuery::enuQueryType p_QueryType,
        const stcMDNSQuery* p_pPrevQuery)
{
    stcMDNSQuery*    pMatchingQuery = 0;

    stcMDNSQuery*    pQuery = (p_pPrevQuery ? p_pPrevQuery->m_pNext : m_pQueries);
    while (pQuery)
    {
        if (((stcMDNSQuery::enuQueryType::None == p_QueryType) ||
                (pQuery->m_QueryType == p_QueryType)) &&
                (p_Domain == pQuery->m_Domain))
        {

            pMatchingQuery = pQuery;
            break;
        }
        pQuery = pQuery->m_pNext;
    }
    return pMatchingQuery;
}


/*
    HOSTNAME
*/

/*
    MDNSResponder::_setHostname
*/
bool MDNSResponder::_setHostname(const char* p_pcHostname)
{
    //DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] _allocHostname (%s)\n"), p_pcHostname););

    bool    bResult = false;

    _releaseHostname();

    size_t  stLength = 0;
    if ((p_pcHostname) &&
            (MDNS_DOMAIN_LABEL_MAXLENGTH >= (stLength = strlen(p_pcHostname))))   // char max size for a single label
    {
        // Copy in hostname characters as lowercase
        if ((bResult = (0 != (m_pcHostname = new char[stLength + 1]))))
        {
#ifdef MDNS_FORCE_LOWERCASE_HOSTNAME
            size_t i = 0;
            for (; i < stLength; ++i)
            {
                m_pcHostname[i] = (isupper(p_pcHostname[i]) ? tolower(p_pcHostname[i]) : p_pcHostname[i]);
            }
            m_pcHostname[i] = 0;
#else
            strncpy(m_pcHostname, p_pcHostname, (stLength + 1));
#endif
            /*  if (m_pNetIf) {
                netif_set_hostname(m_pNetIf, m_pcHostname);
                }*/
        }
    }
    return bResult;
}

/*
    MDNSResponder::_releaseHostname
*/
bool MDNSResponder::_releaseHostname(void)
{

    if (m_pcHostname)
    {
        delete[] m_pcHostname;
        m_pcHostname = 0;
        if (m_pNetIf)
        {
            netif_set_hostname(m_pNetIf, m_pcHostname);
        }
    }
    return true;
}


/*
    SERVICE
*/

/*
    MDNSResponder::_allocService
*/
MDNSResponder::stcMDNSService* MDNSResponder::_allocService(const char* p_pcName,
        const char* p_pcService,
        const char* p_pcProtocol,
        uint16_t p_u16Port)
{

    stcMDNSService* pService = 0;
    if (((!p_pcName) ||
            (MDNS_DOMAIN_LABEL_MAXLENGTH >= strlen(p_pcName))) &&
            (p_pcService) &&
            (MDNS_SERVICE_NAME_LENGTH >= strlen(p_pcService)) &&
            (p_pcProtocol) &&
            (MDNS_SERVICE_PROTOCOL_LENGTH >= strlen(p_pcProtocol)) &&
            (p_u16Port) &&
            (0 != (pService = new stcMDNSService)) &&
            (pService->setName(p_pcName ? : m_pcHostname)) &&
            (pService->setService(p_pcService)) &&
            (pService->setProtocol(p_pcProtocol)))
    {

        pService->m_bAutoName = (0 == p_pcName);
        pService->m_u16Port = p_u16Port;

        // Add to list (or start list)
        pService->m_pNext = m_pServices;
        m_pServices = pService;
    }
    return pService;
}

/*
    MDNSResponder::_releaseService
*/
bool MDNSResponder::_releaseService(MDNSResponder::stcMDNSService* p_pService)
{

    bool    bResult = false;

    if (p_pService)
    {
        stcMDNSService* pPred = m_pServices;
        while ((pPred) &&
                (pPred->m_pNext != p_pService))
        {
            pPred = pPred->m_pNext;
        }
        if (pPred)
        {
            pPred->m_pNext = p_pService->m_pNext;
            delete p_pService;
            bResult = true;
        }
        else    // No predecesor
        {
            if (m_pServices == p_pService)
            {
                m_pServices = p_pService->m_pNext;
                delete p_pService;
                bResult = true;
            }
            else
            {
                DEBUG_EX_ERR(DEBUG_OUTPUT.println("[MDNSResponder] _releaseService: INVALID service!"););
            }
        }
    }
    return bResult;
}

/*
    MDNSResponder::_releaseServices
*/
bool MDNSResponder::_releaseServices(void)
{

    stcMDNSService* pService = m_pServices;
    while (pService)
    {
        _releaseService(pService);
        pService = m_pServices;
    }
    return true;
}

/*
    MDNSResponder::_findService
*/
MDNSResponder::stcMDNSService* MDNSResponder::_findService(const char* p_pcName,
        const char* p_pcService,
        const char* p_pcProtocol)
{

    stcMDNSService* pService = m_pServices;
    while (pService)
    {
        if ((0 == strcmp(pService->m_pcName, p_pcName)) &&
                (0 == strcmp(pService->m_pcService, p_pcService)) &&
                (0 == strcmp(pService->m_pcProtocol, p_pcProtocol)))
        {

            break;
        }
        pService = pService->m_pNext;
    }
    return pService;
}

/*
    MDNSResponder::_findService (const)
*/
const MDNSResponder::stcMDNSService* MDNSResponder::_findService(const MDNSResponder::hMDNSService p_hService) const
{

    const stcMDNSService*   pService = m_pServices;
    while (pService)
    {
        if (p_hService == (hMDNSService)pService)
        {
            break;
        }
        pService = pService->m_pNext;
    }
    return pService;
}

/*
    MDNSResponder::_findService
*/
MDNSResponder::stcMDNSService* MDNSResponder::_findService(const MDNSResponder::hMDNSService p_hService)
{

    return (stcMDNSService*)(((const MDNSResponder*)this)->_findService(p_hService));
}


/*
    SERVICE TXT
*/

/*
    MDNSResponder::_allocServiceTxt
*/
MDNSResponder::stcMDNSServiceTxt* MDNSResponder::_allocServiceTxt(MDNSResponder::stcMDNSService* p_pService,
        const char* p_pcKey,
        const char* p_pcValue,
        bool p_bTemp)
{

    stcMDNSServiceTxt*  pTxt = 0;

    if ((p_pService) &&
            (p_pcKey) &&
            (MDNS_SERVICE_TXT_MAXLENGTH > (p_pService->m_Txts.length() +
                                           1 +                                 // Length byte
                                           (p_pcKey ? strlen(p_pcKey) : 0) +
                                           1 +                                 // '='
                                           (p_pcValue ? strlen(p_pcValue) : 0))))
    {

        pTxt = new stcMDNSServiceTxt;
        if (pTxt)
        {
            size_t  stLength = (p_pcKey ? strlen(p_pcKey) : 0);
            pTxt->m_pcKey = new char[stLength + 1];
            if (pTxt->m_pcKey)
            {
                strncpy(pTxt->m_pcKey, p_pcKey, stLength); pTxt->m_pcKey[stLength] = 0;
            }

            if (p_pcValue)
            {
                stLength = (p_pcValue ? strlen(p_pcValue) : 0);
                pTxt->m_pcValue = new char[stLength + 1];
                if (pTxt->m_pcValue)
                {
                    strncpy(pTxt->m_pcValue, p_pcValue, stLength); pTxt->m_pcValue[stLength] = 0;
                }
            }
            pTxt->m_bTemp = p_bTemp;

            // Add to list (or start list)
            p_pService->m_Txts.add(pTxt);
        }
    }
    return pTxt;
}

/*
    MDNSResponder::_releaseServiceTxt
*/
bool MDNSResponder::_releaseServiceTxt(MDNSResponder::stcMDNSService* p_pService,
                                       MDNSResponder::stcMDNSServiceTxt* p_pTxt)
{

    return ((p_pService) &&
            (p_pTxt) &&
            (p_pService->m_Txts.remove(p_pTxt)));
}

/*
    MDNSResponder::_updateServiceTxt
*/
MDNSResponder::stcMDNSServiceTxt* MDNSResponder::_updateServiceTxt(MDNSResponder::stcMDNSService* p_pService,
        MDNSResponder::stcMDNSServiceTxt* p_pTxt,
        const char* p_pcValue,
        bool p_bTemp)
{

    if ((p_pService) &&
            (p_pTxt) &&
            (MDNS_SERVICE_TXT_MAXLENGTH > (p_pService->m_Txts.length() -
                                           (p_pTxt->m_pcValue ? strlen(p_pTxt->m_pcValue) : 0) +
                                           (p_pcValue ? strlen(p_pcValue) : 0))))
    {
        p_pTxt->update(p_pcValue);
        p_pTxt->m_bTemp = p_bTemp;
    }
    return p_pTxt;
}

/*
    MDNSResponder::_findServiceTxt
*/
MDNSResponder::stcMDNSServiceTxt* MDNSResponder::_findServiceTxt(MDNSResponder::stcMDNSService* p_pService,
        const char* p_pcKey)
{

    return (p_pService ? p_pService->m_Txts.find(p_pcKey) : 0);
}

/*
    MDNSResponder::_findServiceTxt
*/
MDNSResponder::stcMDNSServiceTxt* MDNSResponder::_findServiceTxt(MDNSResponder::stcMDNSService* p_pService,
        const hMDNSTxt p_hTxt)
{

    return (((p_pService) && (p_hTxt)) ? p_pService->m_Txts.find((stcMDNSServiceTxt*)p_hTxt) : 0);
}

/*
    MDNSResponder::_addServiceTxt
*/
MDNSResponder::stcMDNSServiceTxt* MDNSResponder::_addServiceTxt(MDNSResponder::stcMDNSService* p_pService,
        const char* p_pcKey,
        const char* p_pcValue,
        bool p_bTemp)
{
    stcMDNSServiceTxt*  pResult = 0;

    if ((p_pService) &&
            (p_pcKey) &&
            (strlen(p_pcKey)))
    {

        stcMDNSServiceTxt*  pTxt = p_pService->m_Txts.find(p_pcKey);
        if (pTxt)
        {
            pResult = _updateServiceTxt(p_pService, pTxt, p_pcValue, p_bTemp);
        }
        else
        {
            pResult = _allocServiceTxt(p_pService, p_pcKey, p_pcValue, p_bTemp);
        }
    }
    return pResult;
}

/*
    MDNSResponder::_answerKeyValue
*/
MDNSResponder::stcMDNSServiceTxt* MDNSResponder::_answerKeyValue(const MDNSResponder::hMDNSQuery p_hQuery,
        const uint32_t p_u32AnswerIndex)
{
    stcMDNSQuery*            pQuery = _findQuery(p_hQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQuery ? pQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    // Fill m_pcTxts (if not already done)
    return (pSQAnswer) ?  pSQAnswer->m_Txts.m_pTxts : 0;
}

/*
    MDNSResponder::_collectServiceTxts
*/
bool MDNSResponder::_collectServiceTxts(MDNSResponder::stcMDNSService& p_rService)
{

    if (m_fnServiceTxtCallback)
    {
        m_fnServiceTxtCallback(this, (hMDNSService)&p_rService);
    }
    if (p_rService.m_fnTxtCallback)
    {
        p_rService.m_fnTxtCallback(this, (hMDNSService)&p_rService);
    }
    return true;
}

/*
    MDNSResponder::_releaseTempServiceTxts
*/
bool MDNSResponder::_releaseTempServiceTxts(MDNSResponder::stcMDNSService& p_rService)
{

    return (p_rService.m_Txts.removeTempTxts());
}


/*
    MISC
*/

#ifdef DEBUG_ESP_MDNS_RESPONDER
/*
    MDNSResponder::_printRRDomain
*/
bool MDNSResponder::_printRRDomain(const MDNSResponder::stcMDNS_RRDomain& p_RRDomain) const
{

    //DEBUG_OUTPUT.printf_P(PSTR("Domain: "));

    const char* pCursor = p_RRDomain.m_acName;
    uint8_t     u8Length = *pCursor++;
    if (u8Length)
    {
        while (u8Length)
        {
            for (uint8_t u = 0; u < u8Length; ++u)
            {
                DEBUG_OUTPUT.printf_P(PSTR("%c"), *(pCursor++));
            }
            u8Length = *pCursor++;
            if (u8Length)
            {
                DEBUG_OUTPUT.printf_P(PSTR("."));
            }
        }
    }
    else    // empty domain
    {
        DEBUG_OUTPUT.printf_P(PSTR("-empty-"));
    }
    //DEBUG_OUTPUT.printf_P(PSTR("\n"));

    return true;
}

/*
    MDNSResponder::_printRRAnswer
*/
bool MDNSResponder::_printRRAnswer(const MDNSResponder::stcMDNS_RRAnswer& p_RRAnswer) const
{

    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] RRAnswer: "));
    _printRRDomain(p_RRAnswer.m_Header.m_Domain);
    DEBUG_OUTPUT.printf_P(PSTR(" Type:0x%04X Class:0x%04X TTL:%u, "), p_RRAnswer.m_Header.m_Attributes.m_u16Type, p_RRAnswer.m_Header.m_Attributes.m_u16Class, p_RRAnswer.m_u32TTL);
    switch (p_RRAnswer.m_Header.m_Attributes.m_u16Type & (~0x8000))     // Topmost bit might carry 'cache flush' flag
    {
#ifdef MDNS_IPV4_SUPPORT
    case DNS_RRTYPE_A:
        DEBUG_OUTPUT.printf_P(PSTR("A IP:%s"), ((const stcMDNS_RRAnswerA*)&p_RRAnswer)->m_IPAddress.toString().c_str());
        break;
#endif
    case DNS_RRTYPE_PTR:
        DEBUG_OUTPUT.printf_P(PSTR("PTR "));
        _printRRDomain(((const stcMDNS_RRAnswerPTR*)&p_RRAnswer)->m_PTRDomain);
        break;
    case DNS_RRTYPE_TXT:
    {
        size_t  stTxtLength = ((const stcMDNS_RRAnswerTXT*)&p_RRAnswer)->m_Txts.c_strLength();
        char*   pTxts = new char[stTxtLength];
        if (pTxts)
        {
            ((/*const c_str()!!*/stcMDNS_RRAnswerTXT*)&p_RRAnswer)->m_Txts.c_str(pTxts);
            DEBUG_OUTPUT.printf_P(PSTR("TXT(%u) %s"), stTxtLength, pTxts);
            delete[] pTxts;
        }
        break;
    }
#ifdef MDNS_IPV6_SUPPORT
    case DNS_RRTYPE_AAAA:
        DEBUG_OUTPUT.printf_P(PSTR("AAAA IP:%s"), ((stcMDNS_RRAnswerAAAA*&)p_RRAnswer)->m_IPAddress.toString().c_str());
        break;
#endif
    case DNS_RRTYPE_SRV:
        DEBUG_OUTPUT.printf_P(PSTR("SRV Port:%u "), ((const stcMDNS_RRAnswerSRV*)&p_RRAnswer)->m_u16Port);
        _printRRDomain(((const stcMDNS_RRAnswerSRV*)&p_RRAnswer)->m_SRVDomain);
        break;
    default:
        DEBUG_OUTPUT.printf_P(PSTR("generic "));
        break;
    }
    DEBUG_OUTPUT.printf_P(PSTR("\n"));

    return true;
}
/*
    MDNSResponder::_RRType2Name
*/
const char* MDNSResponder::_RRType2Name(uint16_t p_u16RRType) const
{

    static char acRRName[16];
    *acRRName = 0;

    switch (p_u16RRType & (~0x8000))    // Topmost bit might carry 'cache flush' flag
    {
    case DNS_RRTYPE_A:      strcpy(acRRName, "A");      break;
    case DNS_RRTYPE_PTR:    strcpy(acRRName, "PTR");    break;
    case DNS_RRTYPE_TXT:    strcpy(acRRName, "TXT");    break;
    case DNS_RRTYPE_AAAA:   strcpy(acRRName, "AAAA");   break;
    case DNS_RRTYPE_SRV:    strcpy(acRRName, "SRV");    break;
    case DNS_RRTYPE_NSEC:   strcpy(acRRName, "NSEC");   break;
    default:
        sprintf(acRRName, "Unknown(0x%04X)", p_u16RRType);  // MAX 15!
    }
    return acRRName;
}
/*
    MDNSResponder::_RRClass2String
*/
const char* MDNSResponder::_RRClass2String(uint16_t p_u16RRClass,
        bool p_bIsQuery) const
{

    static char acClassString[16];
    *acClassString = 0;

    if (p_u16RRClass & 0x0001)
    {
        strcat(acClassString, "IN ");    //  3
    }
    if (p_u16RRClass & 0x8000)
    {
        strcat(acClassString, (p_bIsQuery ? "UNICAST " : "FLUSH "));    //  8/6
    }

    return acClassString;                                                                       // 11
}
/*
    MDNSResponder::_replyFlags2String
*/
const char* MDNSResponder::_replyFlags2String(uint32_t p_u32ReplyFlags) const
{

    static char acFlagsString[64];

    *acFlagsString = 0;
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::A))
    {
        strcat(acFlagsString, "A ");    //  2
    }
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::PTR_IPv4))
    {
        strcat(acFlagsString, "PTR_IPv4 ");    //  7
    }
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::PTR_IPv6))
    {
        strcat(acFlagsString, "PTR_IPv6 ");    //  7
    }
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::AAAA))
    {
        strcat(acFlagsString, "AAAA ");    //  5
    }
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::PTR_TYPE))
    {
        strcat(acFlagsString, "PTR_TYPE ");    //  9
    }
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::PTR_NAME))
    {
        strcat(acFlagsString, "PTR_NAME ");    //  9
    }
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::TXT))
    {
        strcat(acFlagsString, "TXT ");    //  4
    }
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::SRV))
    {
        strcat(acFlagsString, "SRV ");    //  4
    }
    if (p_u32ReplyFlags & static_cast<uint32_t>(enuContentFlag::NSEC))
    {
        strcat(acFlagsString, "NSEC ");    //  5
    }

    if (0 == p_u32ReplyFlags)
    {
        strcpy(acFlagsString, "none");
    }

    return acFlagsString;                                                                           // 63
}
#endif

}   // namespace MDNSImplementation

} // namespace esp8266




