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
byte currentRoomTemp = 0;
byte currentRoomHumidity = 0;

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
  server.on("/",        handleIndex);
  server.on("/settemp", handleSetTemp);
  server.on("/dectemp", handleDecTemp);
  server.on("/inctemp", handleIncTemp);
  server.on("/gettemp", handleGetTemp);
  server.on("/curtemp", handleCurrTemp);
  server.on("/nextfan", handleNextFan);
  server.on("/setfan",  handleSetFan);
  server.on("/status",  handleGetStatus);
  server.on("/turnon",  handleTurnOn);
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

  currentRoomTemp = temperature;
  currentRoomHumidity = humidity;
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

void cycleFanSpeed() {
  currentFanSpeed += 1;
  if (currentFanSpeed > FUJITSU_AC_FAN_QUIET) {
    currentFanSpeed = FUJITSU_AC_FAN_AUTO;
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

String getSwing() {
  if (currentSwing == FUJITSU_AC_SWING_OFF) {
    return "off";
  } else if (currentSwing == FUJITSU_AC_SWING_VERT) {
    return "vertical";
  } else if (currentSwing == FUJITSU_AC_SWING_HORIZ) {
    return "horizontal";
  } else if (currentSwing == FUJITSU_AC_SWING_BOTH) {
    return "both";
  } else {
    return "unknown";
  }
}

String getMode() {
  if (currentMode == FUJITSU_AC_MODE_AUTO) {
    return "auto";
  } else if (currentMode == FUJITSU_AC_MODE_COOL) {
    return "cool";
  } else if (currentMode == FUJITSU_AC_MODE_DRY) {
    return "dry";
  } else if (currentMode == FUJITSU_AC_MODE_FAN) {
    return "fan";
  } else if (currentMode == FUJITSU_AC_MODE_HEAT) {
    return "heat";
  } else {
    return "unknown";
  }  
}

String getFanSpeed() {
  if (currentFanSpeed == FUJITSU_AC_FAN_AUTO) {
    return "auto";
  } else if (currentFanSpeed == FUJITSU_AC_FAN_HIGH) {
    return "high";
  } else if (currentFanSpeed == FUJITSU_AC_FAN_MED) {
    return "med";
  } else if (currentFanSpeed == FUJITSU_AC_FAN_LOW) {
    return "low";
  } else if (currentFanSpeed == FUJITSU_AC_FAN_QUIET) {
    return "quiet";
  } else {
    return "unknown";
  }  
}

void sendACStatus() {
  String data = "\"temperature\": " + String(currentTemp);
  data += ", \"swing\": \"" + getSwing() + "\"";
  data += ", \"mode\": \"" + getMode() + "\"";
  data += ", \"fanSpeed\": \"" + getFanSpeed() + "\"";
  data += ", \"swingValue\": " + String(currentSwing);
  data += ", \"modeValue\": " + String(currentMode);
  data += ", \"fanSpeedValue\": " + String(currentFanSpeed);

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

void html(String body) {
  server.send(200, "text/html", body);
  blink(200);
}

// --------------
//  HTML helpers
// --------------

String rowDiv = "    <div class=\"row\" style=\"padding-bottom:1em\">\n";
String endDiv = "    </div>\n";

String addButton(int colSize, String label, String url) {
  return  "<div class=\"col-xs-" + String(colSize) + "\" style=\"text-align: center\">\n" +
          "    <button id=\"" + url + "\" type=\"button\" class=\"btn btn-default\" style=\"width: 100%\" onclick='makeAjaxCall(\"" + url + "\")'>" + label+ "</button>\n" +
          "</div>\n";  
}

// ----------------------
//  HTTP server handlers
// ----------------------

void handleIndex() {
  String body = "<!DOCTYPE html>\n";
  body += "<html>\n";
  body += "  <head>\n";
  body += "    <meta charset=\"utf-8\">\n";
  body += "    <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n";
  body += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  body += "    <link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">\n";
  body += "  </head>\n";
  body += "  <body style='margin: 10px;'>\n";

  body += "    <div id=\"status\" style=\"padding: 10px;\"></div>";

  body += rowDiv;
  body += addButton(6, "AC On", "turnon");
  body += addButton(6, "AC Off", "turnoff");
  body += endDiv;
  
  body += rowDiv;
  body += addButton(4, "-", "dectemp");
  body += addButton(4, "+", "inctemp");
  body += addButton(4, "Fan", "nextfan");
  body += endDiv;
  
  body += "    <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js\"></script>\n";
  body += "    <script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\"></script>\n";

  body += "<script>\n";
  body += "  function addRow(data, label, key, unit) {\n";
  body += "    if (!unit) { unit = ''; }\n";
  body += "    return '<div class=\"row\">' +\n";
  body += "      '<div class=\"col-xs-6\" style=\"text-align: right;\">' + label + ':</div>' +\n";
  body += "      '<div class=\"col-xs-6\"><b>' + data[key] + unit + '</b></div>' +\n";
  body += "      '</div>';\n";
  body += "  }\n";
  body += "  \n";
  body += "  function makeAjaxCall(url) {\n";
  body += "    var html = '';\n";
  body += "    $.getJSON('curtemp').done(function(tempData) {\n";
  body += "      html += addRow(tempData, 'Room Temp', 'temperature', '<sup>o</sup>C');\n";
  body += "      html += addRow(tempData, 'Room Humidity', 'humidity', '%');\n";
  body += "      $.getJSON(url).done(function(data) {\n";
  body += "        html += addRow(data, 'Mode', 'mode');\n";
  body += "        html += addRow(data, 'Temperature', 'temperature', '<sup>o</sup>C');\n";
  body += "        html += addRow(data, 'Swing', 'swing');\n";
  body += "        html += addRow(data, 'Fan', 'fanSpeed');\n";
  body += "        \n";
  body += "        $('#status').html(html);\n";
  body += "      });\n";
  body += "    });\n";
  body += "  }\n";
  body += "  makeAjaxCall('status');\n";
  body += "</script>\n";
    
  body += "  </body>\n";
  body += "</html>\n";

  html(body);
}

void handleCurrTemp() {
  String data = "\"temperature\":" + String(currentRoomTemp) + ",\"humidity\":" + String(currentRoomHumidity);
  jsonData(data);
}

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

void handleTurnOn() {
  sendAC(FUJITSU_AC_CMD_TURN_ON);
}

void handleTurnOff() {
  sendAC(FUJITSU_AC_CMD_TURN_OFF);
}

void handleNextFan() {
  cycleFanSpeed();
}


