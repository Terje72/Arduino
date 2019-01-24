/*
  ESP8266 mDNS Responder Service Monitor

  This example demonstrates two features of the LEA MDNSResponder:
  1. The host and service domain negotiation process that ensures
     the uniqueness of the finally choosen host and service domain name.
  2. The dynamic MDNS service lookup/query feature.

  A list of 'HTTP' services in the local network is created and kept up to date.
  In addition to this, a (very simple) HTTP server is set up on port 80
  and announced as a service.

  The ESP itself is initially announced to clients as 'esp8266.local', if this host domain
  is already used in the local network, another host domain is negociated. Keep an
  eye to the serial output to learn the final host domain for the HTTP service.
  The service itself is is announced as 'host domain'._http._tcp.local.
  The HTTP server delivers a short greeting and the current  list of other 'HTTP' services (not updated).
  The web server code is taken nearly 1:1 from the 'mDNS_Web_Server.ino' example.
  Point your browser to 'host domain'.local to see this web page.

  Instructions:
  - Update WiFi SSID and password as necessary.
  - Flash the sketch to the ESP8266 board
  - Install host software:
    - For Linux, install Avahi (http://avahi.org/).
    - For Windows, install Bonjour (http://www.apple.com/support/bonjour/).
    - For Mac OSX and iOS support is built in through Bonjour already.
  - Use a browser like 'Safari' to see the page at http://'host domain'.local.

*/


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

/*
   Include the MDNSResponder (the library needs to be included also)
   As LEA MDNSResponder is experimantal in the ESP8266 environment currently, the
   legacy MDNSResponder is defaulted in th include file.
   There are two ways to access LEA MDNSResponder:
   1. Prepend every declaration and call to global declarations or functions with the namespace, like:
      'LEAmDNS:MDNSResponder::hMDNSService  hMDNSService;'
      This way is used in the example. But be careful, if the namespace declaration is missing
      somewhere, the call might go to the legacy implementation...
   2. Open 'ESP8266mDNS.h' and set LEAmDNS to default.

*/
#include <ESP8266mDNS.h>

/*
   Global defines and vars
*/

#define SERVICE_PORT                                    80                                  // HTTP port

#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK  "your-password"
#endif

const char*                                    ssid                    = STASSID;
const char*                                    password                = STAPSK;

char*                                          pcHostDomain            = 0;        // Negociated host domain
bool                                           bHostDomainConfirmed    = false;    // Flags the confirmation of the host domain
MDNSResponder::hMDNSService                    hMDNSService            = 0;        // The handle of the http service in the MDNS responder
MDNSResponder::hMDNSServiceQuery               hMDNSServiceQuery       = 0;        // The handle of the 'http.tcp' service query in the MDNS responder

const String                                   cstrNoHTTPServices      = "Currently no 'http.tcp' services in the local network!<br/>";
String                                         strHTTPServices         = cstrNoHTTPServices;

// HTTP server at port 'SERVICE_PORT' will respond to HTTP requests
ESP8266WebServer                                     server(SERVICE_PORT);


/*
   setStationHostname
*/
bool setStationHostname(const char* p_pcHostname) {

  if (p_pcHostname) {
    WiFi.hostname(p_pcHostname);
    Serial.printf("setStationHostname: Station hostname is set to '%s'\n", p_pcHostname);
  }
  return true;
}
/*
   MDNSServiceQueryCallback
*/

bool MDNSServiceQueryCallback (MDNSResponder::MDNSServiceInfo serviceInfo, uint32_t p_u32ServiceQueryAnswerMask, bool p_bSetContent)
{
	String answerInfo;
	switch (p_u32ServiceQueryAnswerMask){
	  case MDNSResponder::ServiceQueryAnswerType_ServiceDomain :
		  	  	  answerInfo = "ServiceDomain " + serviceInfo.serviceDomain();
		          break;
	  case MDNSResponder::ServiceQueryAnswerType_HostDomainAndPort :
		  	  	  answerInfo = "HostDomainAndPort " + serviceInfo.hostDomain() + ":" + String(serviceInfo.hostPort());
		          break;
	  case MDNSResponder::ServiceQueryAnswerType_IP4Address :
		  	  	  answerInfo = "IP4Address ";
		  	  	  for (IPAddress ip : serviceInfo.IPAdresses()){
		  	  		  answerInfo += "- " + ip.toString();
		  	  	  };
		          break;
	  case MDNSResponder::ServiceQueryAnswerType_Txts :
		  	  	  answerInfo = "TXT " + serviceInfo.strKeyValue();

		          break;
	  default :
		          answerInfo = "Unknown";
	}
	Serial.printf("Answer %s %s\n",answerInfo.c_str(),p_bSetContent ? "Modified" : "Deleted");

	return true;
}

