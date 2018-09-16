/*
 ESP8266WiFiSTA.cpp - WiFi library for esp8266

 Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 Reworked on 28 Dec 2015 by Markus Sattler

 */

#include "ESP8266WiFi.h"
#include "ESP8266WiFiGeneric.h"
#include "ESP8266WiFiSTA.h"

#include "c_types.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "smartconfig.h"

extern "C" {
#include "lwip/err.h"
#include "lwip/dns.h"
#include "lwip/init.h" // LWIP_VERSION_
#if LWIP_IPV6
#include "lwip/netif.h" // struct netif
#endif
}

#include "debug.h"

extern "C" void esp_schedule();
extern "C" void esp_yield();

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- Private functions ------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static bool sta_config_equal(const station_config& lhs, const station_config& rhs);


/**
 * compare two STA configurations
 * @param lhs station_config
 * @param rhs station_config
 * @return equal
 */
static bool sta_config_equal(const station_config& lhs, const station_config& rhs) {
    if(strcmp(reinterpret_cast<const char*>(lhs.ssid), reinterpret_cast<const char*>(rhs.ssid)) != 0) {
        return false;
    }

    //in case of password, use strncmp with size 64 to cover 64byte psk case (no null term)
    if(strncmp(reinterpret_cast<const char*>(lhs.password), reinterpret_cast<const char*>(rhs.password), sizeof(lhs.password)) != 0) {
        return false;
    }

    if(lhs.bssid_set != rhs.bssid_set) {
        return false;
    }

    if(lhs.bssid_set) {
        if(memcmp(lhs.bssid, rhs.bssid, 6) != 0) {
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- STA function -----------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

bool ESP8266WiFiSTAClass::_useStaticIp = false;

/**
 * Start Wifi connection
 * if passphrase is set the most secure supported mode will be automatically selected
 * @param ssid const char*          Pointer to the SSID string.
 * @param passphrase const char *   Optional. Passphrase. Valid characters in a passphrase must be between ASCII 32-126 (decimal).
 * @param bssid uint8_t[6]          Optional. BSSID / MAC of AP
 * @param channel                   Optional. Channel of AP
 * @param connect                   Optional. call connect
 * @return
 */
wl_status_t ESP8266WiFiSTAClass::begin(const char* ssid, const char *passphrase, int32_t channel, const uint8_t* bssid, bool connect) {

    if(!WiFi.enableSTA(true)) {
        // enable STA failed
        return WL_CONNECT_FAILED;
    }

    if(!ssid || *ssid == 0x00 || strlen(ssid) > 31) {
        // fail SSID too long or missing!
        return WL_CONNECT_FAILED;
    }

    if(passphrase && strlen(passphrase) > 64) {
        // fail passphrase too long!
        return WL_CONNECT_FAILED;
    }

    struct station_config conf;
    strcpy(reinterpret_cast<char*>(conf.ssid), ssid);

    if(passphrase) {
        if (strlen(passphrase) == 64) // it's not a passphrase, is the PSK, which is copied into conf.password without null term
            memcpy(reinterpret_cast<char*>(conf.password), passphrase, 64);
        else
            strcpy(reinterpret_cast<char*>(conf.password), passphrase);
    } else {
        *conf.password = 0;
    }

    conf.threshold.rssi = -127;

    // TODO(#909): set authmode to AUTH_WPA_PSK if passphrase is provided
    conf.threshold.authmode = AUTH_OPEN;

    if(bssid) {
        conf.bssid_set = 1;
        memcpy((void *) &conf.bssid[0], (void *) bssid, 6);
    } else {
        conf.bssid_set = 0;
    }

    struct station_config conf_compare;
    if(WiFi._persistent){
        wifi_station_get_config_default(&conf_compare);
    }
    else {
        wifi_station_get_config(&conf_compare);
    }

    if(sta_config_equal(conf_compare, conf)) {
        DEBUGV("sta config unchanged");
    }
    else {
        ETS_UART_INTR_DISABLE();

        if(WiFi._persistent) {
            wifi_station_set_config(&conf);
        } else {
            wifi_station_set_config_current(&conf);
        }

        ETS_UART_INTR_ENABLE();
    }

    ETS_UART_INTR_DISABLE();
    if(connect) {
        wifi_station_connect();
    }
    ETS_UART_INTR_ENABLE();

    if(channel > 0 && channel <= 13) {
        wifi_set_channel(channel);
    }

    if(!_useStaticIp) {
        wifi_station_dhcpc_start();
    }

    return status();
}

wl_status_t ESP8266WiFiSTAClass::begin(char* ssid, char *passphrase, int32_t channel, const uint8_t* bssid, bool connect) {
    return begin((const char*) ssid, (const char*) passphrase, channel, bssid, connect);
}

/**
 * Use to connect to SDK config.
 * @return wl_status_t
 */
wl_status_t ESP8266WiFiSTAClass::begin() {

    if(!WiFi.enableSTA(true)) {
        // enable STA failed
        return WL_CONNECT_FAILED;
    }

    ETS_UART_INTR_DISABLE();
    wifi_station_connect();
    ETS_UART_INTR_ENABLE();

    if(!_useStaticIp) {
        wifi_station_dhcpc_start();
    }
    return status();
}

/**
 * Change IP configuration settings disabling the dhcp client
 * @param local_ip   Static ip configuration
 * @param gateway    Static gateway configuration
 * @param subnet     Static Subnet mask
 * @param dns1       Static DNS server 1
 * @param dns2       Static DNS server 2
 */
bool ESP8266WiFiSTAClass::config(IPAddress local_ip, IPAddress arg1, IPAddress arg2, IPAddress arg3, IPAddress dns2) {

  if(!WiFi.enableSTA(true)) {
      return false;
  }

  //ESP argument order is: ip, gateway, subnet, dns1
  //Arduino arg order is:  ip, dns, gateway, subnet.

  //first, check whether dhcp should be used, which is when ip == 0 && gateway == 0 && subnet == 0.
  bool espOrderUseDHCP = (local_ip == 0U && arg1 == 0U && arg2 == 0U);
  bool arduinoOrderUseDHCP = (local_ip == 0U && arg2 == 0U && arg3 == 0U);
  if (espOrderUseDHCP || arduinoOrderUseDHCP) {
      _useStaticIp = false;
      wifi_station_dhcpc_start();
      return true;
  }

  //To allow compatibility, check first octet of 3rd arg. If 255, interpret as ESP order, otherwise Arduino order.
  IPAddress gateway = arg1;
  IPAddress subnet = arg2;
  IPAddress dns1 = arg3;

  if(subnet[0] != 255)
  {
    //octet is not 255 => interpret as Arduino order
    gateway = arg2;
    subnet = arg3[0] == 0 ? IPAddress(255,255,255,0) : arg3; //arg order is arduino and 4th arg not given => assign it arduino default
    dns1 = arg1;
  }

  //ip and gateway must be in the same subnet
  if((local_ip & subnet) != (gateway & subnet)) {
    return false;
  }

  struct ip_info info;
  info.ip.addr = static_cast<uint32_t>(local_ip);
  info.gw.addr = static_cast<uint32_t>(gateway);
  info.netmask.addr = static_cast<uint32_t>(subnet);

  wifi_station_dhcpc_stop();
  if(wifi_set_ip_info(STATION_IF, &info)) {
      _useStaticIp = true;
  } else {
      return false;
  }

  if(dns1 != (uint32_t)0x00000000) {
      // Set DNS1-Server
      dns_setserver(0, dns1);
  }

  if(dns2 != (uint32_t)0x00000000) {
      // Set DNS2-Server
      dns_setserver(1, dns2);
  }

  return true;
}

/**
 * will force a disconnect an then start reconnecting to AP
 * @return ok
 */
bool ESP8266WiFiSTAClass::reconnect() {
    if((WiFi.getMode() & WIFI_STA) != 0) {
        if(wifi_station_disconnect()) {
            return wifi_station_connect();
        }
    }
    return false;
}

/**
 * Disconnect from the network
 * @param wifioff
 * @return  one value of wl_status_t enum
 */
bool ESP8266WiFiSTAClass::disconnect(bool wifioff) {
    bool ret;
    struct station_config conf;
    *conf.ssid = 0;
    *conf.password = 0;

    ETS_UART_INTR_DISABLE();
    if(WiFi._persistent) {
        wifi_station_set_config(&conf);
    } else {
        wifi_station_set_config_current(&conf);
    }
    ret = wifi_station_disconnect();
    ETS_UART_INTR_ENABLE();

    if(wifioff) {
        WiFi.enableSTA(false);
    }

    return ret;
}

/**
 * is STA interface connected?
 * @return true if STA is connected to an AD
 */
bool ESP8266WiFiSTAClass::isConnected() {
    return (status() == WL_CONNECTED);
}


/**
 * Setting the ESP8266 station to connect to the AP (which is recorded)
 * automatically or not when powered on. Enable auto-connect by default.
 * @param autoConnect bool
 * @return if saved
 */
bool ESP8266WiFiSTAClass::setAutoConnect(bool autoConnect) {
    bool ret;
    ETS_UART_INTR_DISABLE();
    ret = wifi_station_set_auto_connect(autoConnect);
    ETS_UART_INTR_ENABLE();
    return ret;
}

/**
 * Checks if ESP8266 station mode will connect to AP
 * automatically or not when it is powered on.
 * @return auto connect
 */
bool ESP8266WiFiSTAClass::getAutoConnect() {
    return (wifi_station_get_auto_connect() != 0);
}

/**
 * Set whether reconnect or not when the ESP8266 station is disconnected from AP.
 * @param autoReconnect
 * @return
 */
bool ESP8266WiFiSTAClass::setAutoReconnect(bool autoReconnect) {
    return wifi_station_set_reconnect_policy(autoReconnect);
}

/**
 * get whether reconnect or not when the ESP8266 station is disconnected from AP.
 * @return autoreconnect
 */
bool ESP8266WiFiSTAClass::getAutoReconnect() {
    return wifi_station_get_reconnect_policy();
}

/**
 * Wait for WiFi connection to reach a result
 * returns the status reached or disconnect if STA is off
 * @return wl_status_t
 */
uint8_t ESP8266WiFiSTAClass::waitForConnectResult() {
    //1 and 3 have STA enabled
    if((wifi_get_opmode() & 1) == 0) {
        return WL_DISCONNECTED;
    }
    while(status() == WL_DISCONNECTED) {
        delay(100);
    }
    return status();
}

/**
 * Get the station interface IP address.
 * @return IPAddress station IP
 */
IPAddress ESP8266WiFiSTAClass::localIP() {
    struct ip_info ip;
    wifi_get_ip_info(STATION_IF, &ip);
    return IPAddress(ip.ip.addr);
}

#if LWIP_IPV6

extern "C" struct netif netif_git[2]; // lwip2 interfaces

/**
 * Get the station interface IPv6 link-local address.
 * @return IPAddress station local-scope IPv6
 */
IPAddress ESP8266WiFiSTAClass::localIP6Link() {
    return IPAddress(netif_git[STATION_IF].ip6_addr[0]);
}

/**
 * Get the station interface IPv6 global address.
 * @return IPAddress station global-scope IPv6
 */
IPAddress ESP8266WiFiSTAClass::localIP6Global() {
    return IPAddress(netif_git[STATION_IF].ip6_addr[1]);
}

#endif // LWIP_IPV6

/**
 * Get the station interface MAC address.
 * @param mac   pointer to uint8_t array with length WL_MAC_ADDR_LENGTH
 * @return      pointer to uint8_t *
 */
uint8_t* ESP8266WiFiSTAClass::macAddress(uint8_t* mac) {
    wifi_get_macaddr(STATION_IF, mac);
    return mac;
}

/**
 * Get the station interface MAC address.
 * @return String mac
 */
String ESP8266WiFiSTAClass::macAddress(void) {
    uint8_t mac[6];
    char macStr[18] = { 0 };
    wifi_get_macaddr(STATION_IF, mac);

    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

/**
 * Get the interface subnet mask address.
 * @return IPAddress subnetMask
 */
IPAddress ESP8266WiFiSTAClass::subnetMask() {
    struct ip_info ip;
    wifi_get_ip_info(STATION_IF, &ip);
    return IPAddress(ip.netmask.addr);
}

/**
 * Get the gateway ip address.
 * @return IPAddress gatewayIP
 */
IPAddress ESP8266WiFiSTAClass::gatewayIP() {
    struct ip_info ip;
    wifi_get_ip_info(STATION_IF, &ip);
    return IPAddress(ip.gw.addr);
}

/**
 * Get the DNS ip address.
 * @param dns_no
 * @return IPAddress DNS Server IP
 */
IPAddress ESP8266WiFiSTAClass::dnsIP(uint8_t dns_no) {
#if LWIP_VERSION_MAJOR == 1
    ip_addr_t dns_ip = dns_getserver(dns_no);
    return IPAddress(dns_ip.addr);
#else
    return IPAddress(dns_getserver(dns_no));
#endif
}


/**
 * Get ESP8266 station DHCP hostname
 * @return hostname
 */
String ESP8266WiFiSTAClass::hostname(void) {
    return String(wifi_station_get_hostname());
}


/**
 * Set ESP8266 station DHCP hostname
 * @param aHostname max length:32
 * @return ok
 */
bool ESP8266WiFiSTAClass::hostname(char* aHostname) {
    if(strlen(aHostname) > 32) {
        return false;
    }
    return wifi_station_set_hostname(aHostname);
}

/**
 * Set ESP8266 station DHCP hostname
 * @param aHostname max length:32
 * @return ok
 */
bool ESP8266WiFiSTAClass::hostname(const char* aHostname) {
    return hostname((char*) aHostname);
}

/**
 * Set ESP8266 station DHCP hostname
 * @param aHostname max length:32
 * @return ok
 */
bool ESP8266WiFiSTAClass::hostname(String aHostname) {
    return hostname((char*) aHostname.c_str());
}

/**
 * Return Connection status.
 * @return one of the value defined in wl_status_t
 *
 */
wl_status_t ESP8266WiFiSTAClass::status() {
    station_status_t status = wifi_station_get_connect_status();

    switch(status) {
        case STATION_GOT_IP:
            return WL_CONNECTED;
        case STATION_NO_AP_FOUND:
            return WL_NO_SSID_AVAIL;
        case STATION_CONNECT_FAIL:
        case STATION_WRONG_PASSWORD:
            return WL_CONNECT_FAILED;
        case STATION_IDLE:
            return WL_IDLE_STATUS;
        default:
            return WL_DISCONNECTED;
    }
}

/**
 * Return the current SSID associated with the network
 * @return SSID
 */
String ESP8266WiFiSTAClass::SSID() const {
    struct station_config conf;
    wifi_station_get_config(&conf);
    return String(reinterpret_cast<char*>(conf.ssid));
}

/**
 * Return the current pre shared key associated with the network
 * @return  psk string
 */
String ESP8266WiFiSTAClass::psk() const {
    struct station_config conf;
    wifi_station_get_config(&conf);
    char tmp[65]; //psk is 64 bytes hex => plus null term
    memcpy(tmp, conf.password, sizeof(conf.password));
    tmp[64] = 0; //null term in case of 64 byte psk
    return String(reinterpret_cast<char*>(tmp));
}

/**
 * Return the current bssid / mac associated with the network if configured
 * @return bssid uint8_t *
 */
uint8_t* ESP8266WiFiSTAClass::BSSID(void) {
    static struct station_config conf;
    wifi_station_get_config(&conf);
    return reinterpret_cast<uint8_t*>(conf.bssid);
}

/**
 * Return the current bssid / mac associated with the network if configured
 * @return String bssid mac
 */
String ESP8266WiFiSTAClass::BSSIDstr(void) {
    struct station_config conf;
    char mac[18] = { 0 };
    wifi_station_get_config(&conf);
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", conf.bssid[0], conf.bssid[1], conf.bssid[2], conf.bssid[3], conf.bssid[4], conf.bssid[5]);
    return String(mac);
}

/**
 * Return the current network RSSI.
 * @return  RSSI value
 */
int32_t ESP8266WiFiSTAClass::RSSI(void) {
    return wifi_station_get_rssi();
}



// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------- STA remote configure -----------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

bool ESP8266WiFiSTAClass::_smartConfigStarted = false;
bool ESP8266WiFiSTAClass::_smartConfigDone = false;

/**
 * Start SmartConfig
 */
bool ESP8266WiFiSTAClass::beginSmartConfig() {
    if(_smartConfigStarted) {
        return false;
    }

    if(!WiFi.enableSTA(true)) {
        // enable STA failed
        return false;
    }

    if(smartconfig_start(reinterpret_cast<sc_callback_t>(&ESP8266WiFiSTAClass::_smartConfigCallback), 1)) {
        _smartConfigStarted = true;
        _smartConfigDone = false;
        return true;
    }
    return false;
}


/**
 *  Stop SmartConfig
 */
bool ESP8266WiFiSTAClass::stopSmartConfig() {
    if(!_smartConfigStarted) {
        return true;
    }

    if(smartconfig_stop()) {
        _smartConfigStarted = false;
        return true;
    }
    return false;
}

/**
 * Query SmartConfig status, to decide when stop config
 * @return smartConfig Done
 */
bool ESP8266WiFiSTAClass::smartConfigDone() {
    if(!_smartConfigStarted) {
        return false;
    }

    return _smartConfigDone;
}


/**
 * _smartConfigCallback
 * @param st
 * @param result
 */
void ESP8266WiFiSTAClass::_smartConfigCallback(uint32_t st, void* result) {
    sc_status status = (sc_status) st;
    if(status == SC_STATUS_LINK) {
        station_config* sta_conf = reinterpret_cast<station_config*>(result);

        wifi_station_set_config(sta_conf);
        wifi_station_disconnect();
        wifi_station_connect();

        _smartConfigDone = true;
    } else if(status == SC_STATUS_LINK_OVER) {
        WiFi.stopSmartConfig();
    }
}
