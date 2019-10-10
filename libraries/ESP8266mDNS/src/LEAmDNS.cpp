/*
    LEAmDNS.cpp

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
#include <lwip/netif.h>

#include "LEAmDNS_Priv.h"


namespace esp8266
{

/*
    LEAmDNS
*/
namespace MDNSImplementation
{

/**
    STRINGIZE
*/
#ifndef STRINGIZE
#define STRINGIZE(x) #x
#endif
#ifndef STRINGIZE_VALUE_OF
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)
#endif


/**
    INTERFACE
*/

/**
    MDNSResponder::MDNSResponder
*/
MDNSResponder::MDNSResponder(void)
    :   m_pNetIf(0),
        m_pUDPContext(0),
        m_pcHostname(0),
        m_pServices(0),
        m_pQueries(0),
        m_fnServiceTxtCallback(0),
#ifdef ENABLE_ESP_MDNS_RESPONDER_PASSIV_MODE
        m_bPassivModeEnabled(true)
{
#else
        m_bPassivModeEnabled(false)
{
#endif

    // Set default host probe result callback
    setHostProbeResultCallback([this](MDNSResponder * p_pMDNSResponder, String p_pcDomainName, bool p_bProbeResult)->void
    {
        DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] defaultHostProbeResultCallback: Host domain '%s.local' is %s\n"), p_pcDomainName.c_str(), (p_bProbeResult ? "free" : "already USED!")););

        (void)p_pMDNSResponder;

        if (true == p_bProbeResult)
        {
            // Set station hostname
            MDNSResponder::setStationHostname(p_pcDomainName.c_str());
        }
        else
        {
            // Change hostname, use '-' as divider between base name and index
            char*   pcHostDomainTemp = strdup(p_pcDomainName.c_str());
            if (pcHostDomainTemp)
            {
                if (MDNSResponder::indexDomain(pcHostDomainTemp, "-", 0))
                {
                    setHostname(pcHostDomainTemp);
                }
                else
                {
                    DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] defaultHostProbeResultCallback: FAILED to update host domain (%s)!\n"), p_pcDomainName.c_str()););
                }
                free(pcHostDomainTemp);
            }
            else
            {
                DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] defaultHostProbeResultCallback: FAILED to copy host domain (%s)!\n"), p_pcDomainName.c_str()););
            }
        }
    });
}

/*
    MDNSResponder::~MDNSResponder
*/
MDNSResponder::~MDNSResponder(void)
{

    close();
}

/*
    MDNSResponder::begin (hostname, netif)

    Set the host domain (for probing), install NetIf event handling and
    finally (re)start the responder

*/
bool MDNSResponder::begin(const char* p_pcHostname,
                          netif* p_pNetIf)
{
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] begin(hostname, netif)\n")););

    bool	bResult = false;

    if ((p_pNetIf) &&
            (!m_pNetIf))  	// avoid multiple calls
    {

        bResult = ((_setHostname(p_pcHostname)) &&
                   (_attachNetIf(p_pNetIf)) &&
                   (_restart()));
        DEBUG_EX_ERR(if (!bResult)
    {
        DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] begin: FAILED for '%s'!\n"), (p_pcHostname ? : "-"));
        });
    }
    else if (!p_pNetIf)
    {
        DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] begin: NO network interface given (Ignored host domain: '%s')!\n"), (p_pcHostname ? : "-")););
    }
    else
    {
        DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] begin: Ignoring multiple calls (Ignored host domain: '%s')!\n"), (p_pcHostname ? : "-")););
    }
    return bResult;
}

/*
    MDNSResponder::begin (hostname)
*/
bool MDNSResponder::begin(const char* p_pcHostname)
{
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] begin(hostname)\n")););

    return begin(p_pcHostname, (WiFiMode_t)wifi_get_opmode_default());
}

/*
    MDNSResponder::begin (hostname, WiFiMode)
 **/
bool MDNSResponder::begin(const char* p_pcHostname,
                          WiFiMode_t p_WiFiMode)
{
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] begin(hostname, opmode)\n")););

    bool bResult = false;

    if ((WIFI_STA == p_WiFiMode) ||	// 1
            (WIFI_AP == p_WiFiMode))  	// 2
    {
        DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] begin: WiFi mode %u\n"), (uint32_t)p_WiFiMode););

        bResult = begin(p_pcHostname, netif_get_by_index(p_WiFiMode));
    }
    return bResult;
}

/*
    MDNSResponder::close

    Ends the MDNS responder.
    Announced services are unannounced (by multicasting a goodbye message)

*/
bool MDNSResponder::close(void)
{

    if (m_pUDPContext)
    {
        // Un-announce, if yet connected
        _announce(false, true);
    }
    _resetProbeStatus(false);   // Stop probing

    _releaseQueries();
    _releaseHostname();
    _releaseUDPContext();
    _detachNetIf();
    _releaseServices();

    m_pNetIf = 0;

    return true;
}

/*
    MDNSResponder::setHostname

    Replaces the current hostname and restarts probing.
    For services without own instance name (when the host name was used a instance
    name), the instance names are replaced also (and the probing is restarted).

*/
bool MDNSResponder::setHostname(const char* p_pcHostname)
{

    bool    bResult = false;

    if (_setHostname(p_pcHostname))
    {
        m_HostProbeInformation.m_ProbingStatus = enuProbingStatus::ReadyToStart;

        // Replace 'auto-set' service names
        bResult = true;
        for (stcMDNSService* pService = m_pServices; ((bResult) && (pService)); pService = pService->m_pNext)
        {
            if (pService->m_bAutoName)
            {
                bResult = pService->setName(p_pcHostname);
                pService->m_ProbeInformation.m_ProbingStatus = enuProbingStatus::ReadyToStart;
            }
        }
    }
    DEBUG_EX_ERR(if (!bResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] setHostname: FAILED for '%s'!\n"), (p_pcHostname ? : "-"));
    });
    return bResult;
}

