#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <WiFiManager.h>

/// parameters
const unsigned int threshold = 200;
const IPAddress server(134, 157, 180, 144);
const IPAddress quanticSwitch(192, 168, 0, 121);
const int port = 3003;

/// state
bool isOpen = true;

/// setup is called once on startup.
void setup() {
    // define the pin acquiring the signal
    pinMode(A0, INPUT);

    // connect to the wifi
    Serial.begin(115200);
    WiFiManager wifiManager;
    wifiManager.autoConnect("xHotsWifi_setup");
    Serial.print("Connected to the access point with ip ");
    Serial.println(WiFi.localIP());
}

/// loop is called repeatedly while the chip is on.
void loop() {
    if (isOpen != (analogRead(A0) > threshold)) {
        WiFi.status();
        String url = (isOpen ? "GET /closed/" : "GET /open");
        // connect to the quanticSwitch
        Serial.print("Attempting connection to quanticSwitch ");
        Serial.print(quanticSwitch);
        Serial.print(":");
        Serial.print("80");
        Serial.print(" - ");
        WiFiClient client;
        client.stop();
        if (!client.connect(quanticSwitch, 80)) {
            Serial.println("connection failed");
            delay(500);
            return;
        }
        Serial.println("connected");

        // send the request to quanticSwitch
        Serial.print(String("Sending the request '") + url + "' - ");
        client.println(url + " HTTP/1.1");
        client.println(String("Host: ") + quanticSwitch);
        client.println("Connection: keep-alive");
        client.println();
      
        // wait for response
        unsigned long now = millis();
        while (client.available() == 0) {
            if (millis() - now > 3) {
                Serial.println("timeout");
                client.stop();
                return;
            }
            delay(100);
        }
        Serial.println("success");
        // read the response
        Serial.println("Response:");
        while (client.available() > 0) {
            Serial.print(String("    ") + client.readStringUntil('\r'));
        }
        Serial.println();
        client.stop();

        // connect to the server
        Serial.print("Attempting connection to the server ");
        Serial.print(server);
        Serial.print(":");
        Serial.print(port);
        Serial.print(" - ");
        
        if (!client.connect(server, port)) {
            Serial.println("connection failed");
            delay(500);
            return;
        }
        Serial.println("connected");

        // send the request to server
        Serial.print(String("Sending the request '") + url + "' - ");
        client.println(url + " HTTP/1.1");
        client.println(String("Host: ") + server);
        client.println("Connection: keep-alive");
        client.println();

        // wait for response
        now = millis();
        while (client.available() == 0) {
            if (millis() - now > 10000) {
                Serial.println("timeout");
                client.stop();
                return;
            }
            delay(100);
        }
        Serial.println("success");

        // read the response
        Serial.println("Response:");
        while (client.available() > 0) {
            Serial.print(String("    ") + client.readStringUntil('\r'));
        }
        Serial.println();
        client.stop();
        
        // update the internal state
        isOpen = !isOpen;

        
    }
}