bool MDNSServiceQueryCallback1(MDNSResponder* p_pMDNSResponder,                           // The MDNS responder object
                              const MDNSResponder::hMDNSServiceQuery p_hServiceQuery,    // Handle to the service query
                              uint32_t p_u32AnswerIndex,                                 // Index of the updated answer
                              uint32_t p_u32ServiceQueryAnswerMask,                      // Mask for the updated component
                              bool p_bSetContent,                                        // true: Component set, false: component deleted
                              void* p_pUserdata) {                                       // pUserdata; here '0', as none set via 'installServiceQuery'
  (void) p_pUserdata;
  Serial.printf("MDNSServiceQueryCallback\n");

  if ((p_pMDNSResponder) &&
      (hMDNSServiceQuery == p_hServiceQuery)) {

    if (MDNSResponder::ServiceQueryAnswerType_ServiceDomain & p_u32ServiceQueryAnswerMask) {
      Serial.printf("MDNSServiceQueryCallback: Service domain '%s' %s index %u\n",
                    p_pMDNSResponder->answerServiceDomain(p_hServiceQuery, p_u32AnswerIndex),
                    (p_bSetContent ? "added at" : "removed from"),
                    p_u32AnswerIndex);
    } else if (MDNSResponder::ServiceQueryAnswerType_HostDomainAndPort & p_u32ServiceQueryAnswerMask) {
      if (p_bSetContent) {
        Serial.printf("MDNSServiceQueryCallback: Host domain and port added/updated for service '%s': %s:%u\n",
                      p_pMDNSResponder->answerServiceDomain(p_hServiceQuery, p_u32AnswerIndex),
                      p_pMDNSResponder->answerHostDomain(p_hServiceQuery, p_u32AnswerIndex),
                      p_pMDNSResponder->answerPort(p_hServiceQuery, p_u32AnswerIndex));
      } else {
        Serial.printf("MDNSServiceQueryCallback: Host domain and port removed from service '%s'\n",
                      p_pMDNSResponder->answerServiceDomain(p_hServiceQuery, p_u32AnswerIndex));
      }
    } else if (MDNSResponder::ServiceQueryAnswerType_IP4Address & p_u32ServiceQueryAnswerMask) {
      if (p_bSetContent) {
        Serial.printf("MDNSServiceQueryCallback: IP4 address added/updated for service '%s': ",
                      p_pMDNSResponder->answerServiceDomain(p_hServiceQuery, p_u32AnswerIndex));
        for (uint32_t u = 0; u < p_pMDNSResponder->answerIP4AddressCount(p_hServiceQuery, p_u32AnswerIndex); ++u) {
          Serial.printf("- %s ", p_pMDNSResponder->answerIP4Address(p_hServiceQuery, p_u32AnswerIndex, u).toString().c_str());
        }
        Serial.printf("\n");
      } else {
        Serial.printf("MDNSServiceQueryCallback: IP4 address removed from service '%s'\n",
                      p_pMDNSResponder->answerServiceDomain(p_hServiceQuery, p_u32AnswerIndex));
      }
    } else if (MDNSResponder::ServiceQueryAnswerType_Txts & p_u32ServiceQueryAnswerMask) {
      if (p_bSetContent) {
        Serial.printf("MDNSServiceQueryCallback: TXT items added/updated for service '%s': %s\n",
                      p_pMDNSResponder->answerServiceDomain(p_hServiceQuery, p_u32AnswerIndex),
                      p_pMDNSResponder->answerTxts(p_hServiceQuery, p_u32AnswerIndex));
      } else {
        Serial.printf("MDNSServiceQueryCallback: TXT items removed from service '%s'\n",
                      p_pMDNSResponder->answerServiceDomain(p_hServiceQuery, p_u32AnswerIndex));
      }
    }

    //
    // Create the current list of 'http.tcp' services
    uint32_t    u32Answers = p_pMDNSResponder->answerCount(p_hServiceQuery);
    if (u32Answers) {
      strHTTPServices = "";
      for (uint32_t u = 0; u < u32Answers; ++u) {
        // Index and service domain
        strHTTPServices += "<li>";
        strHTTPServices += p_pMDNSResponder->answerServiceDomain(p_hServiceQuery, u);
        // Host domain and port
        if ((p_pMDNSResponder->hasAnswerHostDomain(p_hServiceQuery, u)) &&
            (p_pMDNSResponder->hasAnswerPort(p_hServiceQuery, u))) {
          strHTTPServices += "<br/>Hostname: ";
          strHTTPServices += p_pMDNSResponder->answerHostDomain(p_hServiceQuery, u);
          strHTTPServices += ":";
          strHTTPServices += p_pMDNSResponder->answerPort(p_hServiceQuery, u);
        }
        // IP4 address
        if (p_pMDNSResponder->hasAnswerIP4Address(p_hServiceQuery, u)) {
          strHTTPServices += "<br/>IP4: ";
          for (uint32_t u2 = 0; u2 < p_pMDNSResponder->answerIP4AddressCount(p_hServiceQuery, u); ++u2) {
            if (0 != u2) {
              strHTTPServices += ", ";
            }
            strHTTPServices += p_pMDNSResponder->answerIP4Address(p_hServiceQuery, u, u2).toString();
          }
        }
        // MDNS TXT items
        if (p_pMDNSResponder->hasAnswerTxts(p_hServiceQuery, u)) {
          strHTTPServices += "<br/>TXT: ";
          strHTTPServices += p_pMDNSResponder->answerTxts(p_hServiceQuery, u);
        }
        strHTTPServices += "</li>";
      }
    } else {
      strHTTPServices = cstrNoHTTPServices;
    }
  }
  return true;
}