/*
    MDNSResponder::setHostname (LEGACY)
*/
bool MDNSResponder::setHostname(String p_strHostname)
{

    return setHostname(p_strHostname.c_str());
}

/*
    MDNSResponder::hostname
*/
const char* MDNSResponder::hostname(void) const
{

    return m_pcHostname;
}

/*
    MDNSResponder::status
*/
bool MDNSResponder::status(void) const
{

    return (enuProbingStatus::Done == m_HostProbeInformation.m_ProbingStatus);
}

/*
    SERVICES
*/

/*
    MDNSResponder::addService

    Add service; using hostname if no name is explicitly provided for the service
    The usual '_' underline, which is prepended to service and protocol, eg. _http,
    may be given. If not, it is added automatically.

*/
MDNSResponder::hMDNSService MDNSResponder::addService(const char* p_pcName,
        const char* p_pcService,
        const char* p_pcProtocol,
        uint16_t p_u16Port)
{

    hMDNSService    hResult = 0;

    if (((!p_pcName) ||                                                     // NO name OR
            (MDNS_DOMAIN_LABEL_MAXLENGTH >= os_strlen(p_pcName))) &&           // Fitting name
            (p_pcService) &&
            (MDNS_SERVICE_NAME_LENGTH >= os_strlen(p_pcService)) &&
            (p_pcProtocol) &&
            ((MDNS_SERVICE_PROTOCOL_LENGTH - 1) != os_strlen(p_pcProtocol)) &&
            (p_u16Port))
    {

        if (!_findService((p_pcName ? : m_pcHostname), p_pcService, p_pcProtocol))  // Not already used
        {
            if (0 != (hResult = (hMDNSService)_allocService(p_pcName, p_pcService, p_pcProtocol, p_u16Port)))
            {

                // Init probing
                ((stcMDNSService*)hResult)->m_ProbeInformation.m_ProbingStatus = enuProbingStatus::ReadyToStart;

                // Set default service probe result callback
                setServiceProbeResultCallback(hResult, [this](MDNSResponder * p_pMDNSResponder, const char* p_pcServiceName, const hMDNSService p_hMDNSService, bool p_bProbeResult)
                {

                    (void)p_pMDNSResponder;

                    if (false == p_bProbeResult)
                    {
                        // Change service name, use ' #' as divider between base name and index
                        char*   pcServiceNameTemp = strdup(p_pcServiceName);
                        if (pcServiceNameTemp)
                        {
                            if (MDNSResponder::indexDomain(pcServiceNameTemp, " #", 0))
                            {
                                setServiceName(p_hMDNSService, pcServiceNameTemp);
                            }
                            else
                            {
                                DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] defaultServiceProbeResultCallback: FAILED to update service name (%s)!\n"), pcServiceNameTemp););
                            }
                            free(pcServiceNameTemp);
                        }
                        else
                        {
                            DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] defaultServiceProbeResultCallback: FAILED to copy service name (%s)!\n"), pcServiceNameTemp););
                        }
                    }
                });
            }
        }
    }   // else: bad arguments
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] addService: %s to add '%s.%s.%s'!\n"), (hResult ? "Succeeded" : "FAILED"), (p_pcName ? : "-"), p_pcService, p_pcProtocol););
    DEBUG_EX_ERR(if (!hResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] addService: FAILED to add '%s.%s.%s'!\n"), (p_pcName ? : "-"), p_pcService, p_pcProtocol);
    });
    return hResult;
}

/*
    MDNSResponder::removeService

    Unanounce a service (by sending a goodbye message) and remove it
    from the MDNS responder

*/
bool MDNSResponder::removeService(const MDNSResponder::hMDNSService p_hService)
{

    stcMDNSService* pService = 0;
    bool    bResult = (((pService = _findService(p_hService))) &&
                       (_announceService(*pService, false)) &&
                       (_releaseService(pService)));
    DEBUG_EX_ERR(if (!bResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] removeService: FAILED!\n"));
    });
    return bResult;
}

/*
    MDNSResponder::removeService
*/
bool MDNSResponder::removeService(const char* p_pcName,
                                  const char* p_pcService,
                                  const char* p_pcProtocol)
{

    return removeService((hMDNSService)_findService((p_pcName ? : m_pcHostname), p_pcService, p_pcProtocol));
}

/*
    MDNSResponder::addService (LEGACY)
*/
bool MDNSResponder::addService(String p_strService,
                               String p_strProtocol,
                               uint16_t p_u16Port)
{

    return (0 != addService(m_pcHostname, p_strService.c_str(), p_strProtocol.c_str(), p_u16Port));
}

/*
    MDNSResponder::setServiceName
*/
bool MDNSResponder::setServiceName(const MDNSResponder::hMDNSService p_hService,
                                   const char* p_pcInstanceName)
{

    stcMDNSService* pService = 0;
    bool    bResult = (((!p_pcInstanceName) ||
                        (MDNS_DOMAIN_LABEL_MAXLENGTH >= os_strlen(p_pcInstanceName))) &&
                       ((pService = _findService(p_hService))) &&
                       (pService->setName(p_pcInstanceName)) &&
                       ((pService->m_ProbeInformation.m_ProbingStatus = enuProbingStatus::ReadyToStart), true));
    DEBUG_EX_ERR(if (!bResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] setServiceName: FAILED for '%s'!\n"), (p_pcInstanceName ? : "-"));
    });
    return bResult;
}

