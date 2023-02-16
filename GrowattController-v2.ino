/**
   Pins
   ====
   Vin - 5V
   GND - GND
   D1  - MAX485_DE               // D1 (5), DE pin on the TTL to RS485 converter
   D2  - MAX485_RE_NEG           // D2 (4), RE pin on the TTL to RS485 converter
   D5  - MAX485_RX               // D5 (14), RO pin on the TTL to RS485 converter
   D6  - MAX485_TX               // D6 (12), DI pin on the TTL to RS485 converter
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

/* Configuration Parameters */
// WIFI

#ifndef APSSID
#define APSSID "Growatt-Timer"
#define APPSK  "12345678"
#endif

const char *ssid = APSSID;
const char *password = APPSK;

#define MODBUS_RATE     9600      // Modbus speed of Growatt, do not change
#define SERIAL_RATE     115200    // Serial speed for status info
#define MAX485_DE       5         // D1, DE pin on the TTL to RS485 converter
#define MAX485_RE_NEG   4         // D2, RE pin on the TTL to RS485 converter
#define MAX485_RX       14        // D5, RO pin on the TTL to RS485 converter
#define MAX485_TX       12        // D6, DI pin on the TTL to RS485 converter
#define SLAVE_ID        1         // Default slave ID of Growatt
#define STATUS_LED      LED_BUILTIN   // Status LED
#define BUZZER          13 

/* End of Configuration Parameters */

// Components variables
SoftwareSerial inverterSerial(MAX485_RX, MAX485_TX, false); // (RX, TX) connect to RX, TX of RS485 to TTL Converter
ModbusMaster growattModbus;

// Webserver configurations
ESP8266WebServer server(80);

// Websocket server configuration
WebSocketsServer webSocket = WebSocketsServer(81);

enum OutputMode {
  SBU,
  SOL,
  UTI,
  SUB
};
OutputMode outputMode;
int sysHour, sysMin;
int counter = 0, commandCounter = 0;

// Load stored timer values
int sub1Hour = -1, sub1Min = -1, sub2Hour = - 1, sub2Min = - 1, sbu1Hour = -1, sbu1Min = - 1, sbu2Hour = -1, sbu2Min = -1;

