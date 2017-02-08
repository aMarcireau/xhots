#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266httpUpdate.h>

/// parameters
const unsigned int serverResponseTimeout = 3000; // milliseconds
const unsigned int masterConnectionRetryPeriod = 1000; // milliseconds
IPAddress masterIpAddress(192, 168, 0, 105);
const int masterPort = 3000;
const int animationDuration = 1000; // milliseconds
const int animationFrameDuration = 20; // milliseconds
const unsigned int internalServerPort = 3001;

/// globals
Adafruit_NeoPixel topStrip = Adafruit_NeoPixel(12, D1);
Adafruit_NeoPixel bottomStrip = Adafruit_NeoPixel(12, D2);
WiFiServer internalServer(internalServerPort);
bool isOpen = true;

/// fill sets the color of each LED on each strip.
void fill(unsigned char r, unsigned char g, unsigned char b) {
    {
        uint32_t color = topStrip.Color(r, g, b);
        for(uint16_t ledIndex = 0; ledIndex < topStrip.numPixels(); ++ledIndex) {
            topStrip.setPixelColor(ledIndex, color);
        }
    }
    {
        uint32_t color = bottomStrip.Color(r, g, b);
        for(uint16_t ledIndex = 0; ledIndex < bottomStrip.numPixels(); ++ledIndex) {
            bottomStrip.setPixelColor(ledIndex, color);
        }
    }
	topStrip.show();
	bottomStrip.show();
}

/// setup is called once on startup.
void setup() {

    // initialise LED strips
    topStrip.begin();
    bottomStrip.begin();
    topStrip.setBrightness(255);
    bottomStrip.setBrightness(255);
    topStrip.show();
    bottomStrip.show();
    fill(0, 0, 255);
    delay(animationFrameDuration);

    // connect to the wifi
    Serial.begin(115200);
    WiFiManager wifiManager;
    wifiManager.autoConnect("xHotsWifi_setup");
    Serial.print("connected to the access point with ip ");
    Serial.println(WiFi.localIP());

    // start the internal server
    internalServer.begin();
    internalServer.setNoDelay(true);
    Serial.println(String("internal server listening on port ") + String(internalServerPort));

    // retrieve the current door status
    {
        WiFiClient client;
        for (;;) {
            if (client.connect(masterIpAddress, masterPort)) {
                Serial.println("connected to master, sending a request to get the current door state");
                client.println("GET / HTTP/1.1");
                client.println(String("Host: ") + masterIpAddress.toString() + ":" + String(masterPort));
                client.println("Connection: close");
                client.println();
                bool stateSet = false;
                unsigned long now = millis();
                while (client.connected()) {
                    if (millis() - now > serverResponseTimeout) {
                        Serial.println("request to master timeout");
                        break;
                    }
                    if (client.available() > 0) {
                        String response = client.readStringUntil('\r');
						Serial.println("master response: '" + response + "'");
                        isOpen = !(response.substring(13) == "closed");
						Serial.println(String("status updated, the door is now ") + (isOpen ? "open" : "closed"));
                        if (isOpen) {
							fill(0, 255, 0);
                        } else {
                            fill(255, 0, 0);
                        }
                        delay(animationFrameDuration);
                        stateSet = true;
                        break;
                    }
                }
                if (stateSet) {
                    break;
                }
            } else {
                Serial.println("connection to master failed");
                delay(masterConnectionRetryPeriod);
            }
        }
    }
}

/// loop is called repeatedly while the chip is on.
void loop() {
    WiFiClient client = internalServer.available();
    if (client) {
		
		Serial.println("received something!"); // @DEBUG
		
        unsigned long now = millis();
        while (client.connected()) {
            if (millis() - now > serverResponseTimeout) {
                client.println("HTTP/1.1 422 Unprocessable Entity\r\n");
                client.stop();
                break;
            }
            if (client.available()) {
                String requestPart = client.readStringUntil('\n');
				
				Serial.println(String("received request part: [[[") + requestPart + "]]]"); // @DEBUG
                
				if (requestPart.substring(0, 7) == "status=") {
                    if (isOpen != !(requestPart.substring(7) == "closed")) {
                        isOpen = !isOpen;
                        Serial.println(String("status updated, the door is now ") + (isOpen ? "open" : "closed"));
                        for (unsigned int frameIndex = 0; frameIndex <= animationDuration / animationFrameDuration; ++frameIndex) {
                            float progress = (float)frameIndex / (animationDuration / animationFrameDuration);
                            if (progress < 0.5) {
                                if (isOpen) {
                                    fill(255, (unsigned char)(510 * progress), 0);
                                } else {
                                    fill((unsigned char)(510 * progress), 255, 0);
                                }
                            } else {
                                if (isOpen) {
                                    fill((unsigned char)(510 * (1 - progress)), 255, 0);
                                } else {
                                    fill(255, (unsigned char)(510 * (1 - progress)), 0);
                                }
                            }
                            delay(animationFrameDuration);
                        }
                    }
					break;
                } else if (requestPart.substring(0, 5) == "path=") {
                    String path = requestPart.substring(5);
                    Serial.println(String("will reboot using the firmware located at '") + path + "'");
                    switch(ESPhttpUpdate.update(path)) {
                        case HTTP_UPDATE_FAILED:
                            Serial.println(String("http-update failed with error '") + ESPhttpUpdate.getLastErrorString().c_str() + "' (" + String(ESPhttpUpdate.getLastError()) + ")");
                            client.println(
                                String("HTTP/1.1 200 http update failed with error '")
                                + ESPhttpUpdate.getLastErrorString().c_str()
                                + "' ("
                                + String(ESPhttpUpdate.getLastError())
                                + ")\r\n"
                            );
                            break;
                        case HTTP_UPDATE_NO_UPDATES:
                            Serial.println("http-update did nothing");
                            client.println("HTTP/1.1 200 http-update did nothing\r\n");
                            break;
                        case HTTP_UPDATE_OK:
                            Serial.println("http-update successful\r\n");
                            break;
                    }
                    break;
                }
            }
        }
    }
}
