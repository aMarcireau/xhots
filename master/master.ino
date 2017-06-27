#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266httpUpdate.h>

/// XhotsServer represents a server's IP address and port.
struct XhotsServer {
    String name;
    IPAddress ip;
    int port;
    bool isConnected;
    bool isOpen;
    WiFiClient client;
};

/// parameters
const unsigned int analogReadThreshold = 200;
const unsigned int serverResponseTimeout = 3000; // milliseconds
const unsigned int sensorDebounce = 500; // milliseconds
XhotsServer servers[] = {
    XhotsServer{"macmini", IPAddress(134, 157, 180, 96), 80},
    XhotsServer{"quantum switch", IPAddress(192, 168, 0, 106), 3001},
};
const unsigned int internalServerPort = 3000;

/// globals
const unsigned int numberOfServers = sizeof(servers) / sizeof(XhotsServer);
WiFiServer internalServer(internalServerPort);

/// setup is called once on startup.
void setup() {

    // define the pin acquiring the signal
    pinMode(A0, INPUT);

    // start the serial connection for debug
    Serial.begin(115200);

    // connect to the wifi
    WiFiManager wifiManager;
    wifiManager.autoConnect("xHotsWifi_setup");
    Serial.print("connected to the access point with ip ");
    Serial.println(WiFi.localIP());

    // set the clients' defaults
    for (unsigned int serverIndex = 0; serverIndex < numberOfServers; ++serverIndex) {
        XhotsServer& server = servers[serverIndex];
        server.isConnected = false;
        server.isOpen = true;
    }

    // start the internal server
    internalServer.begin();
    internalServer.setNoDelay(true);
    Serial.println(String("internal server listening on port ") + String(internalServerPort));
}

/// loop is called repeatedly while the chip is on.
void loop() {

    // manage the internal server
    {
        WiFiClient client = internalServer.available();
        if (client) {
            unsigned long now = millis();
            bool methodFound = false;
            bool isPost;
            while (client.connected()) {
                if (millis() - now > serverResponseTimeout) {
                    client.println("HTTP/1.1 422 Unprocessable Entity\r\n");
                    client.stop();
                    break;
                }
                if (client.available()) {
                    String requestPart = client.readStringUntil('\n');
                    Serial.println("received request part '" + requestPart + "'");
                    if (!methodFound) {
                        methodFound = true;
                        isPost = (requestPart.substring(0, 4) == "POST");
						Serial.println("received request part substring is '" + requestPart.substring(0, 5) + "', it " + (isPost ? "is a post" : "is not a post"));
                    } else {
                        if (isPost) {
                            Serial.println("received request part substring is '" + requestPart.substring(0, 5) + "'");
                            if (requestPart.substring(0, 5) == "path=") {
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
                        } else {
                            client.println(String("HTTP/1.1 200 ") + (analogRead(A0) > analogReadThreshold ? "open" : "closed") + "\r\n");
                            break;
                        }
                    }
                }
            }
        }
    }

    // manage the door
    {
        bool isOpen = analogRead(A0) > analogReadThreshold;
        String status = String("status=") + (isOpen ? "open" : "closed");

        // send a post request to each server
        for (unsigned int serverIndex = 0; serverIndex < numberOfServers; ++serverIndex) {
            XhotsServer& server = servers[serverIndex];
            if (server.isOpen != isOpen) {
                server.isConnected = server.client.connect(server.ip, server.port);
                if (server.isConnected) {
                    server.client.setNoDelay(true);
                    server.client.println("POST /xhots/update HTTP/1.1");
                    server.client.println(String("Host: ") + server.ip.toString() + ":" + String(server.port));
                    server.client.println("Connection: close");
                    server.client.println("Content-Type: application/x-www-form-urlencoded");
                    server.client.println(String("Content-Length: ") + String(status.length()));
                    server.client.println();
                    server.client.println(status);
                    Serial.println(String("sent '") + status + "' to '" + server.name + "'");
                } else {
                    Serial.println(String("connection to '") + server.name + "' failed");
                }
            }
        }

        // read responses from the servers
        {
            unsigned long now = millis();
            for (;;) {
                if (millis() - now > serverResponseTimeout) {
                    for (unsigned int serverIndex = 0; serverIndex < numberOfServers; ++serverIndex) {
                        XhotsServer& server = servers[serverIndex];
                        if (server.isOpen != isOpen && server.isConnected) {
                            Serial.println(String("'") + server.name + "' timed out");
                        }
                    }
                    break;
                }
                bool eachServerAnswered = true;
                for (unsigned int serverIndex = 0; serverIndex < numberOfServers; ++serverIndex) {
                    XhotsServer& server = servers[serverIndex];
                    if (server.isOpen != isOpen && server.isConnected) {
                        if (server.client.connected()) {
                            eachServerAnswered = false;
                            if (server.client.available() > 0) {
                                Serial.println(String("reponse line from '") + server.name + "': '" + server.client.readStringUntil('\r') + "'");
                            }
                        } else {
                            server.isOpen = isOpen;
                        }
                    }
                }
                if (eachServerAnswered) {
                    delay(sensorDebounce);
                    break;
                }
            }
        }

        // disconnect all the clients
        for (unsigned int serverIndex = 0; serverIndex < numberOfServers; ++serverIndex) {
            XhotsServer& server = servers[serverIndex];
            server.client.stop();
        }
    }
}