// Webpage content
String webPage = "<html> <style> table { width: 469px; border: 0; border-spacing: 1px; } td.param { background-color: #adf1bb; color: rgba(34, 131, 34, 0.623); width: 228px; height: 46px; border: 0pt; } td.val, .inputField, .readOnlyField { background-color: #d7f0dc81; color: rgba(34, 131, 34, 0.623); text-align: center } .inputField { width: 50px; height: 40px; border: 0pt; } .inputFieldSubText { width: 20px; height: 40px; border: 0pt; } .readOnlyField { width: 228px; height: 46px; border: 0pt; } td.buttonApply { text-align: right; } td.buttonSet { background-color: #d7f0dc81; color: rgba(34, 131, 34, 0.623); text-align: right; } #sbuButton { margin-right: 10px; width: 100px; } #subButton { margin-right: 10px; width: 100px; } tr { height: 46px; } table.tableStyle { margin-left: 5%; } h4 { color: rgba(22, 87, 22, 0.623); margin: 20px; font-size: 1.3em; } h5 { color: rgba(22, 87, 22, 0.623); margin: 30px; font-size: 1.0em; } body { background: rgb(255, 255, 255); margin: 10px; } h1 { font-family: 'Inconsolata'; font-variant: small-caps; text-align: center; color: #bbb; font-weight: normal; font-size: 2.5em; } </style> <script>var Socket; function start() { document.getElementById(\"systemMode\").value = \"Connecting...\"; Socket = new WebSocket(\"ws://\" + window.location.hostname + \":81/\"); Socket.onmessage = function (event) { document.getElementById(\"systemMode\").value = \"Online\"; var eventDataArr = event.data.split(\"_\"); if (eventDataArr[0] == \"STATUS\") { document.getElementById(\"systemMode\").value = eventDataArr[1]; document.getElementById(\"systemTime\").value = eventDataArr[2]; } else if (eventDataArr[0] == \"CONFIG\") { document.getElementById(\"subTimer1Hours\").value = eventDataArr[1]; document.getElementById(\"subTimer1Mins\").value = eventDataArr[2]; document.getElementById(\"subTimer2Hours\").value = eventDataArr[3]; document.getElementById(\"subTimer2Mins\").value = eventDataArr[4]; document.getElementById(\"sbuTimer1Hours\").value = eventDataArr[5]; document.getElementById(\"sbuTimer1Mins\").value = eventDataArr[6]; document.getElementById(\"sbuTimer2Hours\").value = eventDataArr[7]; document.getElementById(\"sbuTimer2Mins\").value = eventDataArr[8]; } }; } function sub() { Socket.send(\"CMD:SUB\"); } function sbu() { Socket.send(\"CMD:SBU\"); } function updateSUBConfig() { var subTimer1Hours = parseInt(document.getElementById(\"subTimer1Hours\").value); var subTimer1Mins = parseInt(document.getElementById(\"subTimer1Mins\").value); var subTimer2Hours = parseInt(document.getElementById(\"subTimer2Hours\").value); var subTimer2Mins = parseInt(document.getElementById(\"subTimer2Mins\").value); if (subTimer1Hours > 23 || subTimer1Hours < 0) { window.alert(\"SUB Timer 1 Hours should be between 0 and 23\"); return; } if (subTimer1Mins > 59 || subTimer1Mins < 0) { window.alert(\"SUB Timer 1 Mins should be between 0 and 59\"); return; } if (subTimer2Hours > 23 || subTimer2Hours < 0) { window.alert(\"SUB Timer 2 Hours should be between 0 and 23\"); return; } if (subTimer2Mins > 59 || subTimer2Mins < 0) { window.alert(\"SUB Timer 2 Mins should be between 0 and 59\"); return; } var configData = \"SUB:\" + subTimer1Hours.toString() + \":\" + subTimer1Mins.toString() + \":\" + subTimer2Hours.toString() + \":\" + subTimer2Mins.toString() ; Socket.send(configData); window.alert(\"Configuration Updated..!\"); } function updateSBUConfig() { var sbuTimer1Hours = parseInt(document.getElementById(\"sbuTimer1Hours\").value); var sbuTimer1Mins = parseInt(document.getElementById(\"sbuTimer1Mins\").value); var sbuTimer2Hours = parseInt(document.getElementById(\"sbuTimer2Hours\").value); var sbuTimer2Mins = parseInt(document.getElementById(\"sbuTimer2Mins\").value); if (sbuTimer1Hours > 23 || sbuTimer1Hours < 0) { window.alert(\"SBU Timer 1 Hours should be between 0 and 23\"); return; } if (sbuTimer1Mins > 59 || sbuTimer1Mins < 0) { window.alert(\"SBU Timer 1 Mins should be between 0 and 59\"); return; } if (sbuTimer2Hours > 23 || sbuTimer2Hours < 0) { window.alert(\"SBU Timer 2 Hours should be between 0 and 23\"); return; } if (sbuTimer2Mins > 59 || sbuTimer2Mins < 0) { window.alert(\"SBU Timer 2 Mins should be between 0 and 59\"); return; } var configData = \"SBU:\" + sbuTimer1Hours.toString() + \":\" + sbuTimer1Mins.toString() + \":\" + sbuTimer2Hours.toString() + \":\" + sbuTimer2Mins.toString() ; Socket.send(configData); window.alert(\"Configuration Updated..!\"); }</script> <body onload=\"javascript:start();\"> <h1><img src=\"data:image/jpeg;base64, /9j/4AAQSkZJRgABAQAASABIAAD/4QCeRXhpZgAATU0AKgAAAAgABQESAAMAAAABAAEAAAEaAAUAAAABAAAASgEbAAUAAAABAAAAUgExAAIAAAARAAAAWodpAAQAAAABAAAAbAAAAAAAAABIAAAAAQAAAEgAAAABQWRvYmUgSW1hZ2VSZWFkeQAAAAOgAQADAAAAAQABAACgAgAEAAAAAQAAAamgAwAEAAAAAQAAAHYAAAAA/+EJbWh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8APD94cGFja2V0IGJlZ2luPSLvu78iIGlkPSJXNU0wTXBDZWhpSHpyZVN6TlRjemtjOWQiPz4gPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0az0iWE1QIENvcmUgNi4wLjAiPiA8cmRmOlJERiB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5bnRheC1ucyMiPiA8cmRmOkRlc2NyaXB0aW9uIHJkZjphYm91dD0iIiB4bWxuczp4bXA9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8iIHhtcDpDcmVhdG9yVG9vbD0iQWRvYmUgSW1hZ2VSZWFkeSIvPiA8L3JkZjpSREY+IDwveDp4bXBtZXRhPiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIDw/eHBhY2tldCBlbmQ9InciPz4A/8AAEQgAdgGpAwEiAAIRAQMRAf/EAB8AAAEFAQEBAQEBAAAAAAAAAAABAgMEBQYHCAkKC//EALUQAAIBAwMCBAMFBQQEAAABfQECAwAEEQUSITFBBhNRYQcicRQygZGhCCNCscEVUtHw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\">&nbsp;Timer</h1> <p>You can use this console to view the current status and set the configuration parameters of the Growatt Timer system.</p> <p>&nbsp;</p> <h4>System Status</h4> <table class=\"tableStyle\"> <tbody> <tr> <td class=\"param\">&nbsp;System Mode</td> <td class=\"val\"><input class=\"readOnlyField\" type=\"text\" id=\"systemMode\" readonly></td> </tr> <tr> <td class=\"param\">&nbsp;System Time</td> <td class=\"val\"><input class=\"readOnlyField\" type=\"text\" id=\"systemTime\" readonly></td> </tr> </tbody> </table> <p>&nbsp;</p> <div id=\"setMode\"> <h4>Set System Mode</h4> <table class=\"tableStyle\"> <tbody> <tr> <td class=\"param\"> <p>&nbsp;SUB (Solar -> Utility -> Battery)</p> </td> <td class=\"buttonSet\"> <button id=\"subButton\" type=\"button\" onclick=\"sub();\">Set</button> </td> </tr> <tr> <td class=\"param\"> <p>&nbsp;SBU (Solar -> Battery -> Utility)</p> </td> <td class=\"buttonSet\"> <button id=\"sbuButton\" type=\"button\" onclick=\"sbu();\">Set</button> </td> </tr> </tbody> </table> <p>&nbsp;</p> <h4>Timer Configurations</h4> <h5>SUB (Solar -> Utility -> Battery)</h5> <table class=\"tableStyle\"> <tbody> <tr> <td class=\"param\"> <p>&nbsp;Timer 1</p> </td> <td class=\"val\"> <input class=\"inputField\" type=\"text\" id=\"subTimer1Hours\"> <span class=\"inputFieldSubText\">Hours</span> <input class=\"inputField\" type=\"text\" id=\"subTimer1Mins\"> <span class=\"inputFieldSubText\">Mins</span> </td> </tr> <tr> <td class=\"param\"> <p>&nbsp;Timer 2</p> </td> <td class=\"val\"> <input class=\"inputField\" type=\"text\" id=\"subTimer2Hours\"> <span class=\"inputFieldSubText\">Hours</span> <input class=\"inputField\" type=\"text\" id=\"subTimer2Mins\"> <span class=\"inputFieldSubText\">Mins</span> </td> </tr> <tr> <td> </td> <td class=\"buttonApply\"><button id=\"applyButton\" type=\"button\" onclick=\"updateSUBConfig();\">Apply</button> </td> </tr> </tbody> </table> <h5>SBU (Solar -> Battery -> Utility)</h5> <table class=\"tableStyle\"> <tbody> <tr> <td class=\"param\"> <p>&nbsp;Timer 1</p> </td> <td class=\"val\"> <input class=\"inputField\" type=\"text\" id=\"sbuTimer1Hours\"> <span class=\"inputFieldSubText\">Hours</span> <input class=\"inputField\" type=\"text\" id=\"sbuTimer1Mins\"> <span class=\"inputFieldSubText\">Mins</span> </td> </tr> <tr> <td class=\"param\"> <p>&nbsp;Timer 2</p> </td> <td class=\"val\"> <input class=\"inputField\" type=\"text\" id=\"sbuTimer2Hours\"> <span class=\"inputFieldSubText\">Hours</span> <input class=\"inputField\" type=\"text\" id=\"sbuTimer2Mins\"> <span class=\"inputFieldSubText\">Mins</span> </td> </tr> <tr> <td> </td> <td class=\"buttonApply\"><button id=\"applyButton\" type=\"button\" onclick=\"updateSBUConfig();\">Apply</button> </td> </tr> </tbody> </table> </div> </body> </html>";
boolean webSocketConnected = false;
boolean modbusConnected = false;

