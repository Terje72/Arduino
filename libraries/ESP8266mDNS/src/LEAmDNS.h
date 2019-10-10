/*
    LEAmDNS.h
    (c) 2018, LaborEtArs

    Version 0.9 beta

    Some notes (from LaborEtArs, 2018):
    Essentially, this is an rewrite of the original EPS8266 Multicast DNS code (ESP8266mDNS).
    The target of this rewrite was to keep the existing interface as stable as possible while
    adding and extending the supported set of mDNS features.
    A lot of the additions were basicly taken from Erik Ekman's lwIP mdns app code.

    Supported mDNS features (in some cases somewhat limited):
    - Presenting a DNS-SD service to interested observers, eg. a http server by presenting _http._tcp service
    - Support for multi-level compressed names in input; in output only a very simple one-leven full-name compression is implemented
    - Probing host and service domains for uniqueness in the local network
    - Tiebreaking while probing is supportet in a very minimalistic way (the 'higher' IP address wins the tiebreak)
    - Announcing available services after successful probing
    - Using fixed service TXT items or
    - Using dynamic service TXT items for presented services (via callback)
    - Remove services (and un-announcing them to the observers by sending goodbye-messages)
    - Static queries for DNS-SD services (creating a fixed answer set after a certain timeout period)
    - Dynamic queries for DNS-SD services with cached and updated answers and user notifications


    Usage:
    In most cases, this implementation should work as a 'drop-in' replacement for the original
    ESP8266 Multicast DNS code. Adjustments to the existing code would only be needed, if some
    of the new features should be used.

    For presenting services:
    In 'setup()':
      Install a callback for the probing of host (and service) domains via 'MDNS.setProbeResultCallback(probeResultCallback, &userData);'
      Register DNS-SD services with 'MDNSResponder::hMDNSService hService = MDNS.addService("MyESP", "http", "tcp", 5000);'
      (Install additional callbacks for the probing of these service domains via 'MDNS.setServiceProbeResultCallback(hService, probeResultCallback, &userData);')
      Add service TXT items with 'MDNS.addServiceTxt(hService, "c#", "1");' or by installing a service TXT callback
      using 'MDNS.setDynamicServiceTxtCallback(dynamicServiceTxtCallback, &userData);' or service specific
      'MDNS.setDynamicServiceTxtCallback(hService, dynamicServiceTxtCallback, &userData);'
      Call MDNS.begin("MyHostname");

    In 'probeResultCallback(MDNSResponder* p_MDNSResponder, const char* p_pcDomain, MDNSResponder:hMDNSService p_hService, bool p_bProbeResult, void* p_pUserdata)':
      Check the probe result and update the host or service domain name if the probe failed

    In 'dynamicServiceTxtCallback(MDNSResponder* p_MDNSResponder, const hMDNSService p_hService, void* p_pUserdata)':
      Add dynamic TXT items by calling 'MDNS.addDynamicServiceTxt(p_hService, "c#", "1");'

    In loop():
      Call 'MDNS.update();'


    For querying services/hosts:
    Static:
      Call 'uint32_t u32AnswerCount = MDNS.queryService("http", "tcp");' or 'MDNS.queryHost("esp8266")';
      Iterate answers by: 'for (uint32_t u=0; u<u32AnswerCount; ++u) { const char* pHostname = MDNS.answerHostname(u); }'
      You should call MDNS.removeQuery() sometimes later (when the answers are not needed anymore)

    Dynamic:
      Install a dynamic service query by calling 'DNSResponder::hMDNSQuery hQuery = MDNS.installServiceQuery("http", "tcp", serviceQueryCallback, &userData);'
      The callback 'serviceQueryCallback(MDNSResponder* p_MDNSResponder, const stcMDNSAnswerAccessor& p_MDNSAnswerAccessor, typeQueryAnswerType p_QueryAnswerTypeFlags, bool p_bSetContent)'
      is called for any change in the answer set.
      Call 'MDNS.removeQuery(hServiceQuery);' when the answers are not needed anymore


    Reference:
    Used mDNS messages:
    A (0x01):               eg. esp8266.local A OP TTL 123.456.789.012
    AAAA (0x1C):            eg. esp8266.local AAAA OP TTL 1234:5678::90
    PTR (0x0C, srv name):   eg. _http._tcp.local PTR OP TTL MyESP._http._tcp.local
    PTR (0x0C, srv type):   eg. _services._dns-sd._udp.local PTR OP TTL _http._tcp.local
    PTR (0x0C, IPv4):        eg. 012.789.456.123.in-addr.arpa PTR OP TTL esp8266.local
    PTR (0x0C, IPv6):        eg. 90.0.0.0.0.0.0.0.0.0.0.0.78.56.34.12.ip6.arpa PTR OP TTL esp8266.local
    SRV (0x21):             eg. MyESP._http._tcp.local SRV OP TTL PRIORITY WEIGHT PORT esp8266.local
    TXT (0x10):             eg. MyESP._http._tcp.local TXT OP TTL c#=1
    NSEC (0x2F):            eg. esp8266.local ... (DNSSEC)

    Some NOT used message types:
    OPT (0x29):             eDNS


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

#ifndef MDNS_H
#define MDNS_H

#include <functional>   // for UdpContext.h
#include <limits>
#include <map>

#include "lwip/netif.h"
#include "WiFiUdp.h"
#include "lwip/udp.h"
#include "debug.h"
#include "include/UdpContext.h"
#include <PolledTimeout.h>

#include "ESP8266WiFi.h"


namespace esp8266
{

/**
    LEAmDNS
*/
namespace MDNSImplementation
{

//this should be user-defined at build time
#ifndef ARDUINO_BOARD
#define ARDUINO_BOARD "generic"
#endif

#define MDNS_IPV4_SUPPORT
#if LWIP_IPV6
#define MDNS_IPV6_SUPPORT	// If we've got IPv6 support, then we need IPv6 support :-)
#endif


#ifdef MDNS_IPV4_SUPPORT
#define MDNS_IPV4_SIZE              4
#endif
#ifdef MDNS_IPV6_SUPPORT
#define MDNS_IPV6_SIZE              16
#endif
/*
    Maximum length for all service txts for one service
*/
#define MDNS_SERVICE_TXT_MAXLENGTH      1300
/*
    Maximum length for a full domain name eg. MyESP._http._tcp.local
*/
#define MDNS_DOMAIN_MAXLENGTH           256
/*
    Maximum length of on label in a domain name (length info fits into 6 bits)
*/
#define MDNS_DOMAIN_LABEL_MAXLENGTH     63
/*
    Maximum length of a service name eg. http
*/
#define MDNS_SERVICE_NAME_LENGTH        15
/*
    Maximum length of a service protocol name eg. tcp
*/
#define MDNS_SERVICE_PROTOCOL_LENGTH    3
/*
    Default timeout for static service queries
*/
#define MDNS_QUERYSERVICES_WAIT_TIME    5000

/*
    DNS_RRTYPE_NSEC
*/
#ifndef DNS_RRTYPE_NSEC
#define DNS_RRTYPE_NSEC             0x2F
#endif


/**
    MDNSResponder
*/
class MDNSResponder
{
public:
    /* INTERFACE */
    MDNSResponder(void);
    virtual ~MDNSResponder(void);

    // Start the MDNS responder by setting the default hostname
    // Later call MDNS::update() in every 'loop' to run the process loop
    // (probing, announcing, responding, ...)

    bool begin(const char* p_pcHostname);
    bool begin(const char* p_pcHostname,
               WiFiMode_t p_WiFiMode);	// Valid: WIFI_STA(1), WIFI_AP(2), Invalid: WIFI_OFF(0), WIFI_AP_STA(3)
    bool begin(const char* p_pcHostname,
               netif* p_pNetIf);

