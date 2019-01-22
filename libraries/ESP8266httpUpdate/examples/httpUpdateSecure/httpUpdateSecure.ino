// check for updates at: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266httpUpdate/examples/httpUpdateSecure/httpUpdateSecure.ino

/**
   httpUpdateSecure.ino

    Created on: 20.06.2018 as an adaptation of httpUpdate.ino

*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <time.h>

#define USE_SERIAL Serial

#ifndef APSSID
#define APSSID "APSSID"
#define APPSK  "APPSK"
#endif

ESP8266WiFiMulti WiFiMulti;

// A single, global CertStore which can be used by all
// connections.  Needs to stay live the entire time any of
// the WiFiClientBearSSLs are present.
#include <CertStoreBearSSL.h>
BearSSL::CertStore certStore;

#include <FS.h>
class SPIFFSCertStoreFile : public BearSSL::CertStoreFile {
  public:
    SPIFFSCertStoreFile(const char *name) {
      _name = name;
    };
    virtual ~SPIFFSCertStoreFile() override {};

    // The main API
    virtual bool open(bool write = false) override {
      _file = SPIFFS.open(_name, write ? "w" : "r");
      return _file;
    }
    virtual bool seek(size_t absolute_pos) override {
      return _file.seek(absolute_pos, SeekSet);
    }
    virtual ssize_t read(void *dest, size_t bytes) override {
      return _file.readBytes((char*)dest, bytes);
    }
    virtual ssize_t write(void *dest, size_t bytes) override {
      return _file.write((uint8_t*)dest, bytes);
    }
    virtual void close() override {
      _file.close();
    }

  private:
    File _file;
    const char *_name;
};

SPIFFSCertStoreFile certs_idx("/certs.idx");
SPIFFSCertStoreFile certs_ar("/certs.ar");

// Set time via NTP, as required for x.509 validation
void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC

  USE_SERIAL.print(F("Waiting for NTP time sync: "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    yield();
    delay(500);
    USE_SERIAL.print(F("."));
    now = time(nullptr);
  }

  USE_SERIAL.println(F(""));
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  USE_SERIAL.print(F("Current time: "));
  USE_SERIAL.print(asctime(&timeinfo));
}

void setup() {

  USE_SERIAL.begin(115200);
  // USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(APSSID, APPSK);

  SPIFFS.begin();

  int numCerts = certStore.initCertStore(&certs_idx, &certs_ar);
  USE_SERIAL.print(F("Number of CA certs read: ")); USE_SERIAL.println(numCerts);
  if (numCerts == 0) {
    USE_SERIAL.println(F("No certs found. Did you run certs-from-mozill.py and upload the SPIFFS directory before running?"));
    return; // Can't connect to anything w/o certs!
  }
}

void loop() {
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    setClock();

    BearSSL::WiFiClientSecure client;
    bool mfln = client.probeMaxFragmentLength("server", 443, 1024);  // server must be the same as in ESPhttpUpdate.update()
    USE_SERIAL.printf("MFLN supported: %s\n", mfln ? "yes" : "no");
    if (mfln) {
      client.setBufferSizes(1024, 1024);
    }
    client.setCertStore(&certStore);

    // The line below is optional. It can be used to blink the LED on the board during flashing
    // The LED will be on during download of one buffer of data from the network. The LED will
    // be off during writing that buffer to flash
    // On a good connection the LED should flash regularly. On a bad connection the LED will be
    // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
    // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, "https://server/file.bin");
    // Or:
    //t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 443, "file.bin");


    switch (ret) {
      case HTTP_UPDATE_FAILED:
        USE_SERIAL.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        USE_SERIAL.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        USE_SERIAL.println("HTTP_UPDATE_OK");
        break;
    }
  }
}
