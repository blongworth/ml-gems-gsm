

/**************************************************************
  Provides network communication for serial data transfer 
  from GEMS teensy. Mimics previous ESP setup

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 **************************************************************/

#include <Arduino.h>
#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// Credentials and sites
#include <setup.h>

// Increase RX buffer if needed
//#define TINY_GSM_RX_BUFFER 512

// Pins for LTE
#define LTE_RESET_PIN 6
#define LTE_PWRKEY_PIN 5
#define LTE_FLIGHT_PIN 7

// Debug console (to the Serial Monitor, default speed 115200)
#define SerialMon SerialUSB

// GSM Serial
#define SerialAT Serial1

// Define NTP Client to get time
// NTP won't work because we need UDP (not provided by tinyGSM)

// Server details
// const int  port = 443; // port 443 is default for HTTPS
const int  port = 80; // port 80 is default for HTTP

TinyGsm modem(SerialAT);

// TinyGsmClientSecure client(modem); // no ssl (yet) for SIM7600
TinyGsmClient client(modem);
HttpClient http(client, SERVER, port);

// serial client stuff
bool newData = false;
const int RCV_CHARS = 1000;
char receivedChars[RCV_CHARS];

void connect_cellular();
void disconnect_networks();
void rcvSerial();
void handleSerial();

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);

  // Set GSM module baud rate
  SerialAT.begin(115200);
  delay(3000);

  SerialMon.println("Powering on modem...");

  pinMode(LTE_RESET_PIN, OUTPUT);
  digitalWrite(LTE_RESET_PIN, LOW);
  pinMode(LTE_PWRKEY_PIN, OUTPUT);
  digitalWrite(LTE_RESET_PIN, LOW);
  delay(100);
  digitalWrite(LTE_PWRKEY_PIN, HIGH);
  delay(2000);
  digitalWrite(LTE_PWRKEY_PIN, LOW);
  
  pinMode(LTE_FLIGHT_PIN, OUTPUT);
  digitalWrite(LTE_FLIGHT_PIN, LOW);//Normal Mode
  
  delay(5000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem: ");
  SerialMon.println(modemInfo);

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    while (true);
  }
  SerialMon.println(" OK");

  SerialMon.print("Connecting to ");
  SerialMon.print(APN);
  if (!modem.gprsConnect(APN, "", "")) {
    SerialMon.println(" fail");
    while (true);
  }
  SerialMon.println(" OK");

  SerialMon.println("Ready for commands");

}

void loop(){
    rcvSerial();
    handleSerial();
}

void rcvSerial() {
  static bool recvInProgress = false;
  static int ndx = 0;
  const char START_MARKER = '<';
  const char END_MARKER = '>';

  while (SerialMon.available() > 0 && !newData) {
    char rc = SerialMon.read();

    // Check for start marker if not already collecting
    if (!recvInProgress) {
      if (rc == START_MARKER) {
        recvInProgress = true;
        ndx = 0;
      }
      continue;  // Skip rest of loop until start marker found
    }

    // Add character to buffer if not end marker
    if (rc != END_MARKER) {
      if (ndx < RCV_CHARS - 1) {  // Leave space for null terminator
        receivedChars[ndx] = rc;
        ndx++;
      }
    } else {
      // End marker found, terminate string
      receivedChars[ndx] = '\0';
      recvInProgress = false;
      newData = true;
    }
  }
}

void handleTimeRequest() {
//   timeClient.begin();
//   timeClient.update();
//   if (timeClient.isTimeSet())
//   {
//     SerialMon.print("T");
//     SerialMon.println(timeClient.getEpochTime());
//   }
//   else
//   {
    SerialMon.println("0");
//   }
//   timeClient.end();
}

void handleCommandCheck() {
  http.get(GET_PATH); // Make the request
  int statusCode = http.responseStatusCode();
  if (statusCode > 0) {
    String payload = http.responseBody();
    // SerialMon.println(httpCode);
    // SerialMon.println(payload);
    // TODO: change codes to make 0 error
    if (payload == "Start") {
      SerialMon.write('1');
    } else if (payload == "Stop") {
      SerialMon.write('0');
    } else {
      SerialMon.write('2');
    }
  } else {
    // SerialMon.println("Error on HTTP request");
    SerialMon.write('2');
  }
}

void handleDataPacket() {
  const char contentType[] = "text/plain";
  http.post(POST_PATH, contentType, receivedChars);
  int statusCode = http.responseStatusCode();
  // http.getString();
  if (statusCode == 200) {
    SerialMon.write('a');
  } else {
    SerialMon.write('0');
  }
}

void handleSerial() {
  // if no new command from client, return
  if (!newData) return;
  // if not connected, reply 0 in all cases
  if (!modem.isGprsConnected()) {
    SerialMon.write('0');
    newData = false;
    return;
  }

  switch (receivedChars[0]) {
    case '^': // connected? Yes, we tested above.
      SerialMon.write('1');
      break;
    case '$':
      handleTimeRequest();
      break;
    case '?':
      handleCommandCheck();
      break;
    // add case for GPS position
    default:
      handleDataPacket();
      break;
  }

  newData = false;
}

void connect_cellular(){
  SerialMon.print(F("Waiting for network..."));
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" OK");

  SerialMon.print(F("Connecting to "));
  SerialMon.print(APN);
  if (!modem.gprsConnect(APN, "", "")) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" OK");
}

void disconnect_networks(){
  SerialMon.println("Disconnecting");
  http.stop();
  SerialMon.println(F("Server disconnected"));

  modem.gprsDisconnect();
  SerialMon.println(F("GPRS disconnected"));
}


