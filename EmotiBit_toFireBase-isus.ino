#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "EmotiBit.h"
#include <FirebaseESP32.h>
#include <TimeLib.h>
#include <math.h>

#define SerialUSB SERIAL_PORT_USBVIRTUAL
const uint32_t SERIAL_BAUD = 115200;

// ===================================================
// Sensor data arrays
// ===================================================

// PPG Green
const size_t numSamplesPPG_GREEN = 100;
float dataPPG_GREEN[numSamplesPPG_GREEN];
size_t countPPG_GREEN = 0;

// PPG Red
const size_t numSamplesPPG_RED = 100;
float dataPPG_RED[numSamplesPPG_RED];
size_t countPPG_RED = 0;

// PPG Infrared
const size_t numSamplesPPG_IR = 100;
float dataPPG_IR[numSamplesPPG_IR];
size_t countPPG_IR = 0;

// EDA
const size_t numSamplesEDA = 15;
float dataEDA[numSamplesEDA];
size_t countEDA = 0;

// Temperature (ST)
const size_t numSamplesTemp = 8;
float dataTemp[numSamplesTemp];
size_t countTemp = 0;

// ===================================================
// Timing intervals
// ===================================================
unsigned long lastTime_1s = 0;
const unsigned long interval_1s = 1000;

unsigned long lastTime_1min = 0;
const unsigned long interval_1min = 60000;

unsigned long lastTime_5min = 0;
const unsigned long interval_5min = 300000;

// ===================================================
// WiFi & Firebase settings
// ===================================================
const char* ssid = "IoTstudio_148";
const char* password = "IoT@admin";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

const char* FIREBASE_HOST = "https://inewgenweb-default-rtdb.asia-southeast1.firebasedatabase.app/";
const char* FIREBASE_AUTH = "euw2n2EBYABgGk9S0K7oT95nKrswcpN6Txs3Ghor";

EmotiBit emotibit;

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("Starting setup...");

  connectToWiFi();

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  connectToFirebase();

  String inoFilename = __FILE__;
  inoFilename.replace("/", "\\");
  if (inoFilename.lastIndexOf("\\") != -1) {
    inoFilename = inoFilename.substring((inoFilename.lastIndexOf("\\") + 1), inoFilename.indexOf("."));
  }
  emotibit.setup(inoFilename);
}

void loop() {
  timeClient.update();
  setTime(timeClient.getEpochTime());
  emotibit.update();

  readSensorData();

  unsigned long currentTime = millis();

  if (currentTime - lastTime_1s >= interval_1s) {
    lastTime_1s = currentTime;
    sendToFirebase("1s");
  }

  if (currentTime - lastTime_1min >= interval_1min) {
    lastTime_1min = currentTime;
    sendToFirebase("1min");
  }

  if (currentTime - lastTime_5min >= interval_5min) {
    lastTime_5min = currentTime;
    sendToFirebase("5min");
  }
}

void readSensorData() {
  // PPG GREEN
  size_t availGREEN = emotibit.readData(EmotiBit::DataType::PPG_GREEN, &dataPPG_GREEN[countPPG_GREEN], numSamplesPPG_GREEN - countPPG_GREEN);
  countPPG_GREEN += availGREEN;
  if (countPPG_GREEN >= numSamplesPPG_GREEN) {
    Serial.println("PPG GREEN data collected");
    countPPG_GREEN = 0;
  }

  // PPG RED
  size_t availRED = emotibit.readData(EmotiBit::DataType::PPG_RED, &dataPPG_RED[countPPG_RED], numSamplesPPG_RED - countPPG_RED);
  countPPG_RED += availRED;
  if (countPPG_RED >= numSamplesPPG_RED) {
    Serial.println("PPG RED data collected");
    countPPG_RED = 0;
  }

  // PPG Infrared
  size_t availIR = emotibit.readData(EmotiBit::DataType::PPG_INFRARED, &dataPPG_IR[countPPG_IR], numSamplesPPG_IR - countPPG_IR);
  countPPG_IR += availIR;
  if (countPPG_IR >= numSamplesPPG_IR) {
    Serial.println("PPG IR data collected");
    countPPG_IR = 0;
  }

  // EDA
  size_t availEDA = emotibit.readData(EmotiBit::DataType::EDA, &dataEDA[countEDA], numSamplesEDA - countEDA);
  countEDA += availEDA;
  if (countEDA >= numSamplesEDA) {
    Serial.println("EDA data collected");
    countEDA = 0;
  }

  // Temperature (ST)
  size_t availTemp = emotibit.readData(EmotiBit::DataType::THERMOPILE, &dataTemp[countTemp], numSamplesTemp - countTemp);
  countTemp += availTemp;
  if (countTemp >= numSamplesTemp) {
    Serial.println("Temperature data collected");
    countTemp = 0;
  }
}

void sendToFirebase(const String& intervalType) {
  if (Firebase.ready()) {
    FirebaseJson json;

    // Send PPG data as arrays
    FirebaseJsonArray ppgRedArray;
    FirebaseJsonArray ppgGreenArray;
    FirebaseJsonArray ppgIrArray;

    for (size_t i = 0; i < numSamplesPPG_RED; i++) {
      ppgRedArray.add(dataPPG_RED[i]);
      ppgGreenArray.add(dataPPG_GREEN[i]);
      ppgIrArray.add(dataPPG_IR[i]);
    }

    json.set("PPG_RED", ppgRedArray);
    json.set("PPG_GREEN", ppgGreenArray);
    json.set("PPG_IR", ppgIrArray);

    // Send EDA data as array
    FirebaseJsonArray edaArray;
    for (size_t i = 0; i < numSamplesEDA; i++) {
      edaArray.add(dataEDA[i]);
    }
    json.set("EDA", edaArray);

    // Send Temperature data as array
    FirebaseJsonArray tempArray;
    for (size_t i = 0; i < numSamplesTemp; i++) {
      tempArray.add(dataTemp[i]);
    }
    json.set("ST", tempArray);

    // Create path for sending data
    String path = "Device/Inpatient/MD-V5-0000205/" + intervalType + "/";

    if (Firebase.setJSON(firebaseData, path, json)) {
      Serial.println("Data sent successfully to: " + path);
    } else {
      Serial.println("Failed to send data: " + firebaseData.errorReason());
    }
  } else {
    Serial.println("Firebase not ready, retrying...");
    delay(1000);
  }
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

void connectToFirebase() {
  while (!Firebase.ready()) {
    Serial.println("Waiting for Firebase to be ready...");
    delay(1000);
  }
  Serial.println("Firebase is ready!");
}