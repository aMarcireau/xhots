#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include <WiFiManager.h>

#define H_STRAND_PIN D1
#define L_STRAND_PIN D2

const IPAddress serverIP(134, 157, 180, 144);
const int port = 3003;

Adafruit_NeoPixel strip_h = Adafruit_NeoPixel(12, H_STRAND_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip_l = Adafruit_NeoPixel(12, L_STRAND_PIN, NEO_GRB + NEO_KHZ800);

WiFiServer server(80);
bool state = true;
unsigned int serverResponseTimeout = 5000;

// Fill the dots one after the other with a color
void setHighColor(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip_h.numPixels(); i++) {
    strip_h.setPixelColor(i, c);
    strip_h.show();
    delay(wait);
  }
}


void setLowColor(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip_h.numPixels(); i++) {
    strip_l.setPixelColor(i, c);
    strip_l.show();
    delay(wait);
  }
}


void setLightSignal(bool FREE){
    uint32_t color;
    if(FREE) {
      color = strip_h.Color(255, 0, 0);
    }
    else {
      color = strip_h.Color(0, 255, 0);
    }
    setHighColor(color, 0);
    setLowColor(color, 0);
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFiManager wifiManager;
  wifiManager.autoConnect("quantiChiotsSwitch");
  Serial.println(WiFi.SSID());
  strip_l.begin();
  strip_h.begin();
  strip_l.setBrightness(255);
  strip_l.setBrightness(255);
  strip_l.show();
  strip_h.show();
  server.begin();
  Serial.println("Server started");
  // Get initial state
  WiFiClient client;
  Serial.println("Getting door status...");
  do {
    delay(50);
  } while(!client.connect(serverIP, port));
  client.println("GET /status/ HTTP/1.1");
  client.println(String("Host: ") + serverIP);
  client.println("Connection: close");
  client.println();
      
  // wait for response
  unsigned long now = millis();
  while (client.available() == 0) {
    if (millis() - now > 5000) {
      Serial.println("timeout");
      client.stop();
      return;
      }
    delay(100);
    }
  Serial.println("success");
  // read the response
  Serial.println("Response:");
  String response = "";
  while (client.available() > 0) {
    response += client.readStringUntil('\r');
    //Serial.print(String("    ") + client.readStringUntil('\r'));
  }
  Serial.println(response);
  client.stop();
  if(response.indexOf("open") != -1){
    setLightSignal(false);
  }
  if(response.indexOf("closed") != -1){
    setLightSignal(true);
  }
}

void loop() {
    WiFiClient client = server.available();
    if (client) {
		unsigned long now = millis();
		while (client.connected()) {
			if (millis() - now > serverResponseTimeout) {
				client.println("HTTP/1.1 200 OK\r\n");
			    client.stop();
			    break;
			}
			if(client.available()){
				String requestPart = client.readStringUntil('\n');
				if(requestPart.indexOf("closed") != -1) {
					setLightSignal(true);
					Serial.println("Just got trigger from master --> close");
					break;
				}
				if(requestPart.indexOf("open") != -1) {
					setLightSignal(false);
					Serial.println("Just got trigger from master --> open");
					break;
				}
			}
		}
		client.println("HTTP/1.1 200 OK \n\r");
		client.stop();
	}
}   	   
