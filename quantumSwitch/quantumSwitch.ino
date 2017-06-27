#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266httpUpdate.h>
#include <TimeLib.h>
#include <WiFiUdp.h>

/// parameters
const unsigned int serverResponseTimeout = 3000; // milliseconds
const unsigned int masterConnectionRetryPeriod = 1000; // milliseconds
IPAddress masterIpAddress(134, 157, 180, 174);
const int masterPort = 3000;
const int animationDuration = 1000; // milliseconds
const int animationFrameDuration = 20; // milliseconds
const unsigned int internalServerPort = 3001;
static const char ntpServerName[] = "us.pool.ntp.org"; // NTP server to check time
const int timeZone = 1; // East European time
unsigned int localPort = 8888;  // local port to listen for UDP packets

/// globals
WiFiUDP udp;
Adafruit_NeoPixel topStrip = Adafruit_NeoPixel(12, D1);
Adafruit_NeoPixel bottomStrip = Adafruit_NeoPixel(12, D2);
WiFiServer internalServer(internalServerPort);
bool isOpen = true;
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

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

/// getNtpTile retrieves the current time from a server.
time_t getNtpTime() {
    IPAddress ntpServerIP; // NTP server's ip address

    while (udp.parsePacket() > 0) {} // discard any previously received packets
    Serial.println("Transmit NTP Request");

    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNtpPacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
            Serial.println("Receive NTP Response");
            udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
            unsigned long secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        }
    }
    Serial.println("No NTP Response :-(");
    return 0; // return 0 if unable to get the time
}

/// sendNtpPacket sends an NTP request to the time server at the given address.
void sendNtpPacket(IPAddress& address) {
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}

/// printDigits is used by digitalClockDisplay.
void printDigits(int digits) {
    Serial.print(":");
    if (digits < 10) {
        Serial.print('0');
    }
    Serial.print(digits);
}

/// digitalClockDisplay prints the retrieved time in the terminal.
void digitalClockDisplay() {
    // digital clock display of the time
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.print(" ");
    Serial.print(day());
    Serial.print(".");
    Serial.print(month());
    Serial.print(".");
    Serial.print(year());
    Serial.println();
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

    // start the serial connection for debug
    Serial.begin(115200);

    // connect to the wifi
    WiFiManager wifiManager;
    wifiManager.autoConnect("xHotsWifi_setup");
    Serial.print("connected to the access point with ip ");
    Serial.println(WiFi.localIP());

    // start the internal server
    internalServer.begin();
    internalServer.setNoDelay(true);
    Serial.println(String("internal server listening on port ") + String(internalServerPort));

    // Start UDP service to receive NTP packets
    Serial.println("Starting UDP");
    udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(udp.localPort());
    Serial.println("waiting for sync");
    setSyncProvider(getNtpTime);
    setSyncInterval(3600);

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
        unsigned long now = millis();
        while (client.connected()) {
            if (millis() - now > serverResponseTimeout) {
                client.println("HTTP/1.1 422 Unprocessable Entity\r\n");
                client.stop();
                break;
            }
            if (client.available()) {
                String requestPart = client.readStringUntil('\n');
                
                Serial.println(requestPart); // @DEBUG
                
				        if (requestPart.substring(0, 7) == "status=") {
                    client.println("HTTP/1.1 200 OK\r\n");
                    if (isOpen == (requestPart.substring(7, 13) == "closed")) {
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
                                String("HTTP/1.1 500 http update failed with error '")
                                + ESPhttpUpdate.getLastErrorString().c_str()
                                + "' ("
                                + String(ESPhttpUpdate.getLastError())
                                + ")\r\n"
                            );
                            break;
                        case HTTP_UPDATE_NO_UPDATES:
                            Serial.println("http-update did nothing");
                            client.println("HTTP/1.1 500 http-update did nothing\r\n");
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

    // Manage auto shutdown
    if (hour() == 21) {
        for (unsigned int i = 0; i <= 255; i++) {
            fill((unsigned char)(255 - i), (unsigned char)(255 - i), (unsigned char)(0));
            delay(animationFrameDuration);
        }
        if (weekday() > 0 && weekday() < 5) {
            // Turn off for the night
            Serial.println("Going to sleep for the night");
            delay(500);
            ESP.deepSleep(38700 * 1000000); // 10h45 of sleep Time --> will wake up at 7h45 AM
        } else {
            // Turn off for the weekend
            Serial.println("Going to sleep for the week end");
            delay(500);
            ESP.deepSleep(211500 * 1000000);
        }
    }
}
