#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <WiFiManager.h>
#include <ESP8266httpUpdate.h>

/// parameters
const unsigned int threshold = 200;
const IPAddress server(134, 157, 180, 144);
const IPAddress quanticSwitch(192, 168, 0, 121);
const int port = 3003;
String serverStr = String(server);
/// state
bool isOpen = true;

/// server 
WiFiServer internalServer(80);

String getHttpRequestParamValue(String input_str, String param) {
    int param_index = input_str.indexOf(param);
    if(param_index == -1) {return "\0";}
    int start_chr = input_str.indexOf("=", param_index + 1) + 1;
    if(input_str.charAt(param_index + param.length()) == 0x26) {return "NULL";}
    int end_chr = input_str.indexOf("&", start_chr);
    return input_str.substring(start_chr, end_chr);
}

bool checkHttpRequestParam(String input_str, String param) {
    int param_index = input_str.indexOf(param);
    if(param_index == -1) {return false;}
    else {return true;}
}

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
    // Handle incoming connections
    WiFiClient client = internalServer.available();
    if(client) {
        String rawRequest = client.readStringUntil('\r');
        String request = rawRequest;
        request.remove(rawRequest.indexOf("HTTP/1.1"));
        Serial.println("New Client Request : " + request);
        IPAddress remote = client.remoteIP();
        client.flush();
        // Check method
        if(checkHttpRequestParam(request, "POST")) {
            if(checkHttpRequestParam(request, "httpUpdate")) {
            // Respond to client
            String binPath = getHttpRequestParamValue(request, "httpUpdate");
            //Serial.println("httpUpdate toggled, path: " + server + binPath);
            t_httpUpdate_return ret = ESPhttpUpdate.update(serverStr, port, binPath);
            delay(1000);
            switch(ret) {
                case HTTP_UPDATE_FAILED:
                    Serial.println("[update] Update failed.");
                    Serial.println("Error" + String(ESPhttpUpdate.getLastError())  + ESPhttpUpdate.getLastErrorString().c_str());
                    break;
                case HTTP_UPDATE_NO_UPDATES:
                    Serial.println("[update] Update no Update.");
                    break;
                }
            }
        }
        if(checkHttpRequestParam(request, "POST")) {
             // DO SOME STUFF
        }
        client.stop(); 
    }

    // Handle door
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
