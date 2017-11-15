#include <IRsend.h>
#include <ir_Fujitsu.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <SimpleDHT.h>

// Send temperature interval
const int REFRESH_INTERVAL = 60 * 1000; // ms
int lastRefresh = 0;

// ESP8266
const int led = BUILTIN_LED;

// temp sensor
const int pinDHT11 = D2;
SimpleDHT11 dht11;

// AC
IRFujitsuAC fujitsu(D0);
int currentTemp = 18;
int currentSwing = FUJITSU_AC_SWING_OFF;
int currentMode = FUJITSU_AC_MODE_COOL;
int currentFanSpeed = FUJITSU_AC_FAN_HIGH;

// WiFi
const char* ssid = "Biscayne2";
const char* password = "1212felipecoury";

// HTTP Server
ESP8266WebServer server(80);

// HTTP Client
const char* host = "fcoury-sensit.herokuapp.com";
const int httpsPort = 443;

WiFiClientSecure client;

// ----------------------------
//  Main code - setup and loop
// ----------------------------

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
  server.on("/dectemp", handleDecTemp);
  server.on("/inctemp", handleIncTemp);
  server.on("/gettemp", handleGetTemp);
  server.on("/setfan",  handleSetFan);
  server.on("/status",  handleGetStatus);
  server.on("/turnoff", handleTurnOff);
  server.begin();

  // sends a ready blink
  delay(200);
  blink(100);
}

void loop() {
  // HTTP server
  server.handleClient();

  if (millis() - lastRefresh >= REFRESH_INTERVAL) {
    lastRefresh = millis();
    sendTemp();
  }
}


void blink(int pause) {
  digitalWrite(led, 0);
  delay(pause);
  digitalWrite(led, 1);
}

void sendTemp() {
  // read without samples.
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
    return;
  }
  
  send(temperature, humidity);
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
  blink(200);
}

// ------------
//  AC helpers
// ------------

void incTemp() {
  currentTemp += 1;
  if (currentTemp > 24) {
    currentTemp = 24;
  }
  sendAC(-1);
}

void decTemp() {
  currentTemp -= 1;
  if (currentTemp < 18) {
    currentTemp = 18;
  }
  sendAC(-1);
}

void setFanSpeed(int speed) {
  currentFanSpeed = speed;
  sendAC(-1);
}

void setTemp(int temp) {
  currentTemp = temp;
  sendAC(FUJITSU_AC_CMD_TURN_ON);
}

void sendAC(int command) {
  Serial.println("Sending AC command");
  
  if (command > -1) {
    Serial.print("command: ");
    Serial.print(command);
    Serial.print(" | ");
    fujitsu.setCmd(command);
  }
  Serial.print("swing: ");
  Serial.print(currentSwing);
  fujitsu.setSwing(currentSwing);
  Serial.print(" | mode: ");
  Serial.print(currentMode);
  fujitsu.setMode(currentMode);
  Serial.print(" | fanSpeed: ");
  Serial.print(currentFanSpeed);
  fujitsu.setFanSpeed(currentFanSpeed);
  Serial.print(" | temperature: ");
  Serial.println(currentTemp);
  fujitsu.setTemp(currentTemp);
  fujitsu.send();

  sendACStatus();
}

void sendACStatus() {
  String data = "\"temperature\": " + String(currentTemp);
  data += ", \"swing\": " + String(currentSwing);
  data += ", \"mode\": " + String(currentMode);
  data += ", \"fanSpeed\": " + String(currentFanSpeed);

  jsonData(data);
}

// ---------------------
//  HTTP server helpers
// ---------------------

void jsonError(int code, String error) {
  server.send(code, "application/json", "{\"ok\": false, \"error\": \"" + error + "\"}");
  blink(200);
  blink(200);
}

void jsonData(String data) {
  server.send(200, "application/json", "{\"ok\": true, " + data + "}");
  blink(200);
}

void jsonOK() {
  server.send(200, "application/json", "{\"ok\": true}");
  blink(200);
}

// ----------------------
//  HTTP server handlers
// ----------------------

void handleSetTemp() {
  String temperature = server.arg("temperature");
  if (temperature == "") {
    jsonError(422, "Missing temperature");
    return;
  }

  setTemp(temperature.toInt());
}

void handleIncTemp() {
  Serial.print("Increasing current temperature: ");
  Serial.println(currentTemp);
  incTemp();
}

void handleDecTemp() {
  Serial.print("Decreasing current temperature: ");
  Serial.println(currentTemp);
  decTemp();
}

void handleSetFan() {
  String speed = server.arg("speed");

  if (speed == "") {
    jsonError(422, "missing speed parameter");
    return;
  }
  
  if (speed == "high") {
    setFanSpeed(FUJITSU_AC_FAN_HIGH);
  } else if (speed == "med") {
    setFanSpeed(FUJITSU_AC_FAN_MED);
  } else if (speed == "low") {
    setFanSpeed(FUJITSU_AC_FAN_LOW);
  } else if (speed == "auto") {
    setFanSpeed(FUJITSU_AC_FAN_AUTO);
  } else if (speed == "quiet") {
    setFanSpeed(FUJITSU_AC_FAN_QUIET);
  } else {
    jsonError(422, "invalid fan speed: " + speed); 
  }
}

void handleGetStatus() {
  sendACStatus();
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

void handleTurnOff() {
  sendAC(FUJITSU_AC_CMD_TURN_OFF);
}