/*
    MDNSResponder::serviceName
*/
const char* MDNSResponder::serviceName(const hMDNSService p_hService) const
{

    const stcMDNSService* pService = 0;
    return (((pService = _findService(p_hService)))
            ? pService->m_pcName
            : 0);
}

/*
    MDNSResponder::service
*/
const char* MDNSResponder::service(const hMDNSService p_hService) const
{

    const stcMDNSService* pService = 0;
    return (((pService = _findService(p_hService)))
            ? pService->m_pcService
            : 0);
}

/*
    MDNSResponder::serviceProtocol
*/
const char* MDNSResponder::serviceProtocol(const hMDNSService p_hService) const
{

    const stcMDNSService* pService = 0;
    return (((pService = _findService(p_hService)))
            ? pService->m_pcProtocol
            : 0);
}

/*
    MDNSResponder::serviceStatus

    Returns 'true' if probing for the serive 'hMDNSService' is done

*/
bool MDNSResponder::serviceStatus(const hMDNSService p_hService) const
{

    const stcMDNSService* pService = 0;
    return (((pService = _findService(p_hService))) &&
            (enuProbingStatus::Done == pService->m_ProbeInformation.m_ProbingStatus));
}


/*
    SERVICE TXT
*/

/*
    MDNSResponder::addServiceTxt

    Add a static service TXT item ('Key'='Value') to a service.

*/
MDNSResponder::hMDNSTxt MDNSResponder::addServiceTxt(const MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        const char* p_pcValue)
{

    hMDNSTxt    hTxt = 0;
    stcMDNSService* pService = _findService(p_hService);
    if (pService)
    {
        hTxt = (hMDNSTxt)_addServiceTxt(pService, p_pcKey, p_pcValue, false);
    }
    DEBUG_EX_ERR(if (!hTxt)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] addServiceTxt: FAILED for '%s=%s'!\n"), (p_pcKey ? : "-"), (p_pcValue ? : "-"));
    });
    return hTxt;
}