    /*  bool begin(const String& p_strHostname) {return begin(p_strHostname.c_str());}
        // for compatibility
        bool begin(const char* p_pcHostname,
                   IPAddress p_IPAddress,       // ignored
                   uint32_t p_u32TTL = 120);    // ignored
        bool begin(const String& p_strHostname,
                   IPAddress p_IPAddress,       // ignored
                   uint32_t p_u32TTL = 120) {   // ignored
            return begin(p_strHostname.c_str(), p_IPAddress, p_u32TTL);
        }*/
    // Finish MDNS processing
    bool close(void);
    // for ESP32 compatibility
    bool end(void)
    {
        return close();
    }

    // Change hostname (probing is restarted)
    bool setHostname(const char* p_pcHostname);
    // for compatibility...
    bool setHostname(String p_strHostname);

    const char* hostname(void) const;

    // Returns 'true' is host domain probing is done
    bool status(void) const;

    /**
        hMDNSService (opaque handle to access the service)
    */
    using hMDNSService = const void*;

    // Add a new service to the MDNS responder. If no name (instance name) is given (p_pcName = 0)
    // the current hostname is used. If the hostname is changed later, the instance names for
    // these 'auto-named' services are changed to the new name also (and probing is restarted).
    // The usual '_' before p_pcService (eg. http) and protocol (eg. tcp) may be given.
    hMDNSService addService(const char* p_pcName,
                            const char* p_pcService,
                            const char* p_pcProtocol,
                            uint16_t p_u16Port);
    // Removes a service from the MDNS responder
    bool removeService(const hMDNSService p_hService);
    bool removeService(const char* p_pcInstanceName,
                       const char* p_pcServiceName,
                       const char* p_pcProtocol);
    // for compatibility...
    bool addService(String p_strServiceName,
                    String p_strProtocol,
                    uint16_t p_u16Port);


    // Change the services instance name (and restart probing).
    bool setServiceName(const hMDNSService p_hService,
                        const char* p_pcInstanceName);
    //for compatibility
    //Warning: this has the side effect of changing the hostname.
    //TODO: implement instancename different from hostname
    void setInstanceName(const char* p_pcHostname)
    {
        setHostname(p_pcHostname);
    }

    // for ESP32 compatibility
    void setInstanceName(const String& p_strHostname)
    {
        setInstanceName(p_strHostname.c_str());
    }

    const char* serviceName(const hMDNSService p_hService) const;
    const char* service(const hMDNSService p_hService) const;
    const char* serviceProtocol(const hMDNSService p_hService) const;

    bool serviceStatus(const hMDNSService p_hService) const;

    /**
        hMDNSTxt (opaque handle to access the TXT items)
    */
    using hMDNSTxt = const void*;

    // Add a (static) MDNS TXT item ('key' = 'value') to the service
    hMDNSTxt addServiceTxt(const hMDNSService p_hService,
                           const char* p_pcKey,
                           const char* p_pcValue);
    hMDNSTxt addServiceTxt(const hMDNSService p_hService,
                           const char* p_pcKey,
                           uint32_t p_u32Value);
    hMDNSTxt addServiceTxt(const hMDNSService p_hService,
                           const char* p_pcKey,
                           uint16_t p_u16Value);
    hMDNSTxt addServiceTxt(const hMDNSService p_hService,
                           const char* p_pcKey,
                           uint8_t p_u8Value);
    hMDNSTxt addServiceTxt(const hMDNSService p_hService,
                           const char* p_pcKey,
                           int32_t p_i32Value);
    hMDNSTxt addServiceTxt(const hMDNSService p_hService,
                           const char* p_pcKey,
                           int16_t p_i16Value);
    hMDNSTxt addServiceTxt(const hMDNSService p_hService,
                           const char* p_pcKey,
                           int8_t p_i8Value);

    // Remove an existing (static) MDNS TXT item from the service
    bool removeServiceTxt(const hMDNSService p_hService,
                          const hMDNSTxt p_hTxt);
    bool removeServiceTxt(const hMDNSService p_hService,
                          const char* p_pcKey);
    bool removeServiceTxt(const char* p_pcinstanceName,
                          const char* p_pcServiceName,
                          const char* p_pcProtocol,
                          const char* p_pcKey);
    // for compatibility...
    bool addServiceTxt(const char* p_pcService,
                       const char* p_pcProtocol,
                       const char* p_pcKey,
                       const char* p_pcValue);
    bool addServiceTxt(String p_strService,
                       String p_strProtocol,
                       String p_strKey,
                       String p_strValue);

    /**
        MDNSDynamicServiceTxtCallbackFn
        Callback function for dynamic MDNS TXT items
    */
    using MDNSDynamicServiceTxtCallbackFn = std::function<void(MDNSResponder* p_pMDNSResponder,
                                            const hMDNSService p_hService)>;

    // Set a global callback for dynamic MDNS TXT items. The callback function is called
    // every time, a TXT item is needed for one of the installed services.
    bool setDynamicServiceTxtCallback(MDNSDynamicServiceTxtCallbackFn p_fnCallback);

    // Set a service specific callback for dynamic MDNS TXT items. The callback function
    // is called every time, a TXT item is needed for the given service.
    bool setDynamicServiceTxtCallback(const hMDNSService p_hService,
                                      MDNSDynamicServiceTxtCallbackFn p_fnCallback);

    // Add a (dynamic) MDNS TXT item ('key' = 'value') to the service
    // Dynamic TXT items are removed right after one-time use. So they need to be added
    // every time the value s needed (via callback).
    hMDNSTxt addDynamicServiceTxt(hMDNSService p_hService,
                                  const char* p_pcKey,
                                  const char* p_pcValue);
    hMDNSTxt addDynamicServiceTxt(hMDNSService p_hService,
                                  const char* p_pcKey,
                                  uint32_t p_u32Value);
    hMDNSTxt addDynamicServiceTxt(hMDNSService p_hService,
                                  const char* p_pcKey,
                                  uint16_t p_u16Value);
    hMDNSTxt addDynamicServiceTxt(hMDNSService p_hService,
                                  const char* p_pcKey,
                                  uint8_t p_u8Value);
    hMDNSTxt addDynamicServiceTxt(hMDNSService p_hService,
                                  const char* p_pcKey,
                                  int32_t p_i32Value);
    hMDNSTxt addDynamicServiceTxt(hMDNSService p_hService,
                                  const char* p_pcKey,
                                  int16_t p_i16Value);
    hMDNSTxt addDynamicServiceTxt(hMDNSService p_hService,
                                  const char* p_pcKey,
                                  int8_t p_i8Value);

    /**
        hMDNSQuery (opaque handle to access dynamic service queries)
    */
    using hMDNSQuery = const void*;

    // Perform a (static) service/host query. The function returns after p_u16Timeout milliseconds
    // The answers (the number of received answers is returned) can be retrieved by calling
    // - answerHostname (or hostname)
    // - answerIP (or IP)
    // - answerPort (or port)
    uint32_t queryService(const char* p_pcService,
                          const char* p_pcProtocol,
                          const uint16_t p_u16Timeout = MDNS_QUERYSERVICES_WAIT_TIME);
    // for compatibility...
    uint32_t queryService(String p_strService,
                          String p_strProtocol);
    uint32_t queryHost(const char* p_pcHostname,
                       const uint16_t p_u16Timeout = MDNS_QUERYSERVICES_WAIT_TIME);
    bool removeQuery(void);
    bool hasQuery(void);
    hMDNSQuery getQuery(void);

