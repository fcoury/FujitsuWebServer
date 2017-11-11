#include <IRsend.h>
#include <ir_Fujitsu.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <SimpleDHT.h>

// ESP8266
const int led = BUILTIN_LED;

// temp sensor
const int pinDHT11 = D2;
SimpleDHT11 dht11;

// AC
IRFujitsuAC fujitsu(D0);

// WiFi
const char* ssid = "Biscayne2";
const char* password = "1212felipecoury";

// HTTP Server
ESP8266WebServer server(80);

// HTTP Client
const char* host = "fcoury-sensit.herokuapp.com";
const int httpsPort = 443;

WiFiClientSecure client;

void blink(int pause) {
  digitalWrite(led, 0);
  delay(pause);
  digitalWrite(led, 1);
}

void setup() {
  // Serial setup
  Serial.begin(115200);

  // LED setup
  pinMode(led, OUTPUT);

  // AC setup
  Serial.println("AC setup");
  fujitsu.begin();

  // WiFi setup
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS Responder Started");
  } 

  // HTTP Server
  server.on("/settemp", handleSetTemp);
  server.on("/gettemp", handleGetTemp);
  server.begin();

  // sends a ready blink
  delay(200);
  blink(500);
}

void send(byte temperature, byte humidity) {
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  String url = "/";
  Serial.print("requesting URL: ");
  Serial.println(url);

  String data = "{\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + "}";

  String request = String("POST ") + url + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" +
             "User-Agent: BuildFailureDetectorESP8266\r\n" +
             "Content-Type: application/json\r\n" +
             "Content-Length: " + data.length() + "\r\n" +
             "Connection: close\r\n\r\n" + data + "\r\n";
               
  Serial.print("Data: ");
  Serial.println(data);

  Serial.println("Request: ");
  Serial.println(request);
  client.print(request);

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");
  blink(1000);
}

// ----------------------
//  HTTP server handlers
// ----------------------

void jsonError(int code, String error) {
  server.send(code, "application/json", "{\"ok\": false, \"error\": \"" + error + "\"}");
  blink(500);
  blink(500);
}

void jsonData(String data) {
  server.send(200, "application/json", "{\"ok\": false, " + data + "}");
  blink(1000);
}

void jsonOK() {
  server.send(200, "application/json", "{\"ok\": true}");
  blink(1000);
}

void handleSetTemp() {
  String temperature = server.arg("temperature");
  if (temperature == "") {
    jsonError(422, "Missing temperature");
    return;
  }

  fujitsu.setCmd(FUJITSU_AC_CMD_TURN_ON);
  fujitsu.setSwing(FUJITSU_AC_SWING_OFF);
  fujitsu.setMode(FUJITSU_AC_MODE_COOL);
  fujitsu.setFanSpeed(FUJITSU_AC_FAN_HIGH);
  fujitsu.setTemp(temperature.toInt());
  fujitsu.send();

  jsonOK();
}

void handleGetTemp() {
  Serial.println("Sensing temperature...");
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err);
    jsonError(500, "Could not get temperature. Error: " + err);
    delay(1000);
    return;
  }

  jsonData("\"temperature\":" + String(temperature) + ", \"humidity\":" + String(humidity));
}

void sendTemp() {
  // read without samples.
  Serial.println("Sample DHT11...");
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
    return;
  }
  
  send(temperature, humidity);
}


void loop() {
  // HTTP server
  server.handleClient();
}