/*
    MDNSResponder::addServiceTxt (uint32_t)

    Formats: http://www.cplusplus.com/reference/cstdio/printf/
*/
MDNSResponder::hMDNSTxt MDNSResponder::addServiceTxt(const MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        uint32_t p_u32Value)
{
    char    acBuffer[32];   *acBuffer = 0;
    sprintf(acBuffer, "%u", p_u32Value);

    return addServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addServiceTxt (uint16_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addServiceTxt(const MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        uint16_t p_u16Value)
{
    char    acBuffer[16];   *acBuffer = 0;
    sprintf(acBuffer, "%hu", p_u16Value);

    return addServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addServiceTxt (uint8_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addServiceTxt(const MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        uint8_t p_u8Value)
{
    char    acBuffer[8];    *acBuffer = 0;
    sprintf(acBuffer, "%hhu", p_u8Value);

    return addServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addServiceTxt (int32_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addServiceTxt(const MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        int32_t p_i32Value)
{
    char    acBuffer[32];   *acBuffer = 0;
    sprintf(acBuffer, "%i", p_i32Value);

    return addServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addServiceTxt (int16_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addServiceTxt(const MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        int16_t p_i16Value)
{
    char    acBuffer[16];   *acBuffer = 0;
    sprintf(acBuffer, "%hi", p_i16Value);

    return addServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addServiceTxt (int8_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addServiceTxt(const MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        int8_t p_i8Value)
{
    char    acBuffer[8];    *acBuffer = 0;
    sprintf(acBuffer, "%hhi", p_i8Value);

    return addServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::removeServiceTxt

    Remove a static service TXT item from a service.
*/
bool MDNSResponder::removeServiceTxt(const MDNSResponder::hMDNSService p_hService,
                                     const MDNSResponder::hMDNSTxt p_hTxt)
{

    bool    bResult = false;

    stcMDNSService* pService = _findService(p_hService);
    if (pService)
    {
        stcMDNSServiceTxt*  pTxt = _findServiceTxt(pService, p_hTxt);
        if (pTxt)
        {
            bResult = _releaseServiceTxt(pService, pTxt);
        }
    }
    DEBUG_EX_ERR(if (!bResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] removeServiceTxt: FAILED!\n"));
    });
    return bResult;
}

/*
    MDNSResponder::removeServiceTxt
*/
bool MDNSResponder::removeServiceTxt(const MDNSResponder::hMDNSService p_hService,
                                     const char* p_pcKey)
{

    bool    bResult = false;

    stcMDNSService* pService = _findService(p_hService);
    if (pService)
    {
        stcMDNSServiceTxt*  pTxt = _findServiceTxt(pService, p_pcKey);
        if (pTxt)
        {
            bResult = _releaseServiceTxt(pService, pTxt);
        }
    }
    DEBUG_EX_ERR(if (!bResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] removeServiceTxt: FAILED for '%s'!\n"), (p_pcKey ? : "-"));
    });
    return bResult;
}

/*
    MDNSResponder::removeServiceTxt
*/
bool MDNSResponder::removeServiceTxt(const char* p_pcName,
                                     const char* p_pcService,
                                     const char* p_pcProtocol,
                                     const char* p_pcKey)
{

    bool    bResult = false;

    stcMDNSService* pService = _findService((p_pcName ? : m_pcHostname), p_pcService, p_pcProtocol);
    if (pService)
    {
        stcMDNSServiceTxt*  pTxt = _findServiceTxt(pService, p_pcKey);
        if (pTxt)
        {
            bResult = _releaseServiceTxt(pService, pTxt);
        }
    }
    return bResult;
}

/*
    MDNSResponder::addServiceTxt (LEGACY)
*/
bool MDNSResponder::addServiceTxt(const char* p_pcService,
                                  const char* p_pcProtocol,
                                  const char* p_pcKey,
                                  const char* p_pcValue)
{

    return (0 != _addServiceTxt(_findService(m_pcHostname, p_pcService, p_pcProtocol), p_pcKey, p_pcValue, false));
}

/*
    MDNSResponder::addServiceTxt (LEGACY)
*/
bool MDNSResponder::addServiceTxt(String p_strService,
                                  String p_strProtocol,
                                  String p_strKey,
                                  String p_strValue)
{

    return (0 != _addServiceTxt(_findService(m_pcHostname, p_strService.c_str(), p_strProtocol.c_str()), p_strKey.c_str(), p_strValue.c_str(), false));
}

/*
    MDNSResponder::setDynamicServiceTxtCallback (global)

    Set a global callback for dynamic service TXT items. The callback is called, whenever
    service TXT items are needed.

*/
bool MDNSResponder::setDynamicServiceTxtCallback(MDNSResponder::MDNSDynamicServiceTxtCallbackFn p_fnCallback)
{

    m_fnServiceTxtCallback = p_fnCallback;

    return true;
}

/*
    MDNSResponder::setDynamicServiceTxtCallback (service specific)

    Set a service specific callback for dynamic service TXT items. The callback is called, whenever
    service TXT items are needed for the given service.

*/
bool MDNSResponder::setDynamicServiceTxtCallback(MDNSResponder::hMDNSService p_hService,
        MDNSResponder::MDNSDynamicServiceTxtCallbackFn p_fnCallback)
{

    bool    bResult = false;

    stcMDNSService* pService = _findService(p_hService);
    if (pService)
    {
        pService->m_fnTxtCallback = p_fnCallback;

        bResult = true;
    }
    DEBUG_EX_ERR(if (!bResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] setDynamicServiceTxtCallback: FAILED!\n"));
    });
    return bResult;
}

/*
    MDNSResponder::addDynamicServiceTxt
*/
MDNSResponder::hMDNSTxt MDNSResponder::addDynamicServiceTxt(MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        const char* p_pcValue)
{
    //DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] addDynamicServiceTxt (%s=%s)\n"), p_pcKey, p_pcValue););

    hMDNSTxt        hTxt = 0;

    stcMDNSService* pService = _findService(p_hService);
    if (pService)
    {
        hTxt = _addServiceTxt(pService, p_pcKey, p_pcValue, true);
    }
    DEBUG_EX_ERR(if (!hTxt)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] addDynamicServiceTxt: FAILED for '%s=%s'!\n"), (p_pcKey ? : "-"), (p_pcValue ? : "-"));
    });
    return hTxt;
}

/*
    MDNSResponder::addDynamicServiceTxt (uint32_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addDynamicServiceTxt(MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        uint32_t p_u32Value)
{

    char    acBuffer[32];   *acBuffer = 0;
    sprintf(acBuffer, "%u", p_u32Value);

    return addDynamicServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addDynamicServiceTxt (uint16_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addDynamicServiceTxt(MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        uint16_t p_u16Value)
{

    char    acBuffer[16];   *acBuffer = 0;
    sprintf(acBuffer, "%hu", p_u16Value);

    return addDynamicServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addDynamicServiceTxt (uint8_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addDynamicServiceTxt(MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        uint8_t p_u8Value)
{

    char    acBuffer[8];    *acBuffer = 0;
    sprintf(acBuffer, "%hhu", p_u8Value);

    return addDynamicServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addDynamicServiceTxt (int32_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addDynamicServiceTxt(MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        int32_t p_i32Value)
{

    char    acBuffer[32];   *acBuffer = 0;
    sprintf(acBuffer, "%i", p_i32Value);

    return addDynamicServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addDynamicServiceTxt (int16_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addDynamicServiceTxt(MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        int16_t p_i16Value)
{

    char    acBuffer[16];   *acBuffer = 0;
    sprintf(acBuffer, "%hi", p_i16Value);

    return addDynamicServiceTxt(p_hService, p_pcKey, acBuffer);
}

/*
    MDNSResponder::addDynamicServiceTxt (int8_t)
*/
MDNSResponder::hMDNSTxt MDNSResponder::addDynamicServiceTxt(MDNSResponder::hMDNSService p_hService,
        const char* p_pcKey,
        int8_t p_i8Value)
{

    char    acBuffer[8];    *acBuffer = 0;
    sprintf(acBuffer, "%hhi", p_i8Value);

    return addDynamicServiceTxt(p_hService, p_pcKey, acBuffer);
}


/**
    STATIC QUERIES (LEGACY)
*/

/*
    MDNSResponder::queryService

    Perform a (blocking) static service query.
    The arrived answers can be queried by calling:
    - answerHostname (or 'hostname')
    - answerIP (or 'IP')
    - answerPort (or 'port')

*/
uint32_t MDNSResponder::queryService(const char* p_pcService,
                                     const char* p_pcProtocol,
                                     const uint16_t p_u16Timeout /*= MDNS_QUERYSERVICES_WAIT_TIME*/)
{
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] queryService '%s.%s'\n"), p_pcService, p_pcProtocol););

    uint32_t    u32Result = 0;

    stcMDNSQuery*    pQueryQuery = 0;
    if ((p_pcService) &&
            (os_strlen(p_pcService)) &&
            (p_pcProtocol) &&
            (os_strlen(p_pcProtocol)) &&
            (p_u16Timeout) &&
            (_removeLegacyQuery()) &&
            ((pQueryQuery = _allocQuery(stcMDNSQuery::enuQueryType::Service))) &&
            (_buildDomainForService(p_pcService, p_pcProtocol, pQueryQuery->m_Domain)))
    {

        pQueryQuery->m_bLegacyQuery = true;

        if (_sendMDNSQuery(*pQueryQuery))
        {
            // Wait for answers to arrive
            DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] queryService: Waiting %u ms for answers...\n"), p_u16Timeout););
            delay(p_u16Timeout);

            // All answers should have arrived by now -> stop adding new answers
            pQueryQuery->m_bAwaitingAnswers = false;
            u32Result = pQueryQuery->answerCount();
        }
        else    // FAILED to send query
        {
            _removeQuery(pQueryQuery);
        }
    }
    else
    {
        DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] queryService: INVALID input data!\n")););
    }
    return u32Result;
}

/*
    MDNSResponder::queryService (LEGACY)
*/
uint32_t MDNSResponder::queryService(String p_strService,
                                     String p_strProtocol)
{

    return queryService(p_strService.c_str(), p_strProtocol.c_str());
}

/*
    MDNSResponder::queryHost

    Perform a (blocking) static host query.
    The arrived answers can be queried by calling:
    - answerHostname (or 'hostname')
    - answerIP (or 'IP')
    - answerPort (or 'port')

*/
uint32_t MDNSResponder::queryHost(const char* p_pcHostname,
                                  const uint16_t p_u16Timeout /*= MDNS_QUERYSERVICES_WAIT_TIME*/)
{
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] queryHost '%s.local'\n"), p_pcHostname););

    uint32_t    u32Result = 0;

    stcMDNSQuery*    pHostQuery = 0;
    if ((p_pcHostname) &&
            (os_strlen(p_pcHostname)) &&
            (p_u16Timeout) &&
            (_removeLegacyQuery()) &&
            ((pHostQuery = _allocQuery(stcMDNSQuery::enuQueryType::Host))) &&
            (_buildDomainForHost(p_pcHostname, pHostQuery->m_Domain)))
    {

        pHostQuery->m_bLegacyQuery = true;

        if (_sendMDNSQuery(*pHostQuery))
        {
            // Wait for answers to arrive
            DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] queryHost: Waiting %u ms for answers...\n"), p_u16Timeout););
            delay(p_u16Timeout);

            // All answers should have arrived by now -> stop adding new answers
            pHostQuery->m_bAwaitingAnswers = false;
            u32Result = pHostQuery->answerCount();
        }
        else    // FAILED to send query
        {
            _removeQuery(pHostQuery);
        }
    }
    else
    {
        DEBUG_EX_ERR(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] queryHost: INVALID input data!\n")););
    }
    return u32Result;
}

/*
    MDNSResponder::removeQuery

    Remove the last static query (and all answers).

*/
bool MDNSResponder::removeQuery(void)
{

    return _removeLegacyQuery();
}

/*
    MDNSResponder::hasQuery

    Return 'true', if a static query is currently installed

*/
bool MDNSResponder::hasQuery(void)
{

    return (0 != _findLegacyQuery());
}

/*
    MDNSResponder::getQuery

    Return handle to the last static query

*/
MDNSResponder::hMDNSQuery MDNSResponder::getQuery(void)
{

    return ((hMDNSQuery)_findLegacyQuery());
}

/*
    MDNSResponder::answerHostname
*/
const char* MDNSResponder::answerHostname(const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQuery = _findLegacyQuery();
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQuery ? pQuery->answerAtIndex(p_u32AnswerIndex) : 0);

    if ((pSQAnswer) &&
            (pSQAnswer->m_HostDomain.m_u16NameLength) &&
            (!pSQAnswer->m_pcHostDomain))
    {

        char*   pcHostDomain = pSQAnswer->allocHostDomain(pSQAnswer->m_HostDomain.c_strLength());
        if (pcHostDomain)
        {
            pSQAnswer->m_HostDomain.c_str(pcHostDomain);
        }
    }
    return (pSQAnswer ? pSQAnswer->m_pcHostDomain : 0);
}