    const char* answerHostname(const uint32_t p_u32AnswerIndex);
    // for compatibility...
    String hostname(const uint32_t p_u32AnswerIndex);
#ifdef MDNS_IPV4_SUPPORT
    IPAddress answerIPv4(const uint32_t p_u32AnswerIndex);
    // for compatibility
    IPAddress answerIP(const uint32_t p_u32AnswerIndex);
    IPAddress IP(const uint32_t p_u32AnswerIndex);
#endif
#ifdef MDNS_IPV6_SUPPORT
    IPAddress answerIPv6(const uint32_t p_u32AnswerIndex);
#endif
    uint16_t answerPort(const uint32_t p_u32AnswerIndex);
    // for compatibility
    uint16_t port(const uint32_t p_u32AnswerIndex);

    /**
        typeQueryAnswerType & enuQueryAnswerType
    */
    using typeQueryAnswerType = uint8_t;
    enum class enuQueryAnswerType : typeQueryAnswerType
    {
        Unknown             = 0x00,
        ServiceDomain       = 0x01,     // Service domain
        HostDomain          = 0x02,     // Host domain
        Port                = 0x04,     // Port
        Txts                = 0x08,     // TXT items
#ifdef MDNS_IPV4_SUPPORT
        IPv4Address         = 0x10,     // IPv4 address
#endif
#ifdef MDNS_IPV6_SUPPORT
        IPv6Address         = 0x20,     // IPv6 address
#endif
    };

    /**
        stcMDNSAnswerAccessor
    */
    struct stcMDNSAnswerAccessor
    {
    protected:
        /**
            stcCompareTxtKey
        */
        struct stcCompareTxtKey
        {
            bool operator()(char const* p_pA, char const* p_pB) const;
        };
    public:
        stcMDNSAnswerAccessor(MDNSResponder& p_rMDNSResponder,
                              hMDNSQuery p_hQuery,
                              uint32_t p_u32AnswerIndex);
        /**
            clsTxtKeyValueMap
        */
        using clsTxtKeyValueMap = std::map<const char*, const char*, stcCompareTxtKey>;

        bool serviceDomainAvailable(void) const;
        const char* serviceDomain(void) const;
        bool hostDomainAvailable(void) const;
        const char* hostDomain(void) const;
        bool hostPortAvailable(void) const;
        uint16_t hostPort(void) const;
#ifdef MDNS_IPV4_SUPPORT
        bool IPv4AddressAvailable(void) const;
        std::vector<IPAddress> IPv4Addresses(void) const;
#endif
#ifdef MDNS_IPV6_SUPPORT
        bool IPv6AddressAvailable(void) const;
        std::vector<IPAddress> IPv6Addresses(void) const;
#endif
        bool txtsAvailable(void) const;
        const char* txts(void) const;
        const clsTxtKeyValueMap& txtKeyValues(void) const;
        const char* txtValue(const char* p_pcKey) const;

        size_t printTo(Print& p_Print) const;

    protected:
        MDNSResponder&      m_rMDNSResponder;
        hMDNSQuery          m_hQuery;
        uint32_t            m_u32AnswerIndex;
        clsTxtKeyValueMap   m_TxtKeyValueMap;
    };

    /**
        MDNSQueryCallbackFn

        Callback function for received answers for dynamic queries
    */
    using MDNSQueryCallbackFn = std::function<void(MDNSResponder* p_pMDNSResponder,
                                const stcMDNSAnswerAccessor& p_MDNSAnswerAccessor,
                                typeQueryAnswerType p_QueryAnswerTypeFlags,          // flags for the updated answer item
                                bool p_bSetContent)>;                                // true: Answer component set, false: component deleted

    // Install a dynamic service/host query. For every received answer (part) the given callback
    // function is called. The query will be updated every time, the TTL for an answer
    // has timed-out.
    // The answers can also be retrieved by calling
    // - answerCount                                service/host (for host queries, this should never be >1)
    // - answerServiceDomain                        service
    // - hasAnswerHostDomain/answerHostDomain       service/host
    // - hasAnswerIPv4Address/answerIPv4Address     service/host
    // - hasAnswerIPv6Address/answerIPv6Address     service/host
    // - hasAnswerPort/answerPort                   service
    // - hasAnswerTxts/answerTxts                   service
    hMDNSQuery installServiceQuery(const char* p_pcService,
                                   const char* p_pcProtocol,
                                   MDNSQueryCallbackFn p_fnCallback);
    hMDNSQuery installHostQuery(const char* p_pcHostname,
                                MDNSQueryCallbackFn p_fnCallback);
    // Remove a dynamic service query
    bool removeQuery(hMDNSQuery p_hQuery);

    uint32_t answerCount(const hMDNSQuery p_hQuery);

    bool hasAnswerServiceDomain(const hMDNSQuery p_hQuery,
                                const uint32_t p_u32AnswerIndex);
    const char* answerServiceDomain(const hMDNSQuery p_hQuery,
                                    const uint32_t p_u32AnswerIndex);
    bool hasAnswerHostDomain(const hMDNSQuery p_hQuery,
                             const uint32_t p_u32AnswerIndex);
    const char* answerHostDomain(const hMDNSQuery p_hQuery,
                                 const uint32_t p_u32AnswerIndex);
#ifdef MDNS_IPV4_SUPPORT
    bool hasAnswerIPv4Address(const hMDNSQuery p_hQuery,
                              const uint32_t p_u32AnswerIndex);
    uint32_t answerIPv4AddressCount(const hMDNSQuery p_hQuery,
                                    const uint32_t p_u32AnswerIndex);
    IPAddress answerIPv4Address(const hMDNSQuery p_hQuery,
                                const uint32_t p_u32AnswerIndex,
                                const uint32_t p_u32AddressIndex);
#endif
#ifdef MDNS_IPV6_SUPPORT
    bool hasAnswerIPv6Address(const hMDNSQuery p_hQuery,
                              const uint32_t p_u32AnswerIndex);
    uint32_t answerIPv6AddressCount(const hMDNSQuery p_hQuery,
                                    const uint32_t p_u32AnswerIndex);
    IPAddress answerIPv6Address(const hMDNSQuery p_hQuery,
                                const uint32_t p_u32AnswerIndex,
                                const uint32_t p_u32AddressIndex);
#endif
    bool hasAnswerPort(const hMDNSQuery p_hQuery,
                       const uint32_t p_u32AnswerIndex);
    uint16_t answerPort(const hMDNSQuery p_hQuery,
                        const uint32_t p_u32AnswerIndex);
    bool hasAnswerTxts(const hMDNSQuery p_hQuery,
                       const uint32_t p_u32AnswerIndex);
    // Get the TXT items as a ';'-separated string
    const char* answerTxts(const hMDNSQuery p_hQuery,
                           const uint32_t p_u32AnswerIndex);

    /**
        clsMDNSAnswerAccessorVector
    */
    using clsMDNSAnswerAccessorVector = std::vector<MDNSResponder::stcMDNSAnswerAccessor>;

    clsMDNSAnswerAccessorVector answerAccessors(const MDNSResponder::hMDNSQuery p_hQuery);

    /**
        MDNSHostProbeResultCallbackFn
        Callback function for host domain probe results
    */
    using MDNSHostProbeResultCallbackFn = std::function<void(MDNSResponder* p_pMDNSResponder,
                                          const char* p_pcDomainName,
                                          bool p_bProbeResult)>;

    // Set a callback function for host probe results
    // The callback function is called, when the probeing for the host domain
    // succeededs or fails.
    // In case of failure, the failed domain name should be changed.
    bool setHostProbeResultCallback(MDNSHostProbeResultCallbackFn p_fnCallback);

    /**
        MDNSServiceProbeResultCallbackFn
        Callback function for service domain probe results
    */
    using MDNSServiceProbeResultCallbackFn = std::function<void(MDNSResponder* p_pMDNSResponder,
            const char* p_pcServiceName,
            const hMDNSService p_hMDNSService,
            bool p_bProbeResult)>;

