#include <ESP8266WiFi.h>
#include <OneWire.h>

/// parameters
const unsigned int threshold = 200;
const char ssid[] = "passage-74";
const char password[] = "there is no password";
const IPAddress server(134, 157, 180, 144);
const int port = 3003;

/// state
bool isOpen = true;

/// setup is called once on startup.
void setup() {

    // define the pin acquiring the signal
    pinMode(A0, INPUT);

    // connect to the wifi
    Serial.begin(115200);
    Serial.print("Attempting connection to the access point ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected to the access point with ip ");
    Serial.println(WiFi.localIP());
}

/// loop is called repeatedly while the chip is on.
void loop() {
    if (isOpen != (analogRead(A0) > threshold)) {

        // connect to the server
        Serial.print("Attempting connection to the server ");
        Serial.print(server);
        Serial.print(":");
        Serial.print(port);
        Serial.print(" - ");
        WiFiClient client;
        if (!client.connect(server, port)) {
            Serial.println("connection failed");
            delay(500);
            return;
        }
        Serial.println("connected");

        // send the request
        String url = (isOpen ? "GET /closed/" : "GET /open");
        Serial.print(String("Sending the request '") + url + "' - ");
        client.println(url + " HTTP/1.1");
        client.println(String("Host: ") + server);
        client.println("Connection: keep-alive");
        client.println();

        // wait for response
        unsigned long now = millis();
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