/*
   MDNSServiceProbeResultCallback
   Probe result callback for Services
*/

bool serviceProbeResult (String p_pcServiceName,
                         const MDNSResponder::hMDNSService p_hMDNSService,
                         bool p_bProbeResult)
{
	(void) p_hMDNSService;
    Serial.printf("MDNSServiceProbeResultCallback: Service %s probe %s\n", p_pcServiceName.c_str(), (p_bProbeResult ? "succeeded." : "failed!"));
    return true;
}



/*
   MDNSHostProbeResultCallback

   Probe result callback for the host domain.
   If the domain is free, the host domain is set and the http service is
   added.
   If the domain is already used, a new name is created and the probing is
   restarted via p_pMDNSResponder->setHostname().

*/

bool hostProbeResult (String p_pcDomainName, bool p_bProbeResult) {

    Serial.printf("MDNSHostProbeResultCallback: Host domain '%s.local' is %s\n", p_pcDomainName.c_str(), (p_bProbeResult ? "free" : "already USED!"));

    if (true == p_bProbeResult) {
      // Set station hostname
      setStationHostname(pcHostDomain);

      if (!bHostDomainConfirmed) {
        // Hostname free -> setup clock service
        bHostDomainConfirmed = true;

        if (!hMDNSService) {
          // Add a 'http.tcp' service to port 'SERVICE_PORT', using the host domain as instance domain
          hMDNSService = MDNS.addService(0, "http", "tcp", SERVICE_PORT);
          if (hMDNSService) {
            MDNS.setServiceProbeResultCallback(hMDNSService, serviceProbeResult);

            // Add some '_http._tcp' protocol specific MDNS service TXT items
            // See: http://www.dns-sd.org/txtrecords.html#http
            MDNS.addServiceTxt(hMDNSService, "user", "");
            MDNS.addServiceTxt(hMDNSService, "password", "");
            MDNS.addServiceTxt(hMDNSService, "path", "/");
          }

          // Install dynamic 'http.tcp' service query
          if (!hMDNSServiceQuery) {
            hMDNSServiceQuery = MDNS.installServiceQuery("http", "tcp", MDNSServiceQueryCallback);
            if (hMDNSServiceQuery) {
              Serial.printf("MDNSProbeResultCallback: Service query for 'http.tcp' services installed.\n");
            } else {
              Serial.printf("MDNSProbeResultCallback: FAILED to install service query for 'http.tcp' services!\n");
            }
          }
        }
      }
    } else {
        // Change hostname, use '-' as divider between base name and index
        if (MDNSResponder::indexDomain(pcHostDomain, "-", 0)) {
          MDNS.setHostname(pcHostDomain);
        } else {
          Serial.println("MDNSProbeResultCallback: FAILED to update hostname!");
        }
      }

  return true;
}

/*
   HTTP request function (not found is handled by server)
*/
void handleHTTPRequest() {
  Serial.println("");
  Serial.println("HTTP Request");

  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  String s = "<!DOCTYPE HTML>\r\n<html><h3><head>Hello from ";
  s += WiFi.hostname() + ".local at " + ipStr + "</h3></head>";
  s += "<br/><h4>Local HTTP services are :</h4><ol>";
  s += strHTTPServices;
  // done :-)
  s += "</ol><br/>";

  Serial.println("Sending 200");
  server.send(200, "text/html", s);
  Serial.println("Done with request");
  Serial.printf("FM %d\n",ESP.getFreeHeap());
}

/*
   setup
*/
void setup(void) {
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup HTTP server
  server.on("/", handleHTTPRequest);

  // Setup MDNS responders
  MDNS.setHostProbeResultCallback(hostProbeResult);


  // Init the (currently empty) host domain string with 'esp8266'
  if ((!MDNSResponder::indexDomain(pcHostDomain, 0, "esp8266")) ||
      (!MDNS.begin(pcHostDomain))) {
    Serial.println(" Error setting up MDNS responder!");
    while (1) { // STOP
      delay(1000);
    }
  }
  Serial.println("MDNS responder started");

  // Start HTTP server
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  // Check if a request has come in
  server.handleClient();
  // Allow MDNS processing
  MDNS.update();

}