    // Set a service specific probe result callcack
    bool setServiceProbeResultCallback(const hMDNSService p_hService,
                                       MDNSServiceProbeResultCallbackFn p_fnCallback);

    // Application should call this whenever AP is configured/disabled
    bool notifyNetIfChange(void);

    // 'update' should be called in every 'loop' to run the MDNS processing
    bool update(void);

    // 'announce' can be called every time, the configuration of some service
    // changes. Mainly, this would be changed content of TXT items.
    bool announce(void);

    // Enable OTA update
    hMDNSService enableArduino(uint16_t p_u16Port,
                               bool p_bAuthUpload = false);

    // Domain name helper
    static bool indexDomain(char*& p_rpcDomain,
                            const char* p_pcDivider = "-",
                            const char* p_pcDefaultDomain = 0);
    // Host name helper
    static bool setStationHostname(const char* p_pcHostname);

protected:
    /** Internal CLASSES & STRUCTS **/

    /**
        typeIPProtocolType & enuIPProtocolType
    */
    using typeIPProtocolType = uint8_t;
    enum class enuIPProtocolType : typeIPProtocolType
    {
#ifdef MDNS_IPV4_SUPPORT
        V4	=	0x01,
#endif
#ifdef MDNS_IPV6_SUPPORT
        V6	=	0x02,
#endif
    };

    /**
        stcMDNSServiceTxt
    */
    struct stcMDNSServiceTxt
    {
        stcMDNSServiceTxt* m_pNext;
        char*              m_pcKey;
        char*              m_pcValue;
        bool               m_bTemp;

        stcMDNSServiceTxt(const char* p_pcKey = 0,
                          const char* p_pcValue = 0,
                          bool p_bTemp = false);
        stcMDNSServiceTxt(const stcMDNSServiceTxt& p_Other);
        ~stcMDNSServiceTxt(void);

        stcMDNSServiceTxt& operator=(const stcMDNSServiceTxt& p_Other);
        bool clear(void);

        char* allocKey(size_t p_stLength);
        bool setKey(const char* p_pcKey,
                    size_t p_stLength);
        bool setKey(const char* p_pcKey);
        bool releaseKey(void);

        char* allocValue(size_t p_stLength);
        bool setValue(const char* p_pcValue,
                      size_t p_stLength);
        bool setValue(const char* p_pcValue);
        bool releaseValue(void);

        bool set(const char* p_pcKey,
                 const char* p_pcValue,
                 bool p_bTemp = false);

        bool update(const char* p_pcValue);

        size_t length(void) const;
    };

    /**
        stcMDNSTxts
    */
    struct stcMDNSServiceTxts
    {
        stcMDNSServiceTxt*  m_pTxts;

        stcMDNSServiceTxts(void);
        stcMDNSServiceTxts(const stcMDNSServiceTxts& p_Other);
        ~stcMDNSServiceTxts(void);

        stcMDNSServiceTxts& operator=(const stcMDNSServiceTxts& p_Other);

        bool clear(void);

        bool add(stcMDNSServiceTxt* p_pTxt);
        bool remove(stcMDNSServiceTxt* p_pTxt);

        bool removeTempTxts(void);

        stcMDNSServiceTxt* find(const char* p_pcKey);
        const stcMDNSServiceTxt* find(const char* p_pcKey) const;
        stcMDNSServiceTxt* find(const stcMDNSServiceTxt* p_pTxt);

        uint16_t length(void) const;

        size_t c_strLength(void) const;
        bool c_str(char* p_pcBuffer);

        size_t bufferLength(void) const;
        bool buffer(char* p_pcBuffer);

        bool compare(const stcMDNSServiceTxts& p_Other) const;
        bool operator==(const stcMDNSServiceTxts& p_Other) const;
        bool operator!=(const stcMDNSServiceTxts& p_Other) const;
    };

    /**
        typeContentFlag & enuContentFlag
    */
    using typeContentFlag = uint16_t;
    enum class enuContentFlag : typeContentFlag
    {
        // Host
        A           = 0x0001,
        PTR_IPv4    = 0x0002,
        PTR_IPv6    = 0x0004,
        AAAA        = 0x0008,
        // Service
        PTR_TYPE    = 0x0010,
        PTR_NAME    = 0x0020,
        TXT         = 0x0040,
        SRV         = 0x0080,
        // DNSSEC
        NSEC        = 0x0100,

        PTR         = (PTR_IPv4 | PTR_IPv6 | PTR_TYPE | PTR_NAME)
    };

    /**
        stcMDNS_MsgHeader
    */
    struct stcMDNS_MsgHeader
    {
        uint16_t        m_u16ID;            // Identifier
        bool            m_1bQR      : 1;    // Query/Response flag
        uint8_t         m_4bOpcode  : 4;    // Operation code
        bool            m_1bAA      : 1;    // Authoritative Answer flag
        bool            m_1bTC      : 1;    // Truncation flag
        bool            m_1bRD      : 1;    // Recursion desired
        bool            m_1bRA      : 1;    // Recursion available
        uint8_t         m_3bZ       : 3;    // Zero
        uint8_t         m_4bRCode   : 4;    // Response code
        uint16_t        m_u16QDCount;       // Question count
        uint16_t        m_u16ANCount;       // Answer count
        uint16_t        m_u16NSCount;       // Authority Record count
        uint16_t        m_u16ARCount;       // Additional Record count

        stcMDNS_MsgHeader(uint16_t p_u16ID = 0,
                          bool p_bQR = false,
                          uint8_t p_u8Opcode = 0,
                          bool p_bAA = false,
                          bool p_bTC = false,
                          bool p_bRD = false,
                          bool p_bRA = false,
                          uint8_t p_u8RCode = 0,
                          uint16_t p_u16QDCount = 0,
                          uint16_t p_u16ANCount = 0,
                          uint16_t p_u16NSCount = 0,
                          uint16_t p_u16ARCount = 0);
    };

    /**
        stcMDNS_RRDomain
    */
    struct stcMDNS_RRDomain
    {
        char            m_acName[MDNS_DOMAIN_MAXLENGTH];    // Encoded domain name
        uint16_t        m_u16NameLength;                    // Length (incl. '\0')

        stcMDNS_RRDomain(void);
        stcMDNS_RRDomain(const stcMDNS_RRDomain& p_Other);

        stcMDNS_RRDomain& operator=(const stcMDNS_RRDomain& p_Other);

        bool clear(void);

        bool addLabel(const char* p_pcLabel,
                      bool p_bPrependUnderline = false);

        bool compare(const stcMDNS_RRDomain& p_Other) const;
        bool operator==(const stcMDNS_RRDomain& p_Other) const;
        bool operator!=(const stcMDNS_RRDomain& p_Other) const;
        bool operator>(const stcMDNS_RRDomain& p_Other) const;

        size_t c_strLength(void) const;
        bool c_str(char* p_pcBuffer);
    };

    /**
        stcMDNS_RRAttributes
    */
    struct stcMDNS_RRAttributes
    {
        uint16_t            m_u16Type;      // Type
        uint16_t            m_u16Class;     // Class, nearly always 'IN'

        stcMDNS_RRAttributes(uint16_t p_u16Type = 0,
                             uint16_t p_u16Class = 1 /*DNS_RRCLASS_IN Internet*/);
        stcMDNS_RRAttributes(const stcMDNS_RRAttributes& p_Other);

        stcMDNS_RRAttributes& operator=(const stcMDNS_RRAttributes& p_Other);
    };

    /**
        stcMDNS_RRHeader
    */
    struct stcMDNS_RRHeader
    {
        stcMDNS_RRDomain        m_Domain;
        stcMDNS_RRAttributes    m_Attributes;