boolean ledOn = false;

const long WEBSOCKET_INTERVAL = 5000;
const long LED_INTERVAL = 1000;
const long MODBUS_INTERVAL = 30000;

long lastBroadcastedTime = millis();
long ledOnTime = millis();
long lastModbusCheckedTime = millis();

void setup() {

  Serial.begin(SERIAL_RATE);
  inverterSerial.begin(MODBUS_RATE);

  // Wifi
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.println("AP IP address: ");
  Serial.println(myIP);

  //EEPRom
  EEPROM.begin(512);

  // Setup Pins
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH);
  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH);

  // Set up the Modbus
  setupModbus();
  initialize();
  
  // Webpage
  server.on("/", []() {
    server.send(200, "text/html", webPage);
  });

  // Webserver initialization
  server.begin();

  // Websocket server initialization
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  beepOnce();
  Serial.println("Started..!");
}

void setupModbus() {
  growattModbus.begin(SLAVE_ID , inverterSerial);
  growattModbus.preTransmission(preTransmission);
  growattModbus.postTransmission(postTransmission);
}

void preTransmission() {
  digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}

void loop() {
  webSocket.loop();
  server.handleClient();

  long currentTime = millis();

  if ((lastModbusCheckedTime + MODBUS_INTERVAL) < currentTime) {
     lastModbusCheckedTime = currentTime;
     runTimerCheck(); 
  }

  if (webSocketConnected) {
    if ((lastBroadcastedTime + WEBSOCKET_INTERVAL) < currentTime) {
      lastBroadcastedTime = currentTime;
      String message = "";
      if (modbusConnected) {
        String outputModeStr = "";
        switch (outputMode) {
          case 0: {
              outputModeStr = "SBU";
              break;
            }
          case 1: {
              outputModeStr = "SOL";
              break;
            }
          case 2: {
              outputModeStr = "UTI";
              break;
            }
          case 3: {
              outputModeStr = "SUB";
              break;
            }
        }
        message = "STATUS_" + outputModeStr + "_" + String(sysHour) + ":" + String(sysMin);
      } else {
        message =  "STATUS_Modbus Error ";
      }
      int messageSize = message.length() + 1;
      char broadcastMessage[messageSize];
      message.toCharArray(broadcastMessage, messageSize);
      webSocket.broadcastTXT(broadcastMessage);
    }
  }

  if (ledOnTime + LED_INTERVAL < currentTime) {
    ledOnTime = currentTime;
    if (modbusConnected) {
      if (ledOn) {
        digitalWrite(STATUS_LED, HIGH);
        ledOn = false;
      } else {
        digitalWrite(STATUS_LED, LOW);
        ledOn = true;
      }
    } else {
      fetchModbusData();
      beepOnce();     // continuos beep for errors
    }
  }
}