#ifdef MDNS_IPV4_SUPPORT
/*
    MDNSResponder::answerIPv4
*/
IPAddress MDNSResponder::answerIPv4(const uint32_t p_u32AnswerIndex)
{

    const stcMDNSQuery*                              pQuery = _findLegacyQuery();
    const stcMDNSQuery::stcAnswer*                   pSQAnswer = (pQuery ? pQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    const stcMDNSQuery::stcAnswer::stcIPv4Address*   pIPv4Address = (((pSQAnswer) && (pSQAnswer->m_pIPv4Addresses)) ? pSQAnswer->IPv4AddressAtIndex(0) : 0);
    return (pIPv4Address ? pIPv4Address->m_IPAddress : IPAddress());
}

/*
    MDNSResponder::answerIP
*/
IPAddress MDNSResponder::answerIP(const uint32_t p_u32AnswerIndex)
{

    return answerIPv4(p_u32AnswerIndex);
}
#endif

#ifdef MDNS_IPV6_SUPPORT
/*
    MDNSResponder::answerIPv6
*/
IPAddress MDNSResponder::answerIPv6(const uint32_t p_u32AnswerIndex)
{

    const stcMDNSQuery*                              pQuery = _findLegacyQuery();
    const stcMDNSQuery::stcAnswer*                   pSQAnswer = (pQuery ? pQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    const stcMDNSQuery::stcAnswer::stcIPv6Address*	pIPv6Address = (((pSQAnswer) && (pSQAnswer->m_pIPv6Addresses)) ? pSQAnswer->IPv6AddressAtIndex(0) : 0);
    return (pIPv6Address ? pIPv6Address->m_IPAddress : IPAddress());
}
#endif

/*
    MDNSResponder::answerPort
*/
uint16_t MDNSResponder::answerPort(const uint32_t p_u32AnswerIndex)
{

    const stcMDNSQuery*              pQuery = _findLegacyQuery();
    const stcMDNSQuery::stcAnswer*   pSQAnswer = (pQuery ? pQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return (pSQAnswer ? pSQAnswer->m_u16Port : 0);
}

/*
    MDNSResponder::hostname (LEGACY)
*/
String MDNSResponder::hostname(const uint32_t p_u32AnswerIndex)
{

    return String(answerHostname(p_u32AnswerIndex));
}

#ifdef MDNS_IPV4_SUPPORT
/*
    MDNSResponder::IP (LEGACY)
*/
IPAddress MDNSResponder::IP(const uint32_t p_u32AnswerIndex)
{

    return answerIP(p_u32AnswerIndex);
}
#endif

/*
    MDNSResponder::port (LEGACY)
*/
uint16_t MDNSResponder::port(const uint32_t p_u32AnswerIndex)
{

    return answerPort(p_u32AnswerIndex);
}


/**
    DYNAMIC SERVICE QUERY
*/

/*
    MDNSResponder::installServiceQuery

    Add a dynamic service query and a corresponding callback to the MDNS responder.
    The callback will be called for every answer update.
    The answers can also be queried by calling:
    - answerServiceDomain
    - answerHostDomain
    - answerIPv4Address/answerIPv6Address
    - answerPort
    - answerTxts

*/
MDNSResponder::hMDNSQuery MDNSResponder::installServiceQuery(const char* p_pcService,
        const char* p_pcProtocol,
        MDNSResponder::MDNSQueryCallbackFn p_fnCallback)
{
    hMDNSQuery      hResult = 0;

    stcMDNSQuery*   pQueryQuery = 0;
    if ((p_pcService) &&
            (os_strlen(p_pcService)) &&
            (p_pcProtocol) &&
            (os_strlen(p_pcProtocol)) &&
            (p_fnCallback) &&
            ((pQueryQuery = _allocQuery(stcMDNSQuery::enuQueryType::Service))) &&
            (_buildDomainForService(p_pcService, p_pcProtocol, pQueryQuery->m_Domain)))
    {

        pQueryQuery->m_fnCallback = p_fnCallback;
        pQueryQuery->m_bLegacyQuery = false;

        if (_sendMDNSQuery(*pQueryQuery))
        {
            pQueryQuery->m_u8SentCount = 1;
            pQueryQuery->m_ResendTimeout.reset(MDNS_DYNAMIC_QUERY_RESEND_DELAY);

            hResult = (hMDNSQuery)pQueryQuery;
        }
        else
        {
            _removeQuery(pQueryQuery);
        }
    }
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] installServiceQuery: %s for '%s.%s'!\n\n"), (hResult ? "Succeeded" : "FAILED"), (p_pcService ? : "-"), (p_pcProtocol ? : "-")););
    DEBUG_EX_ERR(if (!hResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] installServiceQuery: FAILED for '%s.%s'!\n\n"), (p_pcService ? : "-"), (p_pcProtocol ? : "-"));
    });
    return hResult;
}

/*
    MDNSResponder::installHostQuery
*/
MDNSResponder::hMDNSQuery MDNSResponder::installHostQuery(const char* p_pcHostname,
        MDNSResponder::MDNSQueryCallbackFn p_fnCallback)
{
    hMDNSQuery       hResult = 0;

    if ((p_pcHostname) &&
            (os_strlen(p_pcHostname)))
    {

        stcMDNS_RRDomain    domain;
        hResult = ((_buildDomainForHost(p_pcHostname, domain))
                   ? _installDomainQuery(domain, stcMDNSQuery::enuQueryType::Host, p_fnCallback)
                   : 0);
    }
    DEBUG_EX_INFO(DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] installHostQuery: %s for '%s.local'!\n\n"), (hResult ? "Succeeded" : "FAILED"), (p_pcHostname ? : "-")););
    DEBUG_EX_ERR(if (!hResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] installHostQuery: FAILED for '%s.local'!\n\n"), (p_pcHostname ? : "-"));
    });
    return hResult;
}