        stcMDNS_RRHeader(void);
        stcMDNS_RRHeader(const stcMDNS_RRHeader& p_Other);

        stcMDNS_RRHeader& operator=(const stcMDNS_RRHeader& p_Other);

        bool clear(void);
    };

    /**
        stcMDNS_RRQuestion
    */
    struct stcMDNS_RRQuestion
    {
        stcMDNS_RRQuestion*     m_pNext;
        stcMDNS_RRHeader        m_Header;
        bool                    m_bUnicast;     // Unicast reply requested

        stcMDNS_RRQuestion(void);
    };

    /**
        stcMDNS_NSECBitmap
    */
    struct stcMDNS_NSECBitmap
    {
        uint8_t m_au8BitmapData[6]; // 6 bytes data

        stcMDNS_NSECBitmap(void);

        bool clear(void);
        uint16_t length(void) const;
        bool setBit(uint16_t p_u16Bit);
        bool getBit(uint16_t p_u16Bit) const;
    };

    /**
        typeAnswerType & enuAnswerType
    */
    using typeAnswerType = uint8_t;
    enum class enuAnswerType : typeAnswerType
    {
        A,
        PTR,
        TXT,
        AAAA,
        SRV,
        //NSEC,
        Generic
    };

    /**
        stcMDNS_RRAnswer
    */
    struct stcMDNS_RRAnswer
    {
        stcMDNS_RRAnswer*   m_pNext;
        const enuAnswerType m_AnswerType;
        stcMDNS_RRHeader    m_Header;
        bool                m_bCacheFlush;  // Cache flush command bit
        uint32_t            m_u32TTL;       // Validity time in seconds

        virtual ~stcMDNS_RRAnswer(void);

        enuAnswerType answerType(void) const;

        bool clear(void);

    protected:
        stcMDNS_RRAnswer(enuAnswerType p_AnswerType,
                         const stcMDNS_RRHeader& p_Header,
                         uint32_t p_u32TTL);
    };

#ifdef MDNS_IPV4_SUPPORT
    /**
        stcMDNS_RRAnswerA
    */
    struct stcMDNS_RRAnswerA : public stcMDNS_RRAnswer
    {
        IPAddress           m_IPAddress;

        stcMDNS_RRAnswerA(const stcMDNS_RRHeader& p_Header,
                          uint32_t p_u32TTL);
        ~stcMDNS_RRAnswerA(void);

        bool clear(void);
    };
#endif

    /**
        stcMDNS_RRAnswerPTR
    */
    struct stcMDNS_RRAnswerPTR : public stcMDNS_RRAnswer
    {
        stcMDNS_RRDomain    m_PTRDomain;

        stcMDNS_RRAnswerPTR(const stcMDNS_RRHeader& p_Header,
                            uint32_t p_u32TTL);
        ~stcMDNS_RRAnswerPTR(void);

        bool clear(void);
    };

    /**
        stcMDNS_RRAnswerTXT
    */
    struct stcMDNS_RRAnswerTXT : public stcMDNS_RRAnswer
    {
        stcMDNSServiceTxts  m_Txts;

        stcMDNS_RRAnswerTXT(const stcMDNS_RRHeader& p_Header,
                            uint32_t p_u32TTL);
        ~stcMDNS_RRAnswerTXT(void);

        bool clear(void);
    };

#ifdef MDNS_IPV6_SUPPORT
    /**
        stcMDNS_RRAnswerAAAA
    */
    struct stcMDNS_RRAnswerAAAA : public stcMDNS_RRAnswer
    {
        IPAddress			m_IPAddress;

        stcMDNS_RRAnswerAAAA(const stcMDNS_RRHeader& p_Header,
                             uint32_t p_u32TTL);
        ~stcMDNS_RRAnswerAAAA(void);

        bool clear(void);
    };
#endif

    /**
        stcMDNS_RRAnswerSRV
    */
    struct stcMDNS_RRAnswerSRV : public stcMDNS_RRAnswer
    {
        uint16_t            m_u16Priority;
        uint16_t            m_u16Weight;
        uint16_t            m_u16Port;
        stcMDNS_RRDomain    m_SRVDomain;

        stcMDNS_RRAnswerSRV(const stcMDNS_RRHeader& p_Header,
                            uint32_t p_u32TTL);
        ~stcMDNS_RRAnswerSRV(void);

        bool clear(void);
    };

    /**
        stcMDNS_RRAnswerGeneric
    */
    struct stcMDNS_RRAnswerGeneric : public stcMDNS_RRAnswer
    {
        uint16_t            m_u16RDLength;  // Length of variable answer
        uint8_t*            m_pu8RDData;    // Offset of start of variable answer in packet

        stcMDNS_RRAnswerGeneric(const stcMDNS_RRHeader& p_Header,
                                uint32_t p_u32TTL);
        ~stcMDNS_RRAnswerGeneric(void);

        bool clear(void);
    };


    /**
        typeProbingStatus & enuProbingStatus
    */
    using typeProbingStatus = uint8_t;
    enum class enuProbingStatus : typeProbingStatus
    {
        WaitingForData,
        ReadyToStart,
        InProgress,
        Done
    };

    /**
        stcProbeInformation_Base
    */
    struct stcProbeInformation_Base
    {
        enuProbingStatus                m_ProbingStatus;
        uint8_t                         m_u8SentCount;  // Used for probes and announcements
        esp8266::polledTimeout::oneShot m_Timeout;      // Used for probes and announcements
        bool                            m_bConflict;
        bool                            m_bTiebreakNeeded;

        stcProbeInformation_Base(void);

        bool clear(void);  // No 'virtual' needed, no polymorphic use (save 4 bytes)
    };

    /**
        stcProbeInformation_Host
    */
    struct stcProbeInformation_Host : public stcProbeInformation_Base
    {
        MDNSHostProbeResultCallbackFn       m_fnProbeResultCallback;

        stcProbeInformation_Host(void);

        bool clear(bool p_bClearUserdata = false);
    };

    /**
        stcProbeInformation_Service
    */
    struct stcProbeInformation_Service : public stcProbeInformation_Base
    {
        MDNSServiceProbeResultCallbackFn    m_fnProbeResultCallback;

        stcProbeInformation_Service(void);

        bool clear(bool p_bClearUserdata = false);
    };


    /**
        stcMDNSService
    */
    struct stcMDNSService
    {
        stcMDNSService*                 m_pNext;
        char*                           m_pcName;
        bool                            m_bAutoName;    // Name was set automatically to hostname (if no name was supplied)
        char*                           m_pcService;
        char*                           m_pcProtocol;
        uint16_t                        m_u16Port;
        uint32_t                        m_u32ReplyMask;
        stcMDNSServiceTxts              m_Txts;
        MDNSDynamicServiceTxtCallbackFn m_fnTxtCallback;
        stcProbeInformation_Service     m_ProbeInformation;

        stcMDNSService(const char* p_pcName = 0,
                       const char* p_pcService = 0,
                       const char* p_pcProtocol = 0);
        ~stcMDNSService(void);

        bool setName(const char* p_pcName);
        bool releaseName(void);

        bool setService(const char* p_pcService);
        bool releaseService(void);

        bool setProtocol(const char* p_pcProtocol);
        bool releaseProtocol(void);
    };

    /**
        stcMDNSQuery
    */
    struct stcMDNSQuery
    {
        /**
            stcAnswer
        */
        struct stcAnswer
        {
            /**
                stcTTL
            */
            struct stcTTL
            {
                /**
                    typeTimeoutLevel & enuTimeoutLevel
                */
                using typeTimeoutLevel = uint8_t;
                enum class enuTimeoutLevel : typeTimeoutLevel
                {
                    None        = 0,
                    Base        = 80,
                    Interval    = 5,
                    Final       = 100
                };