void runTimerCheck() {
  fetchModbusData();
  if (!modbusConnected) {
    return;
  }

  if (sysHour == sub1Hour && sysMin == sub1Min) {
    setSUB();
  }
  if (sysHour == sub2Hour && sysMin == sub2Min) {
    setSUB();
  }
  if (sysHour == sbu1Hour && sysMin == sbu1Min)  {
    setSBU();
  }
  if (sysHour == sbu2Hour && sysMin == sbu2Min) {
    setSBU();
  }
}

void beepOnce() {
  digitalWrite(BUZZER, LOW);
  delay(250);
  digitalWrite(BUZZER, HIGH);
}

void beepTwice() {
  digitalWrite(BUZZER, LOW);
  delay(250);
  digitalWrite(BUZZER, HIGH);
  delay(250);
  digitalWrite(BUZZER, LOW);
  delay(250);
  digitalWrite(BUZZER, HIGH);
}

void beepLong() {
  digitalWrite(BUZZER, LOW);
  delay(1000);
  digitalWrite(BUZZER, HIGH);
}

void initialize() {
  loadTimerValues();
  fetchModbusData();
}

void setSBU() {
  Serial.println("Setting SBU");
  beepLong();
  growattModbus.writeSingleRegister(1, 0);
  outputMode = SBU;
}