/*
    MDNSResponder::removeQuery

    Remove a dynamic query (and all collected answers) from the MDNS responder

*/
bool MDNSResponder::removeQuery(MDNSResponder::hMDNSQuery p_hQuery)
{

    stcMDNSQuery*    pQueryQuery = 0;
    bool    bResult = (((pQueryQuery = _findQuery(p_hQuery))) &&
                       (_removeQuery(pQueryQuery)));
    DEBUG_EX_ERR(if (!bResult)
{
    DEBUG_OUTPUT.printf_P(PSTR("[MDNSResponder] removeQuery: FAILED!\n"));
    });
    return bResult;
}

/*
    MDNSResponder::answerCount
*/
uint32_t MDNSResponder::answerCount(const MDNSResponder::hMDNSQuery p_hServiceQuery)
{

    stcMDNSQuery*    pQueryQuery = _findQuery(p_hServiceQuery);
    return (pQueryQuery ? pQueryQuery->answerCount() : 0);
}

/*
    MDNSResponder::answerAccessors
*/
MDNSResponder::clsMDNSAnswerAccessorVector MDNSResponder::answerAccessors(const MDNSResponder::hMDNSQuery p_hServiceQuery)
{

    MDNSResponder::clsMDNSAnswerAccessorVector  tempVector;
    for (uint32_t u = 0; u < answerCount(p_hServiceQuery); ++u)
    {
        tempVector.emplace_back(*this, p_hServiceQuery, u);
    }
    return tempVector;
}