                uint32_t                        m_u32TTL;
                esp8266::polledTimeout::oneShot m_TTLTimeout;
                typeTimeoutLevel                m_TimeoutLevel;

                stcTTL(void);
                bool set(uint32_t p_u32TTL);

                bool flagged(void) const;
                bool restart(void);

                bool prepareDeletion(void);
                bool finalTimeoutLevel(void) const;

                unsigned long timeout(void) const;
            };
#ifdef MDNS_IPV4_SUPPORT
            /**
                stcIPv4Address
            */
            struct stcIPv4Address
            {
                stcIPv4Address* m_pNext;
                IPAddress       m_IPAddress;
                stcTTL          m_TTL;

                stcIPv4Address(IPAddress p_IPAddress,
                               uint32_t p_u32TTL = 0);
            };
#endif
#ifdef MDNS_IPV6_SUPPORT
            /**
                stcIPv6Address
            */
            struct stcIPv6Address
            {
                stcIPv6Address* m_pNext;
                IPAddress		m_IPAddress;
                stcTTL          m_TTL;

                stcIPv6Address(IPAddress p_IPAddress,
                               uint32_t p_u32TTL = 0);
            };
#endif

            stcAnswer*              m_pNext;
            // The service domain is the first 'answer' (from PTR answer, using service and protocol) to be set
            // Defines the key for additional answer, like host domain, etc.
            stcMDNS_RRDomain        m_ServiceDomain;    // 1. level answer (PTR), eg. MyESP._http._tcp.local
            char*                   m_pcServiceDomain;
            stcTTL                  m_TTLServiceDomain;
            stcMDNS_RRDomain        m_HostDomain;       // 2. level answer (SRV, using service domain), eg. esp8266.local
            char*                   m_pcHostDomain;
            uint16_t                m_u16Port;          // 2. level answer (SRV, using service domain), eg. 5000
            stcTTL                  m_TTLHostDomainAndPort;
            stcMDNSServiceTxts      m_Txts;             // 2. level answer (TXT, using service domain), eg. c#=1
            char*                   m_pcTxts;
            stcTTL                  m_TTLTxts;
#ifdef MDNS_IPV4_SUPPORT
            stcIPv4Address*         m_pIPv4Addresses;   // 3. level answer (A, using host domain), eg. 123.456.789.012
#endif
#ifdef MDNS_IPV6_SUPPORT
            stcIPv6Address*         m_pIPv6Addresses;   // 3. level answer (AAAA, using host domain), eg. 1234::09
#endif
            typeQueryAnswerType     m_QueryAnswerFlags; // enuQueryAnswerType

            stcAnswer(void);
            ~stcAnswer(void);

            bool clear(void);

            char* allocServiceDomain(size_t p_stLength);
            bool releaseServiceDomain(void);

            char* allocHostDomain(size_t p_stLength);
            bool releaseHostDomain(void);

            char* allocTxts(size_t p_stLength);
            bool releaseTxts(void);

#ifdef MDNS_IPV4_SUPPORT
            bool releaseIPv4Addresses(void);
            bool addIPv4Address(stcIPv4Address* p_pIPv4Address);
            bool removeIPv4Address(stcIPv4Address* p_pIPv4Address);
            const stcIPv4Address* findIPv4Address(const IPAddress& p_IPAddress) const;
            stcIPv4Address* findIPv4Address(const IPAddress& p_IPAddress);
            uint32_t IPv4AddressCount(void) const;
            const stcIPv4Address* IPv4AddressAtIndex(uint32_t p_u32Index) const;
            stcIPv4Address* IPv4AddressAtIndex(uint32_t p_u32Index);
#endif
#ifdef MDNS_IPV6_SUPPORT
            bool releaseIPv6Addresses(void);
            bool addIPv6Address(stcIPv6Address* p_pIPv6Address);
            bool removeIPv6Address(stcIPv6Address* p_pIPv6Address);
            const stcIPv6Address* findIPv6Address(const IPAddress& p_IPAddress) const;
            stcIPv6Address* findIPv6Address(const IPAddress& p_IPAddress);
            uint32_t IPv6AddressCount(void) const;
            const stcIPv6Address* IPv6AddressAtIndex(uint32_t p_u32Index) const;
            stcIPv6Address* IPv6AddressAtIndex(uint32_t p_u32Index);
#endif
        };  //stcAnswer

        /**
            typeQueryType & enuQueryType
        */
        using   typeQueryType = uint8_t;
        enum class enuQueryType : typeQueryType
        {
            None,
            Service,
            Host
        };

        stcMDNSQuery*                   m_pNext;
        enuQueryType                    m_QueryType;
        stcMDNS_RRDomain                m_Domain;       // Type:Service -> _http._tcp.local; Type:Host -> esp8266.local
        MDNSQueryCallbackFn             m_fnCallback;
        bool                            m_bLegacyQuery;
        uint8_t                         m_u8SentCount;
        esp8266::polledTimeout::oneShot m_ResendTimeout;
        bool                            m_bAwaitingAnswers;
        stcAnswer*                      m_pAnswers;

        stcMDNSQuery(const enuQueryType p_QueryType);
        ~stcMDNSQuery(void);

        bool clear(void);

        uint32_t answerCount(void) const;
        const stcAnswer* answerAtIndex(uint32_t p_u32Index) const;
        stcAnswer* answerAtIndex(uint32_t p_u32Index);
        uint32_t indexOfAnswer(const stcAnswer* p_pAnswer) const;

        bool addAnswer(stcAnswer* p_pAnswer);
        bool removeAnswer(stcAnswer* p_pAnswer);

        stcAnswer* findAnswerForServiceDomain(const stcMDNS_RRDomain& p_ServiceDomain);
        stcAnswer* findAnswerForHostDomain(const stcMDNS_RRDomain& p_HostDomain);
    };

    /**
        stcMDNSSendParameter
    */
    struct stcMDNSSendParameter
    {
    protected:
        /**
            stcDomainCacheItem
        */
        struct stcDomainCacheItem
        {
            stcDomainCacheItem*     m_pNext;
            const void*             m_pHostnameOrService;   // Opaque id for host or service domain (pointer)
            bool                    m_bAdditionalData;      // Opaque flag for special info (service domain included)
            uint16_t                m_u16Offset;            // Offset in UDP output buffer

            stcDomainCacheItem(const void* p_pHostnameOrService,
                               bool p_bAdditionalData,
                               uint32_t p_u16Offset);
        };

    public:
        /**
            typeResponseType & enuResponseType
        */
        using typeResponseType = uint8_t;
        enum class enuResponseType : typeResponseType
        {
            None,
            Response,
            Unsolicited
        };

        uint16_t                m_u16ID;                    // Query ID (used only in lagacy queries)
        stcMDNS_RRQuestion*     m_pQuestions;               // A list of queries
        uint32_t                m_u32HostReplyMask;         // Flags for reply components/answers
        bool                    m_bLegacyQuery;             // Flag: Legacy query
        enuResponseType         m_Response;                 // Enum: Response to a query
        bool                    m_bAuthorative;             // Flag: Authorative (owner) response
        bool                    m_bCacheFlush;              // Flag: Clients should flush their caches
        bool                    m_bUnicast;                 // Flag: Unicast response
        bool                    m_bUnannounce;              // Flag: Unannounce service

        // Temp content; created while processing _prepareMessage
        uint16_t                m_u16Offset;                // Current offset in UDP write buffer (mainly for domain cache)
        stcDomainCacheItem*     m_pDomainCacheItems;        // Cached host and service domains

        stcMDNSSendParameter(void);
        ~stcMDNSSendParameter(void);