void setSOL() {
  beepLong();
  growattModbus.writeSingleRegister(1, 1);
  outputMode = SOL;
}

void setUTI() {
  beepLong();
  growattModbus.writeSingleRegister(1, 2);
  outputMode = UTI;
}

void setSUB() {
  Serial.println("Setting SUB");
  beepLong();
  growattModbus.writeSingleRegister(1, 3);
  outputMode = SUB;
}

void fetchModbusData() {

  ESP.wdtFeed();
  uint8_t result = growattModbus.readHoldingRegisters(counter * 64, 64);
  if (result == growattModbus.ku8MBSuccess) {
    digitalWrite(STATUS_LED, HIGH);
    modbusConnected = true;
    outputMode = (OutputMode) growattModbus.getResponseBuffer(1);
    sysHour = growattModbus.getResponseBuffer(48);
    sysMin = growattModbus.getResponseBuffer(49);
  } else {
    modbusConnected = false;
    digitalWrite(STATUS_LED, LOW);
    Serial.println("Error connecting to Modbus");
  }
  
}

void loadTimerValues() {
  sub1Hour = EEPROM.read(0);
  sub1Min = EEPROM.read(1);
  sub2Hour = EEPROM.read(2);
  sub2Min = EEPROM.read(3);
  sbu1Hour = EEPROM.read(4);
  sbu1Min  = EEPROM.read(5);
  sbu2Hour = EEPROM.read(6);
  sbu2Min  = EEPROM.read(7);
}

void storeSUBValues() {
  beepTwice();
  EEPROM.write(0, sub1Hour);
  EEPROM.write(1, sub1Min);
  EEPROM.write(2, sub2Hour);
  EEPROM.write(3, sub2Min);
  EEPROM.commit();
}

