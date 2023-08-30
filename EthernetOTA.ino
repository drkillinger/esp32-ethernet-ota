
//
// Local ethernet adapted OTA updates, derived from ESP built-in OTA
//
// Long story short, ESP is Wifi only for both OTA and mDNS. This is a test to run an Ethernet centric solution using 
// a modified WifiOTA -> EthernetOTA together with a 3rd party mDNS library.
//
// Depends on  https://github.com/TrippyLighting/EthernetBonjour for discovery
//

#include <Ethernet.h>
#include "EthernetOTA.h" 

#define MAC_ADR { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED } 

EthernetClient eClient;

byte mac[] = MAC_ADR;
char server[] = "wifitest.adafruit.com";  // name address for adafruit (using DNS)


void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  Ethernet.init(33);  // ESP32 with Adafruit Featherwing Ethernet

  Ethernet.begin(mac);


  if (eClient.connect(server, 80)) {
    Serial.println("connected to testserver :80");
  } else {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }


  // Port defaults to 3232
  EthernetOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  EthernetOTA.setHostname("bedrooom");

  // No authentication by default
  EthernetOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  EthernetOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  EthernetOTA
    .onStart([]() {
      String type;
      if (EthernetOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      //Serial.printf("[%u%]% - ", (progress / (total / 100)));
      Serial.printf(".");
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  if (EthernetOTA.begin() ) {
    Serial.println("EthernetOTA initialized.");
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  EthernetOTA.handle();
}