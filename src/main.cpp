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
//
// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialUSB

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <time.h>

// Credentials and sites
#include <setup.h>

// Debug console (to the Serial Monitor, default speed 115200)
#define SerialMon SerialUSB

// GSM Serial
#define SerialAT Serial1

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

// Increase RX buffer if needed
//#define TINY_GSM_RX_BUFFER 512

// Pins for LTE
#define LTE_RESET_PIN 6
#define LTE_PWRKEY_PIN 5
#define LTE_FLIGHT_PIN 7

// Server details
// const int  port = 443; // port 443 is default for HTTPS
const int  port = 80; // port 80 is default for HTTP

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

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

  // Set GSM module baud rate
  SerialAT.begin(115200);

  DBG("Powering on modem...");

  pinMode(LTE_RESET_PIN, OUTPUT);
  pinMode(LTE_PWRKEY_PIN, OUTPUT);
  pinMode(LTE_FLIGHT_PIN, OUTPUT);

  digitalWrite(LTE_RESET_PIN, LOW);
  digitalWrite(LTE_FLIGHT_PIN, LOW);//Normal Mode
  delay(100);
  digitalWrite(LTE_PWRKEY_PIN, HIGH);
  delay(500);
  digitalWrite(LTE_PWRKEY_PIN, LOW);
  
  
  delay(5000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  DBG("Initializing modem...");
  if (!modem.init())
  {
    DBG("Failed initialization, restarting modem.");
    modem.restart();
  }

  String modemInfo = modem.getModemInfo();
  DBG("Modem: ");
  DBG(modemInfo);

  DBG("Waiting for network...");
  if (!modem.waitForNetwork()) {
    DBG(" fail");
    while (true);
  }
  DBG(" OK");

  DBG("Connecting to ");
  DBG(APN);
  if (!modem.gprsConnect(APN, "", "")) {
    DBG(" fail");
    while (true);
  }
  DBG(" OK");

  DBG("Enabling GPS/GNSS/GLONASS...");
  modem.enableGPS();
  delay(1000L);

  DBG("Ready for commands");

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

void handleCommandCheck() {
  http.get(GET_PATH); // Make the request
  int statusCode = http.responseStatusCode();
  if (statusCode > 0) {
    String payload = http.responseBody();
    DBG(statusCode);
    DBG(payload);
    // TODO: change codes to make 0 error
    if (payload == "Start") {
      SerialMon.println('1');
    } else if (payload == "Stop") {
      SerialMon.println('0');
    } else {
      SerialMon.println('2');
    }
  } else {
    DBG("Error on HTTP request");
    SerialMon.println('2');
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

void handleGPSRequest()
{
  float gps_latitude = 0;
  float gps_longitude = 0;
  float gps_speed = 0;
  float gps_altitude = 0;
  int gps_vsat = 0;
  int gps_usat = 0;
  float gps_accuracy = 0;
  int gps_year = 0;
  int gps_month = 0;
  int gps_day = 0;
  int gps_hour = 0;
  int gps_minute = 0;
  int gps_second = 0;

  DBG("Requesting current GPS/GNSS/GLONASS location");
  if (modem.getGPS(&gps_latitude, &gps_longitude, &gps_speed, &gps_altitude,
                   &gps_vsat, &gps_usat, &gps_accuracy, &gps_year, &gps_month,
                   &gps_day, &gps_hour, &gps_minute, &gps_second))
  {
    DBG("Latitude:", String(gps_latitude, 8),
        "\tLongitude:", String(gps_longitude, 8));
    DBG("Speed:", gps_speed, "\tAltitude:", gps_altitude);
    DBG("Visible Satellites:", gps_vsat, "\tUsed Satellites:", gps_usat);
    DBG("Accuracy:", gps_accuracy);
    DBG("Year:", gps_year, "\tMonth:", gps_month, "\tDay:", gps_day);
    DBG("Hour:", gps_hour, "\tMinute:", gps_minute, "\tSecond:", gps_second);
    char gpsData[50];
    snprintf(gpsData, sizeof(gpsData), "%.6f,%.6f", gps_latitude, gps_longitude);
    SerialMon.println(gpsData);
  }
  else
  {
    DBG("Couldn't get GPS/GNSS/GLONASS location.");
    SerialMon.println('0');
  }
}

void handleTimeRequest()
{
  DBG("Asking modem to sync with NTP");
  modem.NTPServerSync("pool.ntp.org");
  int ntp_year = 0;
  int ntp_month = 0;
  int ntp_day = 0;
  int ntp_hour = 0;
  int ntp_min = 0;
  int ntp_sec = 0;
  float ntp_timezone = 0;
  DBG("Requesting current network time");
  // use getNetworkUTCTime() for time in UTC
  if (modem.getNetworkTime(&ntp_year, &ntp_month, &ntp_day, &ntp_hour,
                           &ntp_min, &ntp_sec, &ntp_timezone))
  {
    DBG("Year:", ntp_year, "\tMonth:", ntp_month, "\tDay:", ntp_day);
    DBG("Hour:", ntp_hour, "\tMinute:", ntp_min, "\tSecond:", ntp_sec);
    DBG("Timezone:", ntp_timezone);
    struct tm timeinfo = {};
    timeinfo.tm_year = ntp_year - 1900;
    timeinfo.tm_mon = ntp_month - 1;
    timeinfo.tm_mday = ntp_day;
    timeinfo.tm_hour = ntp_hour;
    timeinfo.tm_min = ntp_min;
    timeinfo.tm_sec = ntp_sec;
    time_t timestamp = mktime(&timeinfo);
    SerialMon.print("T");
    SerialMon.println(timestamp);
  }
  else
  {
    DBG("Couldn't get network time.");
    SerialMon.println('0');
  }
}

void handleSerial() {
  // if no new command from client, return
  if (!newData) return;
  // if not connected, reply 0 in all cases
  if (!modem.isGprsConnected()) {
    SerialMon.println('0');
    newData = false;
    return;
  }

  switch (receivedChars[0]) {
    case '^': // connected? Yes, we tested above.
      SerialMon.println('1');
      break;
    case '$':
      handleTimeRequest();
      break;
    case '?':
      handleCommandCheck();
      break;
    // add case for GPS position
    case '*':
      handleGPSRequest();
      break;
    default:
      handleDataPacket();
      break;
  }

  newData = false;
}

void connect_cellular(){
  DBG(F("Waiting for network..."));
  if (!modem.waitForNetwork()) {
    DBG(" fail");
    delay(10000);
    return;
  }
  DBG(" OK");

  DBG(F("Connecting to "));
  DBG(APN);
  if (!modem.gprsConnect(APN, "", "")) {
    DBG(" fail");
    delay(10000);
    return;
  }
  DBG(" OK");
}

void disconnect_networks(){
  DBG("Disconnecting");
  http.stop();
  DBG(F("Server disconnected"));

  modem.gprsDisconnect();
  DBG(F("GPRS disconnected"));
}