/*
    MDNSResponder::hasAnswerServiceDomain
*/
bool MDNSResponder::hasAnswerServiceDomain(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return ((pSQAnswer) &&
            (pSQAnswer->m_QueryAnswerFlags & static_cast<typeQueryAnswerType>(enuQueryAnswerType::ServiceDomain)));
}

/*
    MDNSResponder::answerServiceDomain

    Returns the domain for the given service.
    If not already existing, the string is allocated, filled and attached to the answer.

*/
const char* MDNSResponder::answerServiceDomain(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    // Fill m_pcServiceDomain (if not already done)
    if ((pSQAnswer) &&
            (pSQAnswer->m_ServiceDomain.m_u16NameLength) &&
            (!pSQAnswer->m_pcServiceDomain))
    {

        pSQAnswer->m_pcServiceDomain = pSQAnswer->allocServiceDomain(pSQAnswer->m_ServiceDomain.c_strLength());
        if (pSQAnswer->m_pcServiceDomain)
        {
            pSQAnswer->m_ServiceDomain.c_str(pSQAnswer->m_pcServiceDomain);
        }
    }
    return (pSQAnswer ? pSQAnswer->m_pcServiceDomain : 0);
}

/*
    MDNSResponder::hasAnswerHostDomain
*/
bool MDNSResponder::hasAnswerHostDomain(const MDNSResponder::hMDNSQuery p_hServiceQuery,
                                        const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return ((pSQAnswer) &&
            (pSQAnswer->m_QueryAnswerFlags & static_cast<typeQueryAnswerType>(enuQueryAnswerType::HostDomain)));
}

/*
    MDNSResponder::answerHostDomain

    Returns the host domain for the given service.
    If not already existing, the string is allocated, filled and attached to the answer.

*/
const char* MDNSResponder::answerHostDomain(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    // Fill m_pcHostDomain (if not already done)
    if ((pSQAnswer) &&
            (pSQAnswer->m_HostDomain.m_u16NameLength) &&
            (!pSQAnswer->m_pcHostDomain))
    {

        pSQAnswer->m_pcHostDomain = pSQAnswer->allocHostDomain(pSQAnswer->m_HostDomain.c_strLength());
        if (pSQAnswer->m_pcHostDomain)
        {
            pSQAnswer->m_HostDomain.c_str(pSQAnswer->m_pcHostDomain);
        }
    }
    return (pSQAnswer ? pSQAnswer->m_pcHostDomain : 0);
}

#ifdef MDNS_IPV4_SUPPORT
/*
    MDNSResponder::hasAnswerIPv4Address
*/
bool MDNSResponder::hasAnswerIPv4Address(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return ((pSQAnswer) &&
            (pSQAnswer->m_QueryAnswerFlags & static_cast<typeQueryAnswerType>(enuQueryAnswerType::IPv4Address)));
}

/*
    MDNSResponder::answerIPv4AddressCount
*/
uint32_t MDNSResponder::answerIPv4AddressCount(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return (pSQAnswer ? pSQAnswer->IPv4AddressCount() : 0);
}

/*
    MDNSResponder::answerIPv4Address
*/
IPAddress MDNSResponder::answerIPv4Address(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex,
        const uint32_t p_u32AddressIndex)
{

    stcMDNSQuery*                        	pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer*                 pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    stcMDNSQuery::stcAnswer::stcIPv4Address* pIPv4Address = (pSQAnswer ? pSQAnswer->IPv4AddressAtIndex(p_u32AddressIndex) : 0);
    return (pIPv4Address ? pIPv4Address->m_IPAddress : IPAddress());
}
#endif

#ifdef MDNS_IPV6_SUPPORT
/*
    MDNSResponder::hasAnswerIPv6Address
*/
bool MDNSResponder::hasAnswerIPv6Address(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return ((pSQAnswer) &&
            (pSQAnswer->m_QueryAnswerFlags & static_cast<typeQueryAnswerType>(enuQueryAnswerType::IPv6Address)));
}

/*
    MDNSResponder::answerIPv6AddressCount
*/
uint32_t MDNSResponder::answerIPv6AddressCount(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return (pSQAnswer ? pSQAnswer->IPv6AddressCount() : 0);
}