void storeSBUValues() {
  beepTwice();
  EEPROM.write(4, sbu1Hour);
  EEPROM.write(5, sbu1Min);
  EEPROM.write(6, sbu2Hour);
  EEPROM.write(7, sbu2Min);
  EEPROM.commit();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  String eventPayload;
  int separatorPosition;
  String tmpConfig;
  String tmpSubConfig;
  int command;

  switch (type) {
    case WStype_TEXT:
      {
        eventPayload = (const char *) payload;
        if (eventPayload.startsWith("CMD")) {
          separatorPosition = eventPayload.indexOf(':');
          String command = eventPayload.substring(separatorPosition + 1);
          if (command == "SUB") {
            setSUB();
          } else if (command == "SBU") {
            setSBU();
          }
        } else if (eventPayload.startsWith("SUB")) {
          separatorPosition = eventPayload.indexOf(':');
          tmpConfig = eventPayload.substring(separatorPosition + 1);

          separatorPosition = tmpConfig.indexOf(':');
          tmpSubConfig = tmpConfig.substring(0, separatorPosition);
          tmpSubConfig.trim();
          if (tmpSubConfig == "" || tmpSubConfig == "NaN") {
            sub1Hour = -1;
          } else {
            sub1Hour = tmpSubConfig.toInt();
          }

          tmpConfig = tmpConfig.substring(separatorPosition + 1);
          separatorPosition = tmpConfig.indexOf(':');
          tmpSubConfig = tmpConfig.substring(0, separatorPosition);
          tmpSubConfig.trim();
          if (tmpSubConfig == "" || tmpSubConfig == "NaN") {
            sub1Min = -1;
          } else {
            sub1Min = tmpSubConfig.toInt();
          }

          tmpConfig = tmpConfig.substring(separatorPosition + 1);
          separatorPosition = tmpConfig.indexOf(':');
          tmpSubConfig = tmpConfig.substring(0, separatorPosition);
          tmpSubConfig.trim();
          if (tmpSubConfig == "" || tmpSubConfig == "NaN") {
            sub2Hour = -1;
          } else {
            sub2Hour = tmpSubConfig.toInt();
          }

          tmpConfig = tmpConfig.substring(separatorPosition + 1);
          separatorPosition = tmpConfig.indexOf(':');
          tmpSubConfig = tmpConfig.substring(0, separatorPosition);
          tmpSubConfig.trim();
          if (tmpSubConfig == "" || tmpSubConfig == "NaN") {
            sub2Min = -1;
          } else {
            sub2Min = tmpSubConfig.toInt();
          }
          storeSUBValues();
        } else if (eventPayload.startsWith("SBU")) {
          separatorPosition = eventPayload.indexOf(':');
          tmpConfig = eventPayload.substring(separatorPosition + 1);

          separatorPosition = tmpConfig.indexOf(':');
          tmpSubConfig = tmpConfig.substring(0, separatorPosition);
          tmpSubConfig.trim();
          if (tmpSubConfig == "" || tmpSubConfig == "NaN") {
            sbu1Hour = -1;
          } else {
            sbu1Hour = tmpSubConfig.toInt();
          }

          tmpConfig = tmpConfig.substring(separatorPosition + 1);
          separatorPosition = tmpConfig.indexOf(':');
          tmpSubConfig = tmpConfig.substring(0, separatorPosition);
          tmpSubConfig.trim();
          if (tmpSubConfig == "" || tmpSubConfig == "NaN") {
            sbu1Min = -1;
          } else {
            sbu1Min = tmpSubConfig.toInt();
          }

          tmpConfig = tmpConfig.substring(separatorPosition + 1);
          separatorPosition = tmpConfig.indexOf(':');
          tmpSubConfig = tmpConfig.substring(0, separatorPosition);
          tmpSubConfig.trim();
          if (tmpSubConfig == "" || tmpSubConfig == "NaN") {
            sbu2Hour = -1;
          } else {
            sbu2Hour = tmpSubConfig.toInt();
          }

          tmpConfig = tmpConfig.substring(separatorPosition + 1);
          separatorPosition = tmpConfig.indexOf(':');
          tmpSubConfig = tmpConfig.substring(0, separatorPosition);
          tmpSubConfig.trim();
          if (tmpSubConfig == "" || tmpSubConfig == "NaN") {
            sbu2Min = -1;
          } else {
            sbu2Min = tmpSubConfig.toInt();
          }
          storeSBUValues();
        }
        break;
      }
    case WStype_DISCONNECTED:
      {
        beepOnce();
        webSocketConnected = false;
        Serial.printf("[%u] Disconnected!\n", num);
        break;
      }
    case WStype_CONNECTED:
      {
        beepOnce();
        webSocketConnected = true;
        // Send initial configuration message

        String message =  "CONFIG_"
                          + (((sub1Hour >= 0 && sub1Hour <= 23)) ? String(sub1Hour) : "")
                          + "_" + (((sub1Min >= 0 && sub1Min <= 59)) ? String(sub1Min) : "")
                          + "_" + (((sub2Hour >= 0 && sub2Hour <= 23)) ? String(sub2Hour) : "")
                          + "_" + (((sub2Min >= 0 && sub2Min <= 59)) ? String(sub2Min) : "")
                          + "_" + (((sbu1Hour >= 0 && sbu1Hour <= 23)) ? String(sbu1Hour) : "")
                          + "_" + (((sbu1Min >= 0 && sbu1Min <= 59)) ? String(sbu1Min) : "")
                          + "_" + (((sbu2Hour >= 0 && sbu2Hour <= 23)) ? String(sbu2Hour) : "")
                          + "_" + (((sbu2Min >= 0 && sbu2Min <= 59)) ? String(sbu2Min) : "");

        int messageSize = message.length() + 1;
        char broadcastMessage[messageSize];
        message.toCharArray(broadcastMessage, messageSize);
        webSocket.broadcastTXT(broadcastMessage);
        break;
      }
  }
}