        bool clear(void);
        bool flushQuestions(void);
        bool flushDomainCache(void);
        bool flushTempContent(void);

        bool shiftOffset(uint16_t p_u16Shift);

        bool addDomainCacheItem(const void* p_pHostnameOrService,
                                bool p_bAdditionalData,
                                uint16_t p_u16Offset);
        uint16_t findCachedDomainOffset(const void* p_pHostnameOrService,
                                        bool p_bAdditionalData) const;
    };

    // Instance variables
    netif*							m_pNetIf;
    UdpContext*                     m_pUDPContext;
    char*                           m_pcHostname;
    stcMDNSService*                 m_pServices;
    stcMDNSQuery*                   m_pQueries;
    WiFiEventHandler                m_DisconnectedHandler;
    WiFiEventHandler                m_GotIPHandler;
    MDNSDynamicServiceTxtCallbackFn m_fnServiceTxtCallback;
    bool                            m_bPassivModeEnabled;
    stcProbeInformation_Host        m_HostProbeInformation;

    /** CONTROL **/
    /* MAINTENANCE */
    bool _process(bool p_bUserContext);
    bool _restart(void);

    /* RECEIVING */
    bool _parseMessage(void);
    bool _parseQuery(const stcMDNS_MsgHeader& p_Header);

    bool _parseResponse(const stcMDNS_MsgHeader& p_Header);
    bool _processAnswers(const stcMDNS_RRAnswer* p_pPTRAnswers);
    bool _processPTRAnswer(const stcMDNS_RRAnswerPTR* p_pPTRAnswer,
                           bool& p_rbFoundNewKeyAnswer);
    bool _processSRVAnswer(const stcMDNS_RRAnswerSRV* p_pSRVAnswer,
                           bool& p_rbFoundNewKeyAnswer);
    bool _processTXTAnswer(const stcMDNS_RRAnswerTXT* p_pTXTAnswer);
#ifdef MDNS_IPV4_SUPPORT
    bool _processAAnswer(const stcMDNS_RRAnswerA* p_pAAnswer);
#endif
#ifdef MDNS_IPV6_SUPPORT
    bool _processAAAAAnswer(const stcMDNS_RRAnswerAAAA* p_pAAAAAnswer);
#endif

    /* PROBING */
    bool _updateProbeStatus(void);
    bool _resetProbeStatus(bool p_bRestart = true);
    bool _hasProbesWaitingForAnswers(void) const;
    bool _sendHostProbe(void);
    bool _sendServiceProbe(stcMDNSService& p_rService);
    bool _cancelProbingForHost(void);
    bool _cancelProbingForService(stcMDNSService& p_rService);

    /* ANNOUNCE */
    bool _announce(bool p_bAnnounce,
                   bool p_bIncludeServices);
    bool _announceService(stcMDNSService& p_rService,
                          bool p_bAnnounce = true);

    /* SERVICE QUERY CACHE */
    stcMDNSQuery* _installDomainQuery(stcMDNS_RRDomain& p_Domain,
                                      stcMDNSQuery::enuQueryType p_QueryType,
                                      MDNSQueryCallbackFn p_fnCallback);
    bool _hasQueriesWaitingForAnswers(void) const;
    bool _checkQueryCache(void);

    /** TRANSFER **/
    /* SENDING */
    bool _sendMDNSMessage(stcMDNSSendParameter& p_SendParameter);
    bool _sendMDNSMessage_Multicast(MDNSResponder::stcMDNSSendParameter& p_rSendParameter,
                                    uint8_t p_IPProtocolTypes);
    bool _prepareMDNSMessage(stcMDNSSendParameter& p_SendParameter);
    bool _addMDNSQueryRecord(stcMDNSSendParameter& p_rSendParameter,
                             const stcMDNS_RRDomain& p_QueryDomain,
                             uint16_t p_u16QueryType);
    bool _sendMDNSQuery(const stcMDNSQuery& p_Query,
                        stcMDNSQuery::stcAnswer* p_pKnownAnswers = 0);
    bool _sendMDNSQuery(const stcMDNS_RRDomain& p_QueryDomain,
                        uint16_t p_u16RecordType,
                        stcMDNSQuery::stcAnswer* p_pKnownAnswers = 0);

    IPAddress _getResponderIPAddress(enuIPProtocolType p_IPProtocolType) const;

    uint32_t _replyMaskForHost(const stcMDNS_RRHeader& p_RRHeader,
                               bool* p_pbFullNameMatch = 0) const;
    uint32_t _replyMaskForService(const stcMDNS_RRHeader& p_RRHeader,
                                  const stcMDNSService& p_Service,
                                  bool* p_pbFullNameMatch = 0) const;

    /* RESOURCE RECORD */
    bool _readRRQuestion(stcMDNS_RRQuestion& p_rQuestion);
    bool _readRRAnswer(stcMDNS_RRAnswer*& p_rpAnswer);
#ifdef MDNS_IPV4_SUPPORT
    bool _readRRAnswerA(stcMDNS_RRAnswerA& p_rRRAnswerA,
                        uint16_t p_u16RDLength);
#endif
    bool _readRRAnswerPTR(stcMDNS_RRAnswerPTR& p_rRRAnswerPTR,
                          uint16_t p_u16RDLength);
    bool _readRRAnswerTXT(stcMDNS_RRAnswerTXT& p_rRRAnswerTXT,
                          uint16_t p_u16RDLength);
#ifdef MDNS_IPV6_SUPPORT
    bool _readRRAnswerAAAA(stcMDNS_RRAnswerAAAA& p_rRRAnswerAAAA,
                           uint16_t p_u16RDLength);
#endif
    bool _readRRAnswerSRV(stcMDNS_RRAnswerSRV& p_rRRAnswerSRV,
                          uint16_t p_u16RDLength);
    bool _readRRAnswerGeneric(stcMDNS_RRAnswerGeneric& p_rRRAnswerGeneric,
                              uint16_t p_u16RDLength);

    bool _readRRHeader(stcMDNS_RRHeader& p_rHeader);
    bool _readRRDomain(stcMDNS_RRDomain& p_rRRDomain);
    bool _readRRDomain_Loop(stcMDNS_RRDomain& p_rRRDomain,
                            uint8_t p_u8Depth);
    bool _readRRAttributes(stcMDNS_RRAttributes& p_rAttributes);

    /* DOMAIN NAMES */
    bool _buildDomainForHost(const char* p_pcHostname,
                             stcMDNS_RRDomain& p_rHostDomain) const;
    bool _buildDomainForDNSSD(stcMDNS_RRDomain& p_rDNSSDDomain) const;
    bool _buildDomainForService(const stcMDNSService& p_Service,
                                bool p_bIncludeName,
                                stcMDNS_RRDomain& p_rServiceDomain) const;
    bool _buildDomainForService(const char* p_pcService,
                                const char* p_pcProtocol,
                                stcMDNS_RRDomain& p_rServiceDomain) const;
#ifdef MDNS_IPV4_SUPPORT
    bool _buildDomainForReverseIPv4(IPAddress p_IPv4Address,
                                    stcMDNS_RRDomain& p_rReverseIPv4Domain) const;
#endif
#ifdef MDNS_IPV6_SUPPORT
    bool _buildDomainForReverseIPv6(IPAddress p_IPv4Address,
                                    stcMDNS_RRDomain& p_rReverseIPv6Domain) const;
#endif

    /* UDP */
    bool _udpReadBuffer(unsigned char* p_pBuffer,
                        size_t p_stLength);
    bool _udpRead8(uint8_t& p_ru8Value);
    bool _udpRead16(uint16_t& p_ru16Value);
    bool _udpRead32(uint32_t& p_ru32Value);

