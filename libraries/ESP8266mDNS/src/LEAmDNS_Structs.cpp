/*
    LEAmDNS_Structs.cpp

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

#include "LEAmDNS_Priv.h"
#include "LEAmDNS_lwIPdefs.h"

namespace esp8266
{

/*
    LEAmDNS
*/
namespace MDNSImplementation
{

/**
    Internal CLASSES & STRUCTS
*/

/**
    MDNSResponder::stcMDNSAnswerAccessor

*/

/*
    MDNSResponder::stcMDNSAnswerAccessor::stcMDNSAnswerAccessor constructor
*/
MDNSResponder::stcMDNSAnswerAccessor::stcMDNSAnswerAccessor(MDNSResponder& p_rMDNSResponder,
        MDNSResponder::hMDNSQuery p_hQuery,
        uint32_t p_u32AnswerIndex)
    :   m_rMDNSResponder(p_rMDNSResponder),
        m_hQuery(p_hQuery),
        m_u32AnswerIndex(p_u32AnswerIndex)
{

    if ((txtsAvailable()) &&
            (0 == m_TxtKeyValueMap.size()))
    {

        for (const stcMDNSServiceTxt* pTxt = m_rMDNSResponder._answerKeyValue(m_hQuery, m_u32AnswerIndex); pTxt; pTxt = pTxt->m_pNext)
        {
            m_TxtKeyValueMap.emplace(std::pair<const char*, const char*>(pTxt->m_pcKey, pTxt->m_pcValue));
        }
    }
}

/**
    MDNSResponder::stcMDNSAnswerAccessor::stcCompareTxtKey
*/

/*
    MDNSResponder::stcMDNSAnswerAccessor::stcCompareTxtKey::operator()
*/
bool MDNSResponder::stcMDNSAnswerAccessor::stcCompareTxtKey::operator()(char const* p_pA,
        char const* p_pB) const
{

    return (0 > strcasecmp(p_pA, p_pB));
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::serviceDomainAvailable
*/
bool MDNSResponder::stcMDNSAnswerAccessor::serviceDomainAvailable(void) const
{

    return m_rMDNSResponder.hasAnswerServiceDomain(m_hQuery, m_u32AnswerIndex);
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::serviceDomain
*/
const char* MDNSResponder::stcMDNSAnswerAccessor::serviceDomain(void) const
{

    return m_rMDNSResponder.answerServiceDomain(m_hQuery, m_u32AnswerIndex);
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::hostDomainAvailable
*/
bool MDNSResponder::stcMDNSAnswerAccessor::hostDomainAvailable(void) const
{

    return m_rMDNSResponder.hasAnswerHostDomain(m_hQuery, m_u32AnswerIndex);
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::hostDomain
*/
const char* MDNSResponder::stcMDNSAnswerAccessor::hostDomain(void) const
{

    return (hostDomainAvailable()
            ? m_rMDNSResponder.answerHostDomain(m_hQuery, m_u32AnswerIndex)
            : nullptr);
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::hostPortAvailable
*/
bool MDNSResponder::stcMDNSAnswerAccessor::hostPortAvailable(void) const
{

    return m_rMDNSResponder.hasAnswerPort(m_hQuery, m_u32AnswerIndex);
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::hostPort
*/
uint16_t MDNSResponder::stcMDNSAnswerAccessor::hostPort(void) const
{

    return (hostPortAvailable()
            ? m_rMDNSResponder.answerPort(m_hQuery, m_u32AnswerIndex)
            : 0);
}

#ifdef MDNS_IPV4_SUPPORT
/*
    MDNSResponder::stcMDNSAnswerAccessor::IPv4AddressAvailable
*/
bool MDNSResponder::stcMDNSAnswerAccessor::IPv4AddressAvailable(void) const
{

    return m_rMDNSResponder.hasAnswerIPv4Address(m_hQuery, m_u32AnswerIndex);
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::IPv4Addresses
*/
std::vector<IPAddress> MDNSResponder::stcMDNSAnswerAccessor::IPv4Addresses(void) const
{

    std::vector<IPAddress>  internalIP;
    if (IPv4AddressAvailable())
    {
        uint16_t    cntIPv4Address = m_rMDNSResponder.answerIPv4AddressCount(m_hQuery, m_u32AnswerIndex);
        for (uint32_t u = 0; u < cntIPv4Address; ++u)
        {
            internalIP.emplace_back(m_rMDNSResponder.answerIPv4Address(m_hQuery, m_u32AnswerIndex, u));
        }
    }
    return internalIP;
}
#endif

#ifdef MDNS_IPV6_SUPPORT
/*
    MDNSResponder::stcMDNSAnswerAccessor::IPv6AddressAvailable
*/
bool MDNSResponder::stcMDNSAnswerAccessor::IPv6AddressAvailable(void) const
{

    return (m_rMDNSResponder.hasAnswerIPv6Address(m_hQuery, m_u32AnswerIndex));
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::IPv6Addresses
*/
std::vector<IPAddress> MDNSResponder::stcMDNSAnswerAccessor::IPv6Addresses(void) const
{

    std::vector<IPAddress>  internalIP;
    if (IPv6AddressAvailable())
    {
        uint16_t    cntIPv6Address = m_rMDNSResponder.answerIPv6AddressCount(m_hQuery, m_u32AnswerIndex);
        for (uint32_t u = 0; u < cntIPv6Address; ++u)
        {
            internalIP.emplace_back(m_rMDNSResponder.answerIPv6Address(m_hQuery, m_u32AnswerIndex, u));
        }
    }
    return internalIP;
}
#endif

/*
    MDNSResponder::stcMDNSAnswerAccessor::txtsAvailable
*/
bool MDNSResponder::stcMDNSAnswerAccessor::txtsAvailable(void) const
{

    return m_rMDNSResponder.hasAnswerTxts(m_hQuery, m_u32AnswerIndex);
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::txts
*/
const char* MDNSResponder::stcMDNSAnswerAccessor::txts(void) const
{

    return (txtsAvailable()
            ? m_rMDNSResponder.answerTxts(m_hQuery, m_u32AnswerIndex)
            : nullptr);
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::txtKeyValues
*/
const MDNSResponder::stcMDNSAnswerAccessor::clsTxtKeyValueMap& MDNSResponder::stcMDNSAnswerAccessor::txtKeyValues(void) const
{

    /*  if ((txtsAvailable()) &&
        (0 == m_TxtKeyValueMap.size())) {

        for (const stcMDNSServiceTxt* pTxt=m_rMDNSResponder._answerKeyValue(m_hQuery, m_u32AnswerIndex); pTxt; pTxt=pTxt->m_pNext) {
            m_TxtKeyValueMap.emplace(std::pair<const char*, const char*>(pTxt->m_pcKey, pTxt->m_pcValue));
        }
        }*/
    return m_TxtKeyValueMap;
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::txtValue
*/
const char* MDNSResponder::stcMDNSAnswerAccessor::txtValue(const char* p_pcKey) const
{

    char*   pcResult = 0;

    for (const stcMDNSServiceTxt* pTxt = m_rMDNSResponder._answerKeyValue(m_hQuery, m_u32AnswerIndex); pTxt; pTxt = pTxt->m_pNext)
    {
        if ((p_pcKey) &&
                (0 == strcasecmp(pTxt->m_pcKey, p_pcKey)))
        {

            pcResult = pTxt->m_pcValue;
            break;
        }
    }
    return pcResult;
}

/*
    MDNSResponder::stcMDNSAnswerAccessor::printTo
 **/
size_t MDNSResponder::stcMDNSAnswerAccessor::printTo(Print& p_Print) const
{

    const char* cpcI = " * ";
    const char* cpcS = "  ";

    p_Print.println(" * * * * *");
    if (hostDomainAvailable())
    {
        p_Print.print(cpcI);
        p_Print.print("Host domain: ");
        p_Print.println(hostDomain());
    }
#ifdef MDNS_IPV4_SUPPORT
    if (IPv4AddressAvailable())
    {
        p_Print.print(cpcI);
        p_Print.println("IPv4 address(es):");
        for (const IPAddress& addr : IPv4Addresses())
        {
            p_Print.print(cpcI);
            p_Print.print(cpcS);
            p_Print.println(addr);
        }
    }
#endif
#ifdef MDNS_IPV6_SUPPORT
    if (IPv6AddressAvailable())
    {
        p_Print.print(cpcI);
        p_Print.println("IPv6 address(es):");
        for (const IPAddress& addr : IPv6Addresses())
        {
            p_Print.print(cpcI);
            p_Print.print(cpcS);
            p_Print.println(addr);
        }
    }
#endif
    if (serviceDomainAvailable())
    {
        p_Print.print(cpcI);
        p_Print.print("Service domain: ");
        p_Print.println(serviceDomain());
    }
    if (hostPortAvailable())
    {
        p_Print.print(cpcI);
        p_Print.print("Host port: ");
        p_Print.println(hostPort());
    }
    if (txtsAvailable())
    {
        p_Print.print(cpcI);
        p_Print.print("TXTs:");
        for (auto const& x : txtKeyValues())
        {
            p_Print.print(cpcI);
            p_Print.print(cpcS);
            p_Print.print(x.first);
            p_Print.print("=");
            p_Print.println(x.second);
        }
    }
    p_Print.println(" * * * * *");
}


/**
    MDNSResponder::stcMDNSServiceTxt

    One MDNS TXT item.
    m_pcValue may be '\0'.
    Objects can be chained together (list, m_pNext).
    A 'm_bTemp' flag differentiates between static and dynamic items.
    Output as byte array 'c#=1' is supported.
*/

/*
    MDNSResponder::stcMDNSServiceTxt::stcMDNSServiceTxt constructor
*/
MDNSResponder::stcMDNSServiceTxt::stcMDNSServiceTxt(const char* p_pcKey /*= 0*/,
        const char* p_pcValue /*= 0*/,
        bool p_bTemp /*= false*/)
    :   m_pNext(0),
        m_pcKey(0),
        m_pcValue(0),
        m_bTemp(p_bTemp)
{

    setKey(p_pcKey);
    setValue(p_pcValue);
}

/*
    MDNSResponder::stcMDNSServiceTxt::stcMDNSServiceTxt copy-constructor
*/
MDNSResponder::stcMDNSServiceTxt::stcMDNSServiceTxt(const MDNSResponder::stcMDNSServiceTxt& p_Other)
    :   m_pNext(0),
        m_pcKey(0),
        m_pcValue(0),
        m_bTemp(false)
{

    operator=(p_Other);
}

/*
    MDNSResponder::stcMDNSServiceTxt::~stcMDNSServiceTxt destructor
*/
MDNSResponder::stcMDNSServiceTxt::~stcMDNSServiceTxt(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNSServiceTxt::operator=
*/
MDNSResponder::stcMDNSServiceTxt& MDNSResponder::stcMDNSServiceTxt::operator=(const MDNSResponder::stcMDNSServiceTxt& p_Other)
{

    if (&p_Other != this)
    {
        clear();
        set(p_Other.m_pcKey, p_Other.m_pcValue, p_Other.m_bTemp);
    }
    return *this;
}

/*
    MDNSResponder::stcMDNSServiceTxt::clear
*/
bool MDNSResponder::stcMDNSServiceTxt::clear(void)
{

    releaseKey();
    releaseValue();
    return true;
}

/*
    MDNSResponder::stcMDNSServiceTxt::allocKey
*/
char* MDNSResponder::stcMDNSServiceTxt::allocKey(size_t p_stLength)
{

    releaseKey();
    if (p_stLength)
    {
        m_pcKey = new char[p_stLength + 1];
    }
    return m_pcKey;
}

/*
    MDNSResponder::stcMDNSServiceTxt::setKey
*/
bool MDNSResponder::stcMDNSServiceTxt::setKey(const char* p_pcKey,
        size_t p_stLength)
{

    bool bResult = false;

    releaseKey();
    if (p_stLength)
    {
        if (allocKey(p_stLength))
        {
            strncpy(m_pcKey, p_pcKey, p_stLength);
            m_pcKey[p_stLength] = 0;
            bResult = true;
        }
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSServiceTxt::setKey
*/
bool MDNSResponder::stcMDNSServiceTxt::setKey(const char* p_pcKey)
{

    return setKey(p_pcKey, (p_pcKey ? strlen(p_pcKey) : 0));
}

/*
    MDNSResponder::stcMDNSServiceTxt::releaseKey
*/
bool MDNSResponder::stcMDNSServiceTxt::releaseKey(void)
{

    if (m_pcKey)
    {
        delete[] m_pcKey;
        m_pcKey = 0;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSServiceTxt::allocValue
*/
char* MDNSResponder::stcMDNSServiceTxt::allocValue(size_t p_stLength)
{

    releaseValue();
    if (p_stLength)
    {
        m_pcValue = new char[p_stLength + 1];
    }
    return m_pcValue;
}

/*
    MDNSResponder::stcMDNSServiceTxt::setValue
*/
bool MDNSResponder::stcMDNSServiceTxt::setValue(const char* p_pcValue,
        size_t p_stLength)
{

    bool bResult = false;

    releaseValue();
    if (p_stLength)
    {
        if (allocValue(p_stLength))
        {
            strncpy(m_pcValue, p_pcValue, p_stLength);
            m_pcValue[p_stLength] = 0;
            bResult = true;
        }
    }
    else    // No value -> also OK
    {
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSServiceTxt::setValue
*/
bool MDNSResponder::stcMDNSServiceTxt::setValue(const char* p_pcValue)
{

    return setValue(p_pcValue, (p_pcValue ? strlen(p_pcValue) : 0));
}

/*
    MDNSResponder::stcMDNSServiceTxt::releaseValue
*/
bool MDNSResponder::stcMDNSServiceTxt::releaseValue(void)
{

    if (m_pcValue)
    {
        delete[] m_pcValue;
        m_pcValue = 0;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSServiceTxt::set
*/
bool MDNSResponder::stcMDNSServiceTxt::set(const char* p_pcKey,
        const char* p_pcValue,
        bool p_bTemp /*= false*/)
{

    m_bTemp = p_bTemp;
    return ((setKey(p_pcKey)) &&
            (setValue(p_pcValue)));
}

/*
    MDNSResponder::stcMDNSServiceTxt::update
*/
bool MDNSResponder::stcMDNSServiceTxt::update(const char* p_pcValue)
{

    return setValue(p_pcValue);
}

/*
    MDNSResponder::stcMDNSServiceTxt::length

    length of eg. 'c#=1' without any closing '\0'
*/
size_t MDNSResponder::stcMDNSServiceTxt::length(void) const
{

    size_t  stLength = 0;
    if (m_pcKey)
    {
        stLength += strlen(m_pcKey);                     // Key
        stLength += 1;                                      // '='
        stLength += (m_pcValue ? strlen(m_pcValue) : 0); // Value
    }
    return stLength;
}


/**
    MDNSResponder::stcMDNSServiceTxts

    A list of zero or more MDNS TXT items.
    Dynamic TXT items can be removed by 'removeTempTxts'.
    A TXT item can be looke up by its 'key' member.
    Export as ';'-separated byte array is supported.
    Export as 'length byte coded' byte array is supported.
    Comparision ((all A TXT items in B and equal) AND (all B TXT items in A and equal)) is supported.

*/

/*
    MDNSResponder::stcMDNSServiceTxts::stcMDNSServiceTxts contructor
*/
MDNSResponder::stcMDNSServiceTxts::stcMDNSServiceTxts(void)
    :   m_pTxts(0)
{

}

/*
    MDNSResponder::stcMDNSServiceTxts::stcMDNSServiceTxts copy-constructor
*/
MDNSResponder::stcMDNSServiceTxts::stcMDNSServiceTxts(const stcMDNSServiceTxts& p_Other)
    :   m_pTxts(0)
{

    operator=(p_Other);
}

/*
    MDNSResponder::stcMDNSServiceTxts::~stcMDNSServiceTxts destructor
*/
MDNSResponder::stcMDNSServiceTxts::~stcMDNSServiceTxts(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNSServiceTxts::operator=
*/
MDNSResponder::stcMDNSServiceTxts& MDNSResponder::stcMDNSServiceTxts::operator=(const stcMDNSServiceTxts& p_Other)
{

    if (this != &p_Other)
    {
        clear();

        for (stcMDNSServiceTxt* pOtherTxt = p_Other.m_pTxts; pOtherTxt; pOtherTxt = pOtherTxt->m_pNext)
        {
            add(new stcMDNSServiceTxt(*pOtherTxt));
        }
    }
    return *this;
}

/*
    MDNSResponder::stcMDNSServiceTxts::clear
*/
bool MDNSResponder::stcMDNSServiceTxts::clear(void)
{

    while (m_pTxts)
    {
        stcMDNSServiceTxt* pNext = m_pTxts->m_pNext;
        delete m_pTxts;
        m_pTxts = pNext;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSServiceTxts::add
*/
bool MDNSResponder::stcMDNSServiceTxts::add(MDNSResponder::stcMDNSServiceTxt* p_pTxt)
{

    bool bResult = false;

    if (p_pTxt)
    {
        p_pTxt->m_pNext = m_pTxts;
        m_pTxts = p_pTxt;
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::remove
*/
bool MDNSResponder::stcMDNSServiceTxts::remove(stcMDNSServiceTxt* p_pTxt)
{

    bool    bResult = false;

    if (p_pTxt)
    {
        stcMDNSServiceTxt*  pPred = m_pTxts;
        while ((pPred) &&
                (pPred->m_pNext != p_pTxt))
        {
            pPred = pPred->m_pNext;
        }
        if (pPred)
        {
            pPred->m_pNext = p_pTxt->m_pNext;
            delete p_pTxt;
            bResult = true;
        }
        else if (m_pTxts == p_pTxt)     // No predecesor, but first item
        {
            m_pTxts = p_pTxt->m_pNext;
            delete p_pTxt;
            bResult = true;
        }
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::removeTempTxts
*/
bool MDNSResponder::stcMDNSServiceTxts::removeTempTxts(void)
{

    bool    bResult = true;

    stcMDNSServiceTxt*  pTxt = m_pTxts;
    while ((bResult) &&
            (pTxt))
    {
        stcMDNSServiceTxt*  pNext = pTxt->m_pNext;
        if (pTxt->m_bTemp)
        {
            bResult = remove(pTxt);
        }
        pTxt = pNext;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::find
*/
MDNSResponder::stcMDNSServiceTxt* MDNSResponder::stcMDNSServiceTxts::find(const char* p_pcKey)
{

    stcMDNSServiceTxt* pResult = 0;

    for (stcMDNSServiceTxt* pTxt = m_pTxts; pTxt; pTxt = pTxt->m_pNext)
    {
        if ((p_pcKey) &&
                (0 == strcmp(pTxt->m_pcKey, p_pcKey)))
        {
            pResult = pTxt;
            break;
        }
    }
    return pResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::find
*/
const MDNSResponder::stcMDNSServiceTxt* MDNSResponder::stcMDNSServiceTxts::find(const char* p_pcKey) const
{

    const stcMDNSServiceTxt*   pResult = 0;

    for (const stcMDNSServiceTxt* pTxt = m_pTxts; pTxt; pTxt = pTxt->m_pNext)
    {
        if ((p_pcKey) &&
                (0 == strcmp(pTxt->m_pcKey, p_pcKey)))
        {

            pResult = pTxt;
            break;
        }
    }
    return pResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::find
*/
MDNSResponder::stcMDNSServiceTxt* MDNSResponder::stcMDNSServiceTxts::find(const stcMDNSServiceTxt* p_pTxt)
{

    stcMDNSServiceTxt* pResult = 0;

    for (stcMDNSServiceTxt* pTxt = m_pTxts; pTxt; pTxt = pTxt->m_pNext)
    {
        if (p_pTxt == pTxt)
        {
            pResult = pTxt;
            break;
        }
    }
    return pResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::length
*/
uint16_t MDNSResponder::stcMDNSServiceTxts::length(void) const
{

    uint16_t    u16Length = 0;

    stcMDNSServiceTxt*  pTxt = m_pTxts;
    while (pTxt)
    {
        u16Length += 1;                 // Length byte
        u16Length += pTxt->length();    // Text
        pTxt = pTxt->m_pNext;
    }
    return u16Length;
}

/*
    MDNSResponder::stcMDNSServiceTxts::c_strLength

    (incl. closing '\0'). Length bytes place is used for delimiting ';' and closing '\0'
*/
size_t MDNSResponder::stcMDNSServiceTxts::c_strLength(void) const
{

    return length();
}

/*
    MDNSResponder::stcMDNSServiceTxts::c_str
*/
bool MDNSResponder::stcMDNSServiceTxts::c_str(char* p_pcBuffer)
{

    bool bResult = false;

    if (p_pcBuffer)
    {
        bResult = true;

        *p_pcBuffer = 0;
        for (stcMDNSServiceTxt* pTxt = m_pTxts; ((bResult) && (pTxt)); pTxt = pTxt->m_pNext)
        {
            size_t  stLength;
            if ((bResult = (0 != (stLength = (pTxt->m_pcKey ? strlen(pTxt->m_pcKey) : 0)))))
            {
                if (pTxt != m_pTxts)
                {
                    *p_pcBuffer++ = ';';
                }
                strncpy(p_pcBuffer, pTxt->m_pcKey, stLength); p_pcBuffer[stLength] = 0;
                p_pcBuffer += stLength;
                *p_pcBuffer++ = '=';
                if ((stLength = (pTxt->m_pcValue ? strlen(pTxt->m_pcValue) : 0)))
                {
                    strncpy(p_pcBuffer, pTxt->m_pcValue, stLength); p_pcBuffer[stLength] = 0;
                    p_pcBuffer += stLength;
                }
            }
        }
        *p_pcBuffer++ = 0;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::bufferLength

    (incl. closing '\0').
*/
size_t MDNSResponder::stcMDNSServiceTxts::bufferLength(void) const
{

    return (length() + 1);
}

/*
    MDNSResponder::stcMDNSServiceTxts::toBuffer
*/
bool MDNSResponder::stcMDNSServiceTxts::buffer(char* p_pcBuffer)
{

    bool bResult = false;

    if (p_pcBuffer)
    {
        bResult = true;

        *p_pcBuffer = 0;
        for (stcMDNSServiceTxt* pTxt = m_pTxts; ((bResult) && (pTxt)); pTxt = pTxt->m_pNext)
        {
            *(unsigned char*)p_pcBuffer++ = pTxt->length();
            size_t  stLength;
            if ((bResult = (0 != (stLength = (pTxt->m_pcKey ? strlen(pTxt->m_pcKey) : 0)))))
            {
                memcpy(p_pcBuffer, pTxt->m_pcKey, stLength);
                p_pcBuffer += stLength;
                *p_pcBuffer++ = '=';
                if ((stLength = (pTxt->m_pcValue ? strlen(pTxt->m_pcValue) : 0)))
                {
                    memcpy(p_pcBuffer, pTxt->m_pcValue, stLength);
                    p_pcBuffer += stLength;
                }
            }
        }
        *p_pcBuffer++ = 0;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::compare
*/
bool MDNSResponder::stcMDNSServiceTxts::compare(const MDNSResponder::stcMDNSServiceTxts& p_Other) const
{

    bool    bResult = false;

    if ((bResult = (length() == p_Other.length())))
    {
        // Compare A->B
        for (const stcMDNSServiceTxt* pTxt = m_pTxts; ((bResult) && (pTxt)); pTxt = pTxt->m_pNext)
        {
            const stcMDNSServiceTxt*    pOtherTxt = p_Other.find(pTxt->m_pcKey);
            bResult = ((pOtherTxt) &&
                       (pTxt->m_pcValue) &&
                       (pOtherTxt->m_pcValue) &&
                       (strlen(pTxt->m_pcValue) == strlen(pOtherTxt->m_pcValue)) &&
                       (0 == strcmp(pTxt->m_pcValue, pOtherTxt->m_pcValue)));
        }
        // Compare B->A
        for (const stcMDNSServiceTxt* pOtherTxt = p_Other.m_pTxts; ((bResult) && (pOtherTxt)); pOtherTxt = pOtherTxt->m_pNext)
        {
            const stcMDNSServiceTxt*    pTxt = find(pOtherTxt->m_pcKey);
            bResult = ((pTxt) &&
                       (pOtherTxt->m_pcValue) &&
                       (pTxt->m_pcValue) &&
                       (strlen(pOtherTxt->m_pcValue) == strlen(pTxt->m_pcValue)) &&
                       (0 == strcmp(pOtherTxt->m_pcValue, pTxt->m_pcValue)));
        }
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSServiceTxts::operator==
*/
bool MDNSResponder::stcMDNSServiceTxts::operator==(const stcMDNSServiceTxts& p_Other) const
{

    return compare(p_Other);
}

/*
    MDNSResponder::stcMDNSServiceTxts::operator!=
*/
bool MDNSResponder::stcMDNSServiceTxts::operator!=(const stcMDNSServiceTxts& p_Other) const
{

    return !compare(p_Other);
}


/**
    MDNSResponder::stcMDNS_MsgHeader

    A MDNS message haeder.

*/

/*
    MDNSResponder::stcMDNS_MsgHeader::stcMDNS_MsgHeader
*/
MDNSResponder::stcMDNS_MsgHeader::stcMDNS_MsgHeader(uint16_t p_u16ID /*= 0*/,
        bool p_bQR /*= false*/,
        uint8_t p_u8Opcode /*= 0*/,
        bool p_bAA /*= false*/,
        bool p_bTC /*= false*/,
        bool p_bRD /*= false*/,
        bool p_bRA /*= false*/,
        uint8_t p_u8RCode /*= 0*/,
        uint16_t p_u16QDCount /*= 0*/,
        uint16_t p_u16ANCount /*= 0*/,
        uint16_t p_u16NSCount /*= 0*/,
        uint16_t p_u16ARCount /*= 0*/)
    :   m_u16ID(p_u16ID),
        m_1bQR(p_bQR), m_4bOpcode(p_u8Opcode), m_1bAA(p_bAA), m_1bTC(p_bTC), m_1bRD(p_bRD),
        m_1bRA(p_bRA), m_3bZ(0), m_4bRCode(p_u8RCode),
        m_u16QDCount(p_u16QDCount),
        m_u16ANCount(p_u16ANCount),
        m_u16NSCount(p_u16NSCount),
        m_u16ARCount(p_u16ARCount)
{

}


/**
    MDNSResponder::stcMDNS_RRDomain

    A MDNS domain object.
    The labels of the domain are stored (DNS-like encoded) in 'm_acName':
    [length byte]varlength label[length byte]varlength label[0]
    'm_u16NameLength' stores the used length of 'm_acName'.
    Dynamic label addition is supported.
    Comparison is supported.
    Export as byte array 'esp8266.local' is supported.

*/

/*
    MDNSResponder::stcMDNS_RRDomain::stcMDNS_RRDomain constructor
*/
MDNSResponder::stcMDNS_RRDomain::stcMDNS_RRDomain(void)
    :   m_u16NameLength(0)
{

    clear();
}

/*
    MDNSResponder::stcMDNS_RRDomain::stcMDNS_RRDomain copy-constructor
*/
MDNSResponder::stcMDNS_RRDomain::stcMDNS_RRDomain(const stcMDNS_RRDomain& p_Other)
    :   m_u16NameLength(0)
{

    operator=(p_Other);
}

/*
    MDNSResponder::stcMDNS_RRDomain::operator =
*/
MDNSResponder::stcMDNS_RRDomain& MDNSResponder::stcMDNS_RRDomain::operator=(const stcMDNS_RRDomain& p_Other)
{

    if (&p_Other != this)
    {
        memcpy(m_acName, p_Other.m_acName, sizeof(m_acName));
        m_u16NameLength = p_Other.m_u16NameLength;
    }
    return *this;
}

/*
    MDNSResponder::stcMDNS_RRDomain::clear
*/
bool MDNSResponder::stcMDNS_RRDomain::clear(void)
{

    memset(m_acName, 0, sizeof(m_acName));
    m_u16NameLength = 0;
    return true;
}

/*
    MDNSResponder::stcMDNS_RRDomain::addLabel
*/
bool MDNSResponder::stcMDNS_RRDomain::addLabel(const char* p_pcLabel,
        bool p_bPrependUnderline /*= false*/)
{

    bool    bResult = false;

    size_t  stLength = (p_pcLabel
                        ? (strlen(p_pcLabel) + (p_bPrependUnderline ? 1 : 0))
                        : 0);
    if ((MDNS_DOMAIN_LABEL_MAXLENGTH >= stLength) &&
            (MDNS_DOMAIN_MAXLENGTH >= (m_u16NameLength + (1 + stLength))))
    {
        // Length byte
        m_acName[m_u16NameLength] = (unsigned char)stLength;    // Might be 0!
        ++m_u16NameLength;
        // Label
        if (stLength)
        {
            if (p_bPrependUnderline)
            {
                m_acName[m_u16NameLength++] = '_';
                --stLength;
            }
            strncpy(&(m_acName[m_u16NameLength]), p_pcLabel, stLength); m_acName[m_u16NameLength + stLength] = 0;
            m_u16NameLength += stLength;
        }
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNS_RRDomain::compare
*/
bool MDNSResponder::stcMDNS_RRDomain::compare(const stcMDNS_RRDomain& p_Other) const
{

    bool    bResult = false;

    if (m_u16NameLength == p_Other.m_u16NameLength)
    {
        const char* pT = m_acName;
        const char* pO = p_Other.m_acName;
        while ((pT) &&
                (pO) &&
                (*((unsigned char*)pT) == *((unsigned char*)pO)) &&                  // Same length AND
                (0 == strncasecmp((pT + 1), (pO + 1), *((unsigned char*)pT))))     // Same content
        {
            if (*((unsigned char*)pT))              // Not 0
            {
                pT += (1 + * ((unsigned char*)pT)); // Shift by length byte and lenght
                pO += (1 + * ((unsigned char*)pO));
            }
            else                                    // Is 0 -> Successfully reached the end
            {
                bResult = true;
                break;
            }
        }
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNS_RRDomain::operator ==
*/
bool MDNSResponder::stcMDNS_RRDomain::operator==(const stcMDNS_RRDomain& p_Other) const
{

    return compare(p_Other);
}

/*
    MDNSResponder::stcMDNS_RRDomain::operator !=
*/
bool MDNSResponder::stcMDNS_RRDomain::operator!=(const stcMDNS_RRDomain& p_Other) const
{

    return !compare(p_Other);
}

/*
    MDNSResponder::stcMDNS_RRDomain::operator >
*/
bool MDNSResponder::stcMDNS_RRDomain::operator>(const stcMDNS_RRDomain& p_Other) const
{

    // TODO: Check, if this is a good idea...
    return !compare(p_Other);
}

/*
    MDNSResponder::stcMDNS_RRDomain::c_strLength
*/
size_t MDNSResponder::stcMDNS_RRDomain::c_strLength(void) const
{

    size_t          stLength = 0;

    unsigned char*  pucLabelLength = (unsigned char*)m_acName;
    while (*pucLabelLength)
    {
        stLength += (*pucLabelLength + 1 /* +1 for '.' or '\0'*/);
        pucLabelLength += (*pucLabelLength + 1);
    }
    return stLength;
}

/*
    MDNSResponder::stcMDNS_RRDomain::c_str
*/
bool MDNSResponder::stcMDNS_RRDomain::c_str(char* p_pcBuffer)
{

    bool bResult = false;

    if (p_pcBuffer)
    {
        *p_pcBuffer = 0;
        unsigned char* pucLabelLength = (unsigned char*)m_acName;
        while (*pucLabelLength)
        {
            memcpy(p_pcBuffer, (const char*)(pucLabelLength + 1), *pucLabelLength);
            p_pcBuffer += *pucLabelLength;
            pucLabelLength += (*pucLabelLength + 1);
            *p_pcBuffer++ = (*pucLabelLength ? '.' : '\0');
        }
        bResult = true;
    }
    return bResult;
}


/**
    MDNSResponder::stcMDNS_RRAttributes

    A MDNS attributes object.

*/

/*
    MDNSResponder::stcMDNS_RRAttributes::stcMDNS_RRAttributes constructor
*/
MDNSResponder::stcMDNS_RRAttributes::stcMDNS_RRAttributes(uint16_t p_u16Type /*= 0*/,
        uint16_t p_u16Class /*= 1 DNS_RRCLASS_IN Internet*/)
    :   m_u16Type(p_u16Type),
        m_u16Class(p_u16Class)
{

}

/*
    MDNSResponder::stcMDNS_RRAttributes::stcMDNS_RRAttributes copy-constructor
*/
MDNSResponder::stcMDNS_RRAttributes::stcMDNS_RRAttributes(const MDNSResponder::stcMDNS_RRAttributes& p_Other)
{

    operator=(p_Other);
}

/*
    MDNSResponder::stcMDNS_RRAttributes::operator =
*/
MDNSResponder::stcMDNS_RRAttributes& MDNSResponder::stcMDNS_RRAttributes::operator=(const MDNSResponder::stcMDNS_RRAttributes& p_Other)
{

    if (&p_Other != this)
    {
        m_u16Type = p_Other.m_u16Type;
        m_u16Class = p_Other.m_u16Class;
    }
    return *this;
}


/**
    MDNSResponder::stcMDNS_RRHeader

    A MDNS record header (domain and attributes) object.

*/

/*
    MDNSResponder::stcMDNS_RRHeader::stcMDNS_RRHeader constructor
*/
MDNSResponder::stcMDNS_RRHeader::stcMDNS_RRHeader(void)
{

}

/*
    MDNSResponder::stcMDNS_RRHeader::stcMDNS_RRHeader copy-constructor
*/
MDNSResponder::stcMDNS_RRHeader::stcMDNS_RRHeader(const stcMDNS_RRHeader& p_Other)
{

    operator=(p_Other);
}

/*
    MDNSResponder::stcMDNS_RRHeader::operator =
*/
MDNSResponder::stcMDNS_RRHeader& MDNSResponder::stcMDNS_RRHeader::operator=(const MDNSResponder::stcMDNS_RRHeader& p_Other)
{

    if (&p_Other != this)
    {
        m_Domain = p_Other.m_Domain;
        m_Attributes = p_Other.m_Attributes;
    }
    return *this;
}

/*
    MDNSResponder::stcMDNS_RRHeader::clear
*/
bool MDNSResponder::stcMDNS_RRHeader::clear(void)
{

    m_Domain.clear();
    return true;
}


/**
    MDNSResponder::stcMDNS_RRQuestion

    A MDNS question record object (header + question flags)

*/

/*
    MDNSResponder::stcMDNS_RRQuestion::stcMDNS_RRQuestion constructor
*/
MDNSResponder::stcMDNS_RRQuestion::stcMDNS_RRQuestion(void)
    :   m_pNext(0),
        m_bUnicast(false)
{

}


/**
    MDNSResponder::stcMDNS_NSECBitmap

    A MDNS question record object (header + question flags)

*/

/*
    MDNSResponder::stcMDNS_NSECBitmap::stcMDNS_NSECBitmap constructor
*/
MDNSResponder::stcMDNS_NSECBitmap::stcMDNS_NSECBitmap(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNS_NSECBitmap::stcMDNS_NSECBitmap destructor
*/
bool MDNSResponder::stcMDNS_NSECBitmap::clear(void)
{

    memset(m_au8BitmapData, 0, sizeof(m_au8BitmapData));
    return true;
}

/*
    MDNSResponder::stcMDNS_NSECBitmap::length
*/
uint16_t MDNSResponder::stcMDNS_NSECBitmap::length(void) const
{

    return sizeof(m_au8BitmapData); // 6
}

/*
    MDNSResponder::stcMDNS_NSECBitmap::setBit
*/
bool MDNSResponder::stcMDNS_NSECBitmap::setBit(uint16_t p_u16Bit)
{

    bool    bResult = false;

    if ((p_u16Bit) &&
            (length() > (p_u16Bit / 8)))                    // bit between 0..47(2F)
    {

        uint8_t&    ru8Byte = m_au8BitmapData[p_u16Bit / 8];
        uint8_t     u8Flag = 1 << (7 - (p_u16Bit % 8)); // (7 - (0..7)) = 7..0

        ru8Byte |= u8Flag;

        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNS_NSECBitmap::getBit
*/
bool MDNSResponder::stcMDNS_NSECBitmap::getBit(uint16_t p_u16Bit) const
{

    bool    bResult = false;

    if ((p_u16Bit) &&
            (length() > (p_u16Bit / 8)))                    // bit between 0..47(2F)
    {

        uint8_t u8Byte = m_au8BitmapData[p_u16Bit / 8];
        uint8_t u8Flag = 1 << (7 - (p_u16Bit % 8));     // (7 - (0..7)) = 7..0

        bResult = (u8Byte & u8Flag);
    }
    return bResult;
}


/**
    MDNSResponder::stcMDNS_RRAnswer

    A MDNS answer record object (header + answer content).
    This is a 'virtual' base class for all other MDNS answer classes.

*/

/*
    MDNSResponder::stcMDNS_RRAnswer::stcMDNS_RRAnswer constructor
*/
MDNSResponder::stcMDNS_RRAnswer::stcMDNS_RRAnswer(enuAnswerType p_AnswerType,
        const MDNSResponder::stcMDNS_RRHeader& p_Header,
        uint32_t p_u32TTL)
    :   m_pNext(0),
        m_AnswerType(p_AnswerType),
        m_Header(p_Header),
        m_u32TTL(p_u32TTL)
{

    // Extract 'cache flush'-bit
    m_bCacheFlush = (m_Header.m_Attributes.m_u16Class & 0x8000);
    m_Header.m_Attributes.m_u16Class &= (~0x8000);
}

/*
    MDNSResponder::stcMDNS_RRAnswer::~stcMDNS_RRAnswer destructor
*/
MDNSResponder::stcMDNS_RRAnswer::~stcMDNS_RRAnswer(void)
{

}

/*
    MDNSResponder::stcMDNS_RRAnswer::answerType
*/
MDNSResponder::enuAnswerType MDNSResponder::stcMDNS_RRAnswer::answerType(void) const
{

    return m_AnswerType;
}

/*
    MDNSResponder::stcMDNS_RRAnswer::clear
*/
bool MDNSResponder::stcMDNS_RRAnswer::clear(void)
{

    m_pNext = 0;
    m_Header.clear();
    return true;
}


/**
    MDNSResponder::stcMDNS_RRAnswerA

    A MDNS A answer object.
    Extends the base class by an IPv4 address member.

*/

#ifdef MDNS_IPV4_SUPPORT
/*
    MDNSResponder::stcMDNS_RRAnswerA::stcMDNS_RRAnswerA constructor
*/
MDNSResponder::stcMDNS_RRAnswerA::stcMDNS_RRAnswerA(const MDNSResponder::stcMDNS_RRHeader& p_Header,
        uint32_t p_u32TTL)
    :   stcMDNS_RRAnswer(enuAnswerType::A, p_Header, p_u32TTL),
        m_IPAddress()
{

}

/*
    MDNSResponder::stcMDNS_RRAnswerA::stcMDNS_RRAnswerA destructor
*/
MDNSResponder::stcMDNS_RRAnswerA::~stcMDNS_RRAnswerA(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNS_RRAnswerA::clear
*/
bool MDNSResponder::stcMDNS_RRAnswerA::clear(void)
{

    m_IPAddress = IPAddress();
    return true;
}
#endif


/**
    MDNSResponder::stcMDNS_RRAnswerPTR

    A MDNS PTR answer object.
    Extends the base class by a MDNS domain member.

*/

/*
    MDNSResponder::stcMDNS_RRAnswerPTR::stcMDNS_RRAnswerPTR constructor
*/
MDNSResponder::stcMDNS_RRAnswerPTR::stcMDNS_RRAnswerPTR(const MDNSResponder::stcMDNS_RRHeader& p_Header,
        uint32_t p_u32TTL)
    :   stcMDNS_RRAnswer(enuAnswerType::PTR, p_Header, p_u32TTL)
{

}

/*
    MDNSResponder::stcMDNS_RRAnswerPTR::~stcMDNS_RRAnswerPTR destructor
*/
MDNSResponder::stcMDNS_RRAnswerPTR::~stcMDNS_RRAnswerPTR(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNS_RRAnswerPTR::clear
*/
bool MDNSResponder::stcMDNS_RRAnswerPTR::clear(void)
{

    m_PTRDomain.clear();
    return true;
}


/**
    MDNSResponder::stcMDNS_RRAnswerTXT

    A MDNS TXT answer object.
    Extends the base class by a MDNS TXT items list member.

*/

/*
    MDNSResponder::stcMDNS_RRAnswerTXT::stcMDNS_RRAnswerTXT constructor
*/
MDNSResponder::stcMDNS_RRAnswerTXT::stcMDNS_RRAnswerTXT(const MDNSResponder::stcMDNS_RRHeader& p_Header,
        uint32_t p_u32TTL)
    :   stcMDNS_RRAnswer(enuAnswerType::TXT, p_Header, p_u32TTL)
{

}

/*
    MDNSResponder::stcMDNS_RRAnswerTXT::~stcMDNS_RRAnswerTXT destructor
*/
MDNSResponder::stcMDNS_RRAnswerTXT::~stcMDNS_RRAnswerTXT(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNS_RRAnswerTXT::clear
*/
bool MDNSResponder::stcMDNS_RRAnswerTXT::clear(void)
{

    m_Txts.clear();
    return true;
}


/**
    MDNSResponder::stcMDNS_RRAnswerAAAA

    A MDNS AAAA answer object.
    Extends the base class by an IPv6 address member.

*/

#ifdef MDNS_IPV6_SUPPORT
/*
    MDNSResponder::stcMDNS_RRAnswerAAAA::stcMDNS_RRAnswerAAAA constructor
*/
MDNSResponder::stcMDNS_RRAnswerAAAA::stcMDNS_RRAnswerAAAA(const MDNSResponder::stcMDNS_RRHeader& p_Header,
        uint32_t p_u32TTL)
    :   stcMDNS_RRAnswer(enuAnswerType::AAAA, p_Header, p_u32TTL),
        m_IPAddress()
{

}

/*
    MDNSResponder::stcMDNS_RRAnswerAAAA::~stcMDNS_RRAnswerAAAA destructor
*/
MDNSResponder::stcMDNS_RRAnswerAAAA::~stcMDNS_RRAnswerAAAA(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNS_RRAnswerAAAA::clear
*/
bool MDNSResponder::stcMDNS_RRAnswerAAAA::clear(void)
{

    m_IPAddress = IPAddress();
    return true;
}
#endif


/**
    MDNSResponder::stcMDNS_RRAnswerSRV

    A MDNS SRV answer object.
    Extends the base class by a port member.

*/

/*
    MDNSResponder::stcMDNS_RRAnswerSRV::stcMDNS_RRAnswerSRV constructor
*/
MDNSResponder::stcMDNS_RRAnswerSRV::stcMDNS_RRAnswerSRV(const MDNSResponder::stcMDNS_RRHeader& p_Header,
        uint32_t p_u32TTL)
    :   stcMDNS_RRAnswer(enuAnswerType::SRV, p_Header, p_u32TTL),
        m_u16Priority(0),
        m_u16Weight(0),
        m_u16Port(0)
{

}

/*
    MDNSResponder::stcMDNS_RRAnswerSRV::~stcMDNS_RRAnswerSRV destructor
*/
MDNSResponder::stcMDNS_RRAnswerSRV::~stcMDNS_RRAnswerSRV(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNS_RRAnswerSRV::clear
*/
bool MDNSResponder::stcMDNS_RRAnswerSRV::clear(void)
{

    m_u16Priority = 0;
    m_u16Weight = 0;
    m_u16Port = 0;
    m_SRVDomain.clear();
    return true;
}


/**
    MDNSResponder::stcMDNS_RRAnswerGeneric

    An unknown (generic) MDNS answer object.
    Extends the base class by a RDATA buffer member.

*/

/*
    MDNSResponder::stcMDNS_RRAnswerGeneric::stcMDNS_RRAnswerGeneric constructor
*/
MDNSResponder::stcMDNS_RRAnswerGeneric::stcMDNS_RRAnswerGeneric(const stcMDNS_RRHeader& p_Header,
        uint32_t p_u32TTL)
    :   stcMDNS_RRAnswer(enuAnswerType::Generic, p_Header, p_u32TTL),
        m_u16RDLength(0),
        m_pu8RDData(0)
{

}

/*
    MDNSResponder::stcMDNS_RRAnswerGeneric::~stcMDNS_RRAnswerGeneric destructor
*/
MDNSResponder::stcMDNS_RRAnswerGeneric::~stcMDNS_RRAnswerGeneric(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNS_RRAnswerGeneric::clear
*/
bool MDNSResponder::stcMDNS_RRAnswerGeneric::clear(void)
{

    if (m_pu8RDData)
    {
        delete[] m_pu8RDData;
        m_pu8RDData = 0;
    }
    m_u16RDLength = 0;

    return true;
}


/**
    MDNSResponder::stcProbeInformation_Base

    Probing status information for a host or service domain

*/

/*
    MDNSResponder::stcProbeInformation_Base::stcProbeInformation_Base constructor
*/
MDNSResponder::stcProbeInformation_Base::stcProbeInformation_Base(void)
    :   m_ProbingStatus(enuProbingStatus::WaitingForData),
        m_u8SentCount(0),
        m_Timeout(std::numeric_limits<esp8266::polledTimeout::oneShot::timeType>::max()),
        m_bConflict(false),
        m_bTiebreakNeeded(false)
{
}

/*
    MDNSResponder::stcProbeInformation_Base::clear
*/
bool MDNSResponder::stcProbeInformation_Base::clear(void)
{

    m_ProbingStatus = enuProbingStatus::WaitingForData;
    m_u8SentCount = 0;
    m_Timeout.reset(std::numeric_limits<esp8266::polledTimeout::oneShot::timeType>::max());
    m_bConflict = false;
    m_bTiebreakNeeded = false;

    return true;
}



/**
    MDNSResponder::stcProbeInformation_Host

    Probing status information for a host or service domain

*/

/*
    MDNSResponder::stcProbeInformation_Host::stcProbeInformation_Host constructor
*/
MDNSResponder::stcProbeInformation_Host::stcProbeInformation_Host(void)
    :   m_fnProbeResultCallback(0)
{
}

/*
    MDNSResponder::stcProbeInformation_Host::clear
*/
bool MDNSResponder::stcProbeInformation_Host::clear(bool p_bClearUserdata /*= false*/)
{

    if (p_bClearUserdata)
    {
        m_fnProbeResultCallback = 0;
    }
    return stcProbeInformation_Base::clear();
}


/**
    MDNSResponder::stcProbeInformation_Service

    Probing status information for a host or service domain

*/

/*
    MDNSResponder::stcProbeInformation_Service::stcProbeInformation_Service constructor
*/
MDNSResponder::stcProbeInformation_Service::stcProbeInformation_Service(void)
    :   m_fnProbeResultCallback(0)
{
}

/*
    MDNSResponder::stcProbeInformation_Service::clear
*/
bool MDNSResponder::stcProbeInformation_Service::clear(bool p_bClearUserdata /*= false*/)
{

    if (p_bClearUserdata)
    {
        m_fnProbeResultCallback = 0;
    }
    return stcProbeInformation_Base::clear();
}


/**
    MDNSResponder::stcMDNSService

    A MDNS service object (to be announced by the MDNS responder)
    The service instance may be '\0'; in this case the hostname is used
    and the flag m_bAutoName is set. If the hostname changes, all 'auto-
    named' services are renamed also.
    m_u8Replymask is used while preparing a response to a MDNS query. It is
    resetted in '_sendMDNSMessage' afterwards.
*/

/*
    MDNSResponder::stcMDNSService::stcMDNSService constructor
*/
MDNSResponder::stcMDNSService::stcMDNSService(const char* p_pcName /*= 0*/,
        const char* p_pcService /*= 0*/,
        const char* p_pcProtocol /*= 0*/)
    :   m_pNext(0),
        m_pcName(0),
        m_bAutoName(false),
        m_pcService(0),
        m_pcProtocol(0),
        m_u16Port(0),
        m_u32ReplyMask(0),
        m_fnTxtCallback(0)
{

    setName(p_pcName);
    setService(p_pcService);
    setProtocol(p_pcProtocol);
}

/*
    MDNSResponder::stcMDNSService::~stcMDNSService destructor
*/
MDNSResponder::stcMDNSService::~stcMDNSService(void)
{

    releaseName();
    releaseService();
    releaseProtocol();
}

/*
    MDNSResponder::stcMDNSService::setName
*/
bool MDNSResponder::stcMDNSService::setName(const char* p_pcName)
{

    bool bResult = false;

    releaseName();
    size_t stLength = (p_pcName ? strlen(p_pcName) : 0);
    if (stLength)
    {
        if ((bResult = (0 != (m_pcName = new char[stLength + 1]))))
        {
            strncpy(m_pcName, p_pcName, stLength);
            m_pcName[stLength] = 0;
        }
    }
    else
    {
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSService::releaseName
*/
bool MDNSResponder::stcMDNSService::releaseName(void)
{

    if (m_pcName)
    {
        delete[] m_pcName;
        m_pcName = 0;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSService::setService
*/
bool MDNSResponder::stcMDNSService::setService(const char* p_pcService)
{

    bool bResult = false;

    releaseService();
    size_t stLength = (p_pcService ? strlen(p_pcService) : 0);
    if (stLength)
    {
        if ((bResult = (0 != (m_pcService = new char[stLength + 1]))))
        {
            strncpy(m_pcService, p_pcService, stLength);
            m_pcService[stLength] = 0;
        }
    }
    else
    {
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSService::releaseService
*/
bool MDNSResponder::stcMDNSService::releaseService(void)
{

    if (m_pcService)
    {
        delete[] m_pcService;
        m_pcService = 0;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSService::setProtocol
*/
bool MDNSResponder::stcMDNSService::setProtocol(const char* p_pcProtocol)
{

    bool bResult = false;

    releaseProtocol();
    size_t stLength = (p_pcProtocol ? strlen(p_pcProtocol) : 0);
    if (stLength)
    {
        if ((bResult = (0 != (m_pcProtocol = new char[stLength + 1]))))
        {
            strncpy(m_pcProtocol, p_pcProtocol, stLength);
            m_pcProtocol[stLength] = 0;
        }
    }
    else
    {
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSService::releaseProtocol
*/
bool MDNSResponder::stcMDNSService::releaseProtocol(void)
{

    if (m_pcProtocol)
    {
        delete[] m_pcProtocol;
        m_pcProtocol = 0;
    }
    return true;
}


/**
    MDNSResponder::stcMDNSQuery

    A MDNS service query object.
    Service queries may be static or dynamic.
    As the static service query is processed in the blocking function 'queryService',
    only one static service service may exist. The processing of the answers is done
    on the WiFi-stack side of the ESP stack structure (via 'UDPContext.onRx(_update)').

*/

/**
    MDNSResponder::stcMDNSQuery::stcAnswer

    One answer for a service query.
    Every answer must contain
    - a service instance entry (pivot),
    and may contain
    - a host domain,
    - a port
    - an IPv4 address
    (- an IPv6 address)
    - a MDNS TXTs
    The existance of a component is flaged in 'm_u32ContentFlags'.
    For every answer component a TTL value is maintained.
    Answer objects can be connected to a linked list.

    For the host domain, service domain and TXTs components, a char array
    representation can be retrieved (which is created on demand).

*/

/**
    MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL

    The TTL (Time-To-Live) for an specific answer content.
    The 80% and outdated states are calculated based on the current time (millis)
    and the 'set' time (also millis).
    If the answer is scheduled for an update, the corresponding flag should be set.

*/

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::stcTTL constructor
*/
MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::stcTTL(void)
    :   m_u32TTL(0),
        m_TTLTimeout(std::numeric_limits<esp8266::polledTimeout::oneShot::timeType>::max()),
        m_TimeoutLevel(static_cast<typeTimeoutLevel>(enuTimeoutLevel::None))
{

}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::set
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::set(uint32_t p_u32TTL)
{

    m_u32TTL = p_u32TTL;
    if (m_u32TTL)
    {
        m_TimeoutLevel = static_cast<typeTimeoutLevel>(enuTimeoutLevel::Base);  // Set to 80%
        m_TTLTimeout.reset(timeout());
    }
    else
    {
        m_TimeoutLevel = static_cast<typeTimeoutLevel>(enuTimeoutLevel::None);  // undef
        m_TTLTimeout.reset(std::numeric_limits<esp8266::polledTimeout::oneShot::timeType>::max());
    }
    return true;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::flagged
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::flagged(void) const
{

    return ((m_u32TTL) &&
            (static_cast<typeTimeoutLevel>(enuTimeoutLevel::None) != m_TimeoutLevel) &&
            (((esp8266::polledTimeout::timeoutTemplate<false>*)&m_TTLTimeout)->expired())); // Cast-away the const; in case of oneShot-timer OK (but ugly...)
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::restart
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::restart(void)
{

    bool    bResult = true;

    if ((static_cast<typeTimeoutLevel>(enuTimeoutLevel::Base) <= m_TimeoutLevel) &&     // >= 80% AND
            (static_cast<typeTimeoutLevel>(enuTimeoutLevel::Final) > m_TimeoutLevel))       // < 100%
    {

        m_TimeoutLevel += static_cast<typeTimeoutLevel>(enuTimeoutLevel::Interval);     // increment by 5%
        m_TTLTimeout.reset(timeout());
    }
    else
    {
        bResult = false;
        m_TTLTimeout.reset(std::numeric_limits<esp8266::polledTimeout::oneShot::timeType>::max());
        m_TimeoutLevel = static_cast<typeTimeoutLevel>(enuTimeoutLevel::None);
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::prepareDeletion
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::prepareDeletion(void)
{

    m_TimeoutLevel = static_cast<typeTimeoutLevel>(enuTimeoutLevel::Final);
    m_TTLTimeout.reset(1 * 1000);   // See RFC 6762, 10.1

    return true;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::finalTimeoutLevel
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::finalTimeoutLevel(void) const
{

    return (static_cast<typeTimeoutLevel>(enuTimeoutLevel::Final) == m_TimeoutLevel);
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::timeout
*/
unsigned long MDNSResponder::stcMDNSQuery::stcAnswer::stcTTL::timeout(void) const
{

    uint32_t    u32Timeout = std::numeric_limits<esp8266::polledTimeout::oneShot::timeType>::max();

    if (static_cast<typeTimeoutLevel>(enuTimeoutLevel::Base) == m_TimeoutLevel)             // 80%
    {
        u32Timeout = (m_u32TTL * 800);                                                      // to milliseconds
    }
    else if ((static_cast<typeTimeoutLevel>(enuTimeoutLevel::Base) < m_TimeoutLevel) &&     // >80% AND
             (static_cast<typeTimeoutLevel>(enuTimeoutLevel::Final) >= m_TimeoutLevel))     // <= 100%
    {

        u32Timeout = (m_u32TTL * 50);
    }   // else: invalid
    return u32Timeout;
}


#ifdef MDNS_IPV4_SUPPORT
/**
    MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address

*/

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address::stcIPv4Address constructor
*/
MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address::stcIPv4Address(IPAddress p_IPAddress,
        uint32_t p_u32TTL /*= 0*/)
    :   m_pNext(0),
        m_IPAddress(p_IPAddress)
{

    m_TTL.set(p_u32TTL);
}
#endif


#ifdef MDNS_IPV6_SUPPORT
/**
    MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address

*/

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address::stcIPv6Address constructor
*/
MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address::stcIPv6Address(IPAddress p_IPAddress,
        uint32_t p_u32TTL /*= 0*/)
    :   m_pNext(0),
        m_IPAddress(p_IPAddress)
{

    m_TTL.set(p_u32TTL);
}
#endif


/**
    MDNSResponder::stcMDNSQuery::stcAnswer
*/

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::stcAnswer constructor
*/
MDNSResponder::stcMDNSQuery::stcAnswer::stcAnswer(void)
    :   m_pNext(0),
        m_pcServiceDomain(0),
        m_pcHostDomain(0),
        m_u16Port(0),
        m_pcTxts(0),
#ifdef MDNS_IPV4_SUPPORT
        m_pIPv4Addresses(0),
#endif
#ifdef MDNS_IPV6_SUPPORT
        m_pIPv6Addresses(0),
#endif
        m_QueryAnswerFlags(0)
{
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::~stcAnswer destructor
*/
MDNSResponder::stcMDNSQuery::stcAnswer::~stcAnswer(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::clear
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::clear(void)
{

    return ((releaseTxts()) &&
#ifdef MDNS_IPV4_SUPPORT
            (releaseIPv4Addresses()) &&
#endif
#ifdef MDNS_IPV6_SUPPORT
            (releaseIPv6Addresses()) &&
#endif
            (releaseHostDomain()) &&
            (releaseServiceDomain()));
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::allocServiceDomain

    Alloc memory for the char array representation of the service domain.

*/
char* MDNSResponder::stcMDNSQuery::stcAnswer::allocServiceDomain(size_t p_stLength)
{

    releaseServiceDomain();
    if (p_stLength)
    {
        m_pcServiceDomain = new char[p_stLength];
    }
    return m_pcServiceDomain;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::releaseServiceDomain
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::releaseServiceDomain(void)
{

    if (m_pcServiceDomain)
    {
        delete[] m_pcServiceDomain;
        m_pcServiceDomain = 0;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::allocHostDomain

    Alloc memory for the char array representation of the host domain.

*/
char* MDNSResponder::stcMDNSQuery::stcAnswer::allocHostDomain(size_t p_stLength)
{

    releaseHostDomain();
    if (p_stLength)
    {
        m_pcHostDomain = new char[p_stLength];
    }
    return m_pcHostDomain;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::releaseHostDomain
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::releaseHostDomain(void)
{

    if (m_pcHostDomain)
    {
        delete[] m_pcHostDomain;
        m_pcHostDomain = 0;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::allocTxts

    Alloc memory for the char array representation of the TXT items.

*/
char* MDNSResponder::stcMDNSQuery::stcAnswer::allocTxts(size_t p_stLength)
{

    releaseTxts();
    if (p_stLength)
    {
        m_pcTxts = new char[p_stLength];
    }
    return m_pcTxts;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::releaseTxts
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::releaseTxts(void)
{

    if (m_pcTxts)
    {
        delete[] m_pcTxts;
        m_pcTxts = 0;
    }
    return true;
}

#ifdef MDNS_IPV4_SUPPORT
/*
    MDNSResponder::stcMDNSQuery::stcAnswer::releaseIPv4Addresses
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::releaseIPv4Addresses(void)
{

    while (m_pIPv4Addresses)
    {
        stcIPv4Address*	pNext = m_pIPv4Addresses->m_pNext;
        delete m_pIPv4Addresses;
        m_pIPv4Addresses = pNext;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::addIPv4Address
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::addIPv4Address(MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address* p_pIPv4Address)
{

    bool bResult = false;

    if (p_pIPv4Address)
    {
        p_pIPv4Address->m_pNext = m_pIPv4Addresses;
        m_pIPv4Addresses = p_pIPv4Address;
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::removeIPv4Address
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::removeIPv4Address(MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address* p_pIPv4Address)
{

    bool    bResult = false;

    if (p_pIPv4Address)
    {
        stcIPv4Address*  pPred = m_pIPv4Addresses;
        while ((pPred) &&
                (pPred->m_pNext != p_pIPv4Address))
        {
            pPred = pPred->m_pNext;
        }
        if (pPred)
        {
            pPred->m_pNext = p_pIPv4Address->m_pNext;
            delete p_pIPv4Address;
            bResult = true;
        }
        else if (m_pIPv4Addresses == p_pIPv4Address)     // No predecesor, but first item
        {
            m_pIPv4Addresses = p_pIPv4Address->m_pNext;
            delete p_pIPv4Address;
            bResult = true;
        }
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::findIPv4Address (const)
*/
const MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address* MDNSResponder::stcMDNSQuery::stcAnswer::findIPv4Address(const IPAddress& p_IPAddress) const
{

    return (stcIPv4Address*)(((const stcAnswer*)this)->findIPv4Address(p_IPAddress));
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::findIPv4Address
*/
MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address* MDNSResponder::stcMDNSQuery::stcAnswer::findIPv4Address(const IPAddress& p_IPAddress)
{

    stcIPv4Address*	pIPv4Address = m_pIPv4Addresses;
    while (pIPv4Address)
    {
        if (pIPv4Address->m_IPAddress == p_IPAddress)
        {
            break;
        }
        pIPv4Address = pIPv4Address->m_pNext;
    }
    return pIPv4Address;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::IPv4AddressCount
*/
uint32_t MDNSResponder::stcMDNSQuery::stcAnswer::IPv4AddressCount(void) const
{

    uint32_t    u32Count = 0;

    stcIPv4Address*	pIPv4Address = m_pIPv4Addresses;
    while (pIPv4Address)
    {
        ++u32Count;
        pIPv4Address = pIPv4Address->m_pNext;
    }
    return u32Count;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::IPv4AddressAtIndex
*/
MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address* MDNSResponder::stcMDNSQuery::stcAnswer::IPv4AddressAtIndex(uint32_t p_u32Index)
{

    return (stcIPv4Address*)(((const stcAnswer*)this)->IPv4AddressAtIndex(p_u32Index));
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::IPv4AddressAtIndex (const)
*/
const MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv4Address* MDNSResponder::stcMDNSQuery::stcAnswer::IPv4AddressAtIndex(uint32_t p_u32Index) const
{

    const stcIPv4Address*	pIPv4Address = 0;

    if (((uint32_t)(-1) != p_u32Index) &&
            (m_pIPv4Addresses))
    {

        uint32_t    u32Index;
        for (pIPv4Address = m_pIPv4Addresses, u32Index = 0; ((pIPv4Address) && (u32Index < p_u32Index)); pIPv4Address = pIPv4Address->m_pNext, ++u32Index);
    }
    return pIPv4Address;
}
#endif

#ifdef MDNS_IPV6_SUPPORT
/*
    MDNSResponder::stcMDNSQuery::stcAnswer::releaseIPv6Addresses
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::releaseIPv6Addresses(void)
{

    while (m_pIPv6Addresses)
    {
        stcIPv6Address*	pNext = m_pIPv6Addresses->m_pNext;
        delete m_pIPv6Addresses;
        m_pIPv6Addresses = pNext;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::addIPv6Address
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::addIPv6Address(MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address* p_pIPv6Address)
{

    bool bResult = false;

    if (p_pIPv6Address)
    {
        p_pIPv6Address->m_pNext = m_pIPv6Addresses;
        m_pIPv6Addresses = p_pIPv6Address;
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::removeIPv6Address
*/
bool MDNSResponder::stcMDNSQuery::stcAnswer::removeIPv6Address(MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address* p_pIPv6Address)
{

    bool    bResult = false;

    if (p_pIPv6Address)
    {
        stcIPv6Address*	pPred = m_pIPv6Addresses;
        while ((pPred) &&
                (pPred->m_pNext != p_pIPv6Address))
        {
            pPred = pPred->m_pNext;
        }
        if (pPred)
        {
            pPred->m_pNext = p_pIPv6Address->m_pNext;
            delete p_pIPv6Address;
            bResult = true;
        }
        else if (m_pIPv6Addresses == p_pIPv6Address)     // No predecesor, but first item
        {
            m_pIPv6Addresses = p_pIPv6Address->m_pNext;
            delete p_pIPv6Address;
            bResult = true;
        }
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::findIPv6Address
*/
MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address* MDNSResponder::stcMDNSQuery::stcAnswer::findIPv6Address(const IPAddress& p_IPAddress)
{

    return (stcIPv6Address*)(((const stcAnswer*)this)->findIPv6Address(p_IPAddress));
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::findIPv6Address (const)
*/
const MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address* MDNSResponder::stcMDNSQuery::stcAnswer::findIPv6Address(const IPAddress& p_IPAddress) const
{

    const stcIPv6Address*	pIPv6Address = m_pIPv6Addresses;
    while (pIPv6Address)
    {
        if (pIPv6Address->m_IPAddress == p_IPAddress)
        {
            break;
        }
        pIPv6Address = pIPv6Address->m_pNext;
    }
    return pIPv6Address;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::IPv6AddressCount
*/
uint32_t MDNSResponder::stcMDNSQuery::stcAnswer::IPv6AddressCount(void) const
{

    uint32_t    u32Count = 0;

    stcIPv6Address*	pIPv6Address = m_pIPv6Addresses;
    while (pIPv6Address)
    {
        ++u32Count;
        pIPv6Address = pIPv6Address->m_pNext;
    }
    return u32Count;
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::IPv6AddressAtIndex (const)
*/
const MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address* MDNSResponder::stcMDNSQuery::stcAnswer::IPv6AddressAtIndex(uint32_t p_u32Index) const
{

    return (stcIPv6Address*)(((const stcAnswer*)this)->IPv6AddressAtIndex(p_u32Index));
}

/*
    MDNSResponder::stcMDNSQuery::stcAnswer::IPv6AddressAtIndex
*/
MDNSResponder::stcMDNSQuery::stcAnswer::stcIPv6Address* MDNSResponder::stcMDNSQuery::stcAnswer::IPv6AddressAtIndex(uint32_t p_u32Index)
{

    stcIPv6Address*	pIPv6Address = 0;

    if (((uint32_t)(-1) != p_u32Index) &&
            (m_pIPv6Addresses))
    {

        uint32_t    u32Index;
        for (pIPv6Address = m_pIPv6Addresses, u32Index = 0; ((pIPv6Address) && (u32Index < p_u32Index)); pIPv6Address = pIPv6Address->m_pNext, ++u32Index);
    }
    return pIPv6Address;
}
#endif


/**
    MDNSResponder::stcMDNSQuery

    A service query object.
    A static query is flaged via 'm_bLegacyQuery'; while the function 'queryService'
    is waiting for answers, the internal flag 'm_bAwaitingAnswers' is set. When the
    timeout is reached, the flag is removed. These two flags are only used for static
    service queries.
    All answers to the service query are stored in 'm_pAnswers' list.
    Individual answers may be addressed by index (in the list of answers).
    Every time a answer component is added (or changes) in a dynamic service query,
    the callback 'm_fnCallback' is called.
    The answer list may be searched by service and host domain.

    Service query object may be connected to a linked list.
*/

/*
    MDNSResponder::stcMDNSQuery::stcMDNSQuery constructor
*/
MDNSResponder::stcMDNSQuery::stcMDNSQuery(const enuQueryType p_QueryType)
    :   m_pNext(0),
        m_QueryType(p_QueryType),
        m_fnCallback(0),
        m_bLegacyQuery(false),
        m_u8SentCount(0),
        m_ResendTimeout(std::numeric_limits<esp8266::polledTimeout::oneShot::timeType>::max()),
        m_bAwaitingAnswers(true),
        m_pAnswers(0)
{

    clear();
    m_QueryType = p_QueryType;
}

/*
    MDNSResponder::stcMDNSQuery::~stcMDNSQuery destructor
*/
MDNSResponder::stcMDNSQuery::~stcMDNSQuery(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNSQuery::clear
*/
bool MDNSResponder::stcMDNSQuery::clear(void)
{

    m_pNext = 0;
    m_QueryType = enuQueryType::None;
    m_fnCallback = 0;
    m_bLegacyQuery = false;
    m_u8SentCount = 0;
    m_ResendTimeout.reset(std::numeric_limits<esp8266::polledTimeout::oneShot::timeType>::max());
    m_bAwaitingAnswers = true;
    while (m_pAnswers)
    {
        stcAnswer*  pNext = m_pAnswers->m_pNext;
        delete m_pAnswers;
        m_pAnswers = pNext;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSQuery::answerCount
*/
uint32_t MDNSResponder::stcMDNSQuery::answerCount(void) const
{

    uint32_t    u32Count = 0;

    stcAnswer*  pAnswer = m_pAnswers;
    while (pAnswer)
    {
        ++u32Count;
        pAnswer = pAnswer->m_pNext;
    }
    return u32Count;
}

/*
    MDNSResponder::stcMDNSQuery::answerAtIndex
*/
const MDNSResponder::stcMDNSQuery::stcAnswer* MDNSResponder::stcMDNSQuery::answerAtIndex(uint32_t p_u32Index) const
{

    const stcAnswer*    pAnswer = 0;

    if (((uint32_t)(-1) != p_u32Index) &&
            (m_pAnswers))
    {

        uint32_t    u32Index;
        for (pAnswer = m_pAnswers, u32Index = 0; ((pAnswer) && (u32Index < p_u32Index)); pAnswer = pAnswer->m_pNext, ++u32Index);
    }
    return pAnswer;
}

/*
    MDNSResponder::stcMDNSQuery::answerAtIndex
*/
MDNSResponder::stcMDNSQuery::stcAnswer* MDNSResponder::stcMDNSQuery::answerAtIndex(uint32_t p_u32Index)
{

    return (stcAnswer*)(((const stcMDNSQuery*)this)->answerAtIndex(p_u32Index));
}

/*
    MDNSResponder::stcMDNSQuery::indexOfAnswer
*/
uint32_t MDNSResponder::stcMDNSQuery::indexOfAnswer(const MDNSResponder::stcMDNSQuery::stcAnswer* p_pAnswer) const
{

    uint32_t    u32Index = 0;

    for (const stcAnswer* pAnswer = m_pAnswers; pAnswer; pAnswer = pAnswer->m_pNext, ++u32Index)
    {
        if (pAnswer == p_pAnswer)
        {
            return u32Index;
        }
    }
    return ((uint32_t)(-1));
}

/*
    MDNSResponder::stcMDNSQuery::addAnswer
*/
bool MDNSResponder::stcMDNSQuery::addAnswer(MDNSResponder::stcMDNSQuery::stcAnswer* p_pAnswer)
{

    bool    bResult = false;

    if (p_pAnswer)
    {
        p_pAnswer->m_pNext = m_pAnswers;
        m_pAnswers = p_pAnswer;
        bResult = true;
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSQuery::removeAnswer
*/
bool MDNSResponder::stcMDNSQuery::removeAnswer(MDNSResponder::stcMDNSQuery::stcAnswer* p_pAnswer)
{

    bool    bResult = false;

    if (p_pAnswer)
    {
        stcAnswer*  pPred = m_pAnswers;
        while ((pPred) &&
                (pPred->m_pNext != p_pAnswer))
        {
            pPred = pPred->m_pNext;
        }
        if (pPred)
        {
            pPred->m_pNext = p_pAnswer->m_pNext;
            delete p_pAnswer;
            bResult = true;
        }
        else if (m_pAnswers == p_pAnswer)   // No predecesor, but first item
        {
            m_pAnswers = p_pAnswer->m_pNext;
            delete p_pAnswer;
            bResult = true;
        }
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSQuery::findAnswerForServiceDomain
*/
MDNSResponder::stcMDNSQuery::stcAnswer* MDNSResponder::stcMDNSQuery::findAnswerForServiceDomain(const MDNSResponder::stcMDNS_RRDomain& p_ServiceDomain)
{

    stcAnswer*  pAnswer = m_pAnswers;
    while (pAnswer)
    {
        if (pAnswer->m_ServiceDomain == p_ServiceDomain)
        {
            break;
        }
        pAnswer = pAnswer->m_pNext;
    }
    return pAnswer;
}

/*
    MDNSResponder::stcMDNSQuery::findAnswerForHostDomain
*/
MDNSResponder::stcMDNSQuery::stcAnswer* MDNSResponder::stcMDNSQuery::findAnswerForHostDomain(const MDNSResponder::stcMDNS_RRDomain& p_HostDomain)
{

    stcAnswer*  pAnswer = m_pAnswers;
    while (pAnswer)
    {
        if (pAnswer->m_HostDomain == p_HostDomain)
        {
            break;
        }
        pAnswer = pAnswer->m_pNext;
    }
    return pAnswer;
}


/**
    MDNSResponder::stcMDNSSendParameter

    A 'collection' of properties and flags for one MDNS query or response.
    Mainly managed by the 'Control' functions.
    The current offset in the UPD output buffer is tracked to be able to do
    a simple host or service domain compression.

*/

/**
    MDNSResponder::stcMDNSSendParameter::stcDomainCacheItem

    A cached host or service domain, incl. the offset in the UDP output buffer.

*/

/*
    MDNSResponder::stcMDNSSendParameter::stcDomainCacheItem::stcDomainCacheItem constructor
*/
MDNSResponder::stcMDNSSendParameter::stcDomainCacheItem::stcDomainCacheItem(const void* p_pHostnameOrService,
        bool p_bAdditionalData,
        uint32_t p_u16Offset)
    :   m_pNext(0),
        m_pHostnameOrService(p_pHostnameOrService),
        m_bAdditionalData(p_bAdditionalData),
        m_u16Offset(p_u16Offset)
{

}

/**
    MDNSResponder::stcMDNSSendParameter
*/

/*
    MDNSResponder::stcMDNSSendParameter::stcMDNSSendParameter constructor
*/
MDNSResponder::stcMDNSSendParameter::stcMDNSSendParameter(void)
    :   m_pQuestions(0),
        m_Response(enuResponseType::None),
        m_pDomainCacheItems(0)
{

    clear();
}

/*
    MDNSResponder::stcMDNSSendParameter::~stcMDNSSendParameter destructor
*/
MDNSResponder::stcMDNSSendParameter::~stcMDNSSendParameter(void)
{

    clear();
}

/*
    MDNSResponder::stcMDNSSendParameter::clear
*/
bool MDNSResponder::stcMDNSSendParameter::clear(void)
{

    m_u16ID = 0;
    flushQuestions();
    m_u32HostReplyMask = 0;

    m_bLegacyQuery = false;
    m_Response = enuResponseType::None;
    m_bAuthorative = false;
    m_bCacheFlush = false;
    m_bUnicast = false;
    m_bUnannounce = false;

    m_u16Offset = 0;
    flushDomainCache();
    return true;
}

/*
    MDNSResponder::stcMDNSSendParameter::flushQuestions
*/
bool MDNSResponder::stcMDNSSendParameter::flushQuestions(void)
{

    while (m_pQuestions)
    {
        stcMDNS_RRQuestion* pNext = m_pQuestions->m_pNext;
        delete m_pQuestions;
        m_pQuestions = pNext;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSSendParameter::flushDomainCache
*/
bool MDNSResponder::stcMDNSSendParameter::flushDomainCache(void)
{

    while (m_pDomainCacheItems)
    {
        stcDomainCacheItem* pNext = m_pDomainCacheItems->m_pNext;
        delete m_pDomainCacheItems;
        m_pDomainCacheItems = pNext;
    }
    return true;
}

/*
    MDNSResponder::stcMDNSSendParameter::flushTempContent
*/
bool MDNSResponder::stcMDNSSendParameter::flushTempContent(void)
{

    m_u16Offset = 0;
    flushDomainCache();
    return true;
}

/*
    MDNSResponder::stcMDNSSendParameter::shiftOffset
*/
bool MDNSResponder::stcMDNSSendParameter::shiftOffset(uint16_t p_u16Shift)
{

    m_u16Offset += p_u16Shift;
    return true;
}

/*
    MDNSResponder::stcMDNSSendParameter::addDomainCacheItem
*/
bool MDNSResponder::stcMDNSSendParameter::addDomainCacheItem(const void* p_pHostnameOrService,
        bool p_bAdditionalData,
        uint16_t p_u16Offset)
{

    bool    bResult = false;

    stcDomainCacheItem* pNewItem = 0;
    if ((p_pHostnameOrService) &&
            (p_u16Offset) &&
            ((pNewItem = new stcDomainCacheItem(p_pHostnameOrService, p_bAdditionalData, p_u16Offset))))
    {

        pNewItem->m_pNext = m_pDomainCacheItems;
        bResult = ((m_pDomainCacheItems = pNewItem));
    }
    return bResult;
}

/*
    MDNSResponder::stcMDNSSendParameter::findCachedDomainOffset
*/
uint16_t MDNSResponder::stcMDNSSendParameter::findCachedDomainOffset(const void* p_pHostnameOrService,
        bool p_bAdditionalData) const
{

    const stcDomainCacheItem*   pCacheItem = m_pDomainCacheItems;

    for (; pCacheItem; pCacheItem = pCacheItem->m_pNext)
    {
        if ((pCacheItem->m_pHostnameOrService == p_pHostnameOrService) &&
                (pCacheItem->m_bAdditionalData == p_bAdditionalData))   // Found cache item
        {
            break;
        }
    }
    return (pCacheItem ? pCacheItem->m_u16Offset : 0);
}

}   // namespace MDNSImplementation

} // namespace esp8266