/*
    MDNSResponder::answerIPv6Address
*/
IPAddress MDNSResponder::answerIPv6Address(const MDNSResponder::hMDNSQuery p_hServiceQuery,
        const uint32_t p_u32AnswerIndex,
        const uint32_t p_u32AddressIndex)
{

    stcMDNSQuery*                            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer*                 pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    stcMDNSQuery::stcAnswer::stcIPv6Address*  pIPv6Address = (pSQAnswer ? pSQAnswer->IPv6AddressAtIndex(p_u32AddressIndex) : 0);
    return (pIPv6Address ? pIPv6Address->m_IPAddress : IPAddress());
}
#endif

/*
    MDNSResponder::hasAnswerPort
*/
bool MDNSResponder::hasAnswerPort(const MDNSResponder::hMDNSQuery p_hServiceQuery,
                                  const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return ((pSQAnswer) &&
            (pSQAnswer->m_QueryAnswerFlags & static_cast<typeQueryAnswerType>(enuQueryAnswerType::Port)));
}

/*
    MDNSResponder::answerPort
*/
uint16_t MDNSResponder::answerPort(const MDNSResponder::hMDNSQuery p_hServiceQuery,
                                   const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return (pSQAnswer ? pSQAnswer->m_u16Port : 0);
}

/*
    MDNSResponder::hasAnswerTxts
*/
bool MDNSResponder::hasAnswerTxts(const MDNSResponder::hMDNSQuery p_hServiceQuery,
                                  const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    return ((pSQAnswer) &&
            (pSQAnswer->m_QueryAnswerFlags & static_cast<typeQueryAnswerType>(enuQueryAnswerType::Txts)));
}

/*
    MDNSResponder::answerTxts

    Returns all TXT items for the given service as a ';'-separated string.
    If not already existing; the string is alloced, filled and attached to the answer.

*/
const char* MDNSResponder::answerTxts(const MDNSResponder::hMDNSQuery p_hServiceQuery,
                                      const uint32_t p_u32AnswerIndex)
{

    stcMDNSQuery*            pQueryQuery = _findQuery(p_hServiceQuery);
    stcMDNSQuery::stcAnswer* pSQAnswer = (pQueryQuery ? pQueryQuery->answerAtIndex(p_u32AnswerIndex) : 0);
    // Fill m_pcTxts (if not already done)
    if ((pSQAnswer) &&
            (pSQAnswer->m_Txts.m_pTxts) &&
            (!pSQAnswer->m_pcTxts))
    {

        pSQAnswer->m_pcTxts = pSQAnswer->allocTxts(pSQAnswer->m_Txts.c_strLength());
        if (pSQAnswer->m_pcTxts)
        {
            pSQAnswer->m_Txts.c_str(pSQAnswer->m_pcTxts);
        }
    }
    return (pSQAnswer ? pSQAnswer->m_pcTxts : 0);
}


/*
    PROBING
*/

/*
    MDNSResponder::setHostProbeResultCallback

    Set a callback for probe results. The callback is called, when probing
    for the host domain failes or succeedes.
    In the case of failure, the domain name should be changed via 'setHostname'
    When succeeded, the host domain will be announced by the MDNS responder.

*/
bool MDNSResponder::setHostProbeResultCallback(MDNSResponder::MDNSHostProbeResultCallbackFn p_fnCallback)
{

    m_HostProbeInformation.m_fnProbeResultCallback = p_fnCallback;

    return true;
}

/*
    MDNSResponder::setServiceProbeResultCallback

    Set a service specific callback for probe results. The callback is called, when probing
    for the service domain failes or succeedes.
    In the case of failure, the service name should be changed via 'setServiceName'.
    When succeeded, the service domain will be announced by the MDNS responder.

*/
bool MDNSResponder::setServiceProbeResultCallback(const MDNSResponder::hMDNSService p_hService,
        MDNSResponder::MDNSServiceProbeResultCallbackFn p_fnCallback)
{
    bool    bResult = false;

    stcMDNSService* pService = _findService(p_hService);
    if (pService)
    {
        pService->m_ProbeInformation.m_fnProbeResultCallback = p_fnCallback;

        bResult = true;
    }
    return bResult;
}


/*
    MISC
*/

/*
    MDNSResponder::notifyNetIfChange

    Should be called, whenever the AP for the MDNS responder changes.
    A bit of this is caught by the event callbacks installed in the constructor.

*/
bool MDNSResponder::notifyNetIfChange(void)
{

    return _restart();
}

/*
    MDNSResponder::update

    Should be called in every 'loop'.

*/
bool MDNSResponder::update(void)
{

    if (m_bPassivModeEnabled)
    {
        m_bPassivModeEnabled = false;
    }
    return _process(true);
}

/*
    MDNSResponder::announce

    Should be called, if the 'configuration' changes. Mainly this will be changes in the TXT items...
*/
bool MDNSResponder::announce(void)
{

    return (_announce(true, true));
}

/*
    MDNSResponder::enableArduino

    Enable the OTA update service.

*/
MDNSResponder::hMDNSService MDNSResponder::enableArduino(uint16_t p_u16Port,
        bool p_bAuthUpload /*= false*/)
{

    hMDNSService    hService = addService(0, "arduino", "tcp", p_u16Port);
    if (hService)
    {
        if ((!addServiceTxt(hService, "tcp_check", "no")) ||
                (!addServiceTxt(hService, "ssh_upload", "no")) ||
                (!addServiceTxt(hService, "board", STRINGIZE_VALUE_OF(ARDUINO_BOARD))) ||
                (!addServiceTxt(hService, "auth_upload", (p_bAuthUpload) ? "yes" : "no")))
        {

            removeService(hService);
            hService = 0;
        }
    }
    return hService;
}


} //namespace MDNSImplementation

} //namespace esp8266