    bool _udpAppendBuffer(const unsigned char* p_pcBuffer,
                          size_t p_stLength);
    bool _udpAppend8(uint8_t p_u8Value);
    bool _udpAppend16(uint16_t p_u16Value);
    bool _udpAppend32(uint32_t p_u32Value);

#if not defined ESP_8266_MDNS_INCLUDE || defined DEBUG_ESP_MDNS_RESPONDER
    bool _udpDump(bool p_bMovePointer = false);
    bool _udpDump(unsigned p_uOffset,
                  unsigned p_uLength);
#endif

    /* READ/WRITE MDNS STRUCTS */
    bool _readMDNSMsgHeader(stcMDNS_MsgHeader& p_rMsgHeader);

    bool _write8(uint8_t p_u8Value,
                 stcMDNSSendParameter& p_rSendParameter);
    bool _write16(uint16_t p_u16Value,
                  stcMDNSSendParameter& p_rSendParameter);
    bool _write32(uint32_t p_u32Value,
                  stcMDNSSendParameter& p_rSendParameter);

    bool _writeMDNSMsgHeader(const stcMDNS_MsgHeader& p_MsgHeader,
                             stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSRRAttributes(const stcMDNS_RRAttributes& p_Attributes,
                                stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSRRDomain(const stcMDNS_RRDomain& p_Domain,
                            stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSHostDomain(const char* m_pcHostname,
                              bool p_bPrependRDLength,
                              uint16_t p_u16AdditionalLength,
                              stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSServiceDomain(const stcMDNSService& p_Service,
                                 bool p_bIncludeName,
                                 bool p_bPrependRDLength,
                                 uint16_t p_u16AdditionalLength,
                                 stcMDNSSendParameter& p_rSendParameter);

    bool _writeMDNSQuestion(stcMDNS_RRQuestion& p_Question,
                            stcMDNSSendParameter& p_rSendParameter);

#ifdef MDNS_IPV4_SUPPORT
    bool _writeMDNSAnswer_A(IPAddress p_IPAddress,
                            stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSAnswer_PTR_IPv4(IPAddress p_IPAddress,
                                   stcMDNSSendParameter& p_rSendParameter);
#endif
    bool _writeMDNSAnswer_PTR_TYPE(stcMDNSService& p_rService,
                                   stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSAnswer_PTR_NAME(stcMDNSService& p_rService,
                                   stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSAnswer_TXT(stcMDNSService& p_rService,
                              stcMDNSSendParameter& p_rSendParameter);
#ifdef MDNS_IPV6_SUPPORT
    bool _writeMDNSAnswer_AAAA(IPAddress p_IPAddress,
                               stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSAnswer_PTR_IPv6(IPAddress p_IPAddress,
                                   stcMDNSSendParameter& p_rSendParameter);
#endif
    bool _writeMDNSAnswer_SRV(stcMDNSService& p_rService,
                              stcMDNSSendParameter& p_rSendParameter);
    stcMDNS_NSECBitmap* _createNSECBitmap(uint32_t p_u32NSECContent);
    bool _writeMDNSNSECBitmap(const stcMDNS_NSECBitmap& p_NSECBitmap,
                              stcMDNSSendParameter& p_rSendParameter);
    bool _writeMDNSAnswer_NSEC(uint32_t p_u32NSECContent,
                               stcMDNSSendParameter& p_rSendParameter);
#ifdef MDNS_IPV4_SUPPORT
    bool _writeMDNSAnswer_NSEC_PTR_IPv4(IPAddress p_IPAddress,
                                        stcMDNSSendParameter& p_rSendParameter);
#endif
#ifdef MDNS_IPV6_SUPPORT
    bool _writeMDNSAnswer_NSEC_PTR_IPv6(IPAddress p_IPAddress,
                                        stcMDNSSendParameter& p_rSendParameter);
#endif
    bool _writeMDNSAnswer_NSEC(stcMDNSService& p_rService,
                               uint32_t p_u32NSECContent,
                               stcMDNSSendParameter& p_rSendParameter);

    /** HELPERS **/
    /* NETIF */
    bool _attachNetIf(netif* p_pNetIf);
    bool _detachNetIf(void);

    /* UDP CONTEXT */
    bool _callProcess(void);
    bool _allocUDPContext(void);
    bool _releaseUDPContext(void);

    /* QUERIES */
    stcMDNSQuery* _allocQuery(stcMDNSQuery::enuQueryType p_QueryType);
    bool _removeQuery(stcMDNSQuery* p_pQuery);
    bool _removeLegacyQuery(void);
    stcMDNSQuery* _findQuery(hMDNSQuery p_hQuery);
    stcMDNSQuery* _findLegacyQuery(void);
    bool _releaseQueries(void);
    stcMDNSQuery* _findNextQueryByDomain(const stcMDNS_RRDomain& p_Domain,
                                         stcMDNSQuery::enuQueryType p_QueryType,
                                         const stcMDNSQuery* p_pPrevQuery);

    /* HOSTNAME */
    bool _setHostname(const char* p_pcHostname);
    bool _releaseHostname(void);

    /* SERVICE */
    stcMDNSService* _allocService(const char* p_pcName,
                                  const char* p_pcService,
                                  const char* p_pcProtocol,
                                  uint16_t p_u16Port);
    bool _releaseService(stcMDNSService* p_pService);
    bool _releaseServices(void);

    stcMDNSService* _findService(const char* p_pcName,
                                 const char* p_pcService,
                                 const char* p_pcProtocol);
    stcMDNSService* _findService(const hMDNSService p_hService);
    const stcMDNSService* _findService(const hMDNSService p_hService) const;

    size_t _countServices(void) const;

    /* SERVICE TXT */
    stcMDNSServiceTxt* _allocServiceTxt(stcMDNSService* p_pService,
                                        const char* p_pcKey,
                                        const char* p_pcValue,
                                        bool p_bTemp);
    bool _releaseServiceTxt(stcMDNSService* p_pService,
                            stcMDNSServiceTxt* p_pTxt);
    stcMDNSServiceTxt* _updateServiceTxt(stcMDNSService* p_pService,
                                         stcMDNSServiceTxt* p_pTxt,
                                         const char* p_pcValue,
                                         bool p_bTemp);

    stcMDNSServiceTxt* _findServiceTxt(stcMDNSService* p_pService,
                                       const char* p_pcKey);
    stcMDNSServiceTxt* _findServiceTxt(stcMDNSService* p_pService,
                                       const hMDNSTxt p_hTxt);

    stcMDNSServiceTxt* _addServiceTxt(stcMDNSService* p_pService,
                                      const char* p_pcKey,
                                      const char* p_pcValue,
                                      bool p_bTemp);

    stcMDNSServiceTxt* _answerKeyValue(const hMDNSQuery p_hQuery,
                                       const uint32_t p_u32AnswerIndex);

    bool _collectServiceTxts(stcMDNSService& p_rService);
    bool _releaseTempServiceTxts(stcMDNSService& p_rService);
    const stcMDNSServiceTxt* _serviceTxts(const char* p_pcName,
                                          const char* p_pcService,
                                          const char* p_pcProtocol);

    /* MISC */
#if not defined ESP_8266_MDNS_INCLUDE || defined DEBUG_ESP_MDNS_RESPONDER
    bool _printRRDomain(const stcMDNS_RRDomain& p_rRRDomain) const;
    bool _printRRAnswer(const MDNSResponder::stcMDNS_RRAnswer& p_RRAnswer) const;
    const char* _RRType2Name(uint16_t p_u16RRType) const;
    const char* _RRClass2String(uint16_t p_u16RRClass,
                                bool p_bIsQuery) const;
    const char* _replyFlags2String(uint32_t p_u32ReplyFlags) const;
#endif
};

}	// namespace MDNSImplementation

}	// namespace esp8266

#endif // MDNS_H



