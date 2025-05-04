#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "EmotiBit.h"
#include <FirebaseESP32.h>
#include <TimeLib.h>
#include <vector>
#include <algorithm>

#define SerialUSB SERIAL_PORT_USBVIRTUAL // Required to work in Visual Micro / Visual Studio IDE
const uint32_t SERIAL_BAUD = 115200;     //115200

// Arduino Low Power: Version=1.2.2
// Arduino_Json by Arduino: Version=0.2.0
// NTPClient: Version=3.2.1
// Adafruit BusIO: Version=1.16.2
// Adafruit GFX Library: Version=1.11.11
// Adafruit IS31FL3731 Library: Version=2.0.2
// AdafruitSSD1306: Version=2.5.13
// Adafruit Unified Sensor: Version=1.1.14
// ArduinoJson by Benoit Blanchon: Version=7.2.1
// AstnceTCP: Version=1.1.4
// EmotiBit ADS1X15: Version=2.2.0+EmotiBit0.0.1
// EmotiBit ArdionoFilters: Version=1.0.0
// EmotiBit BMI160: Version=0.3.3
// EmotiBit Emojib: Version= 0.0.1
// EmotiBit External EEPROM: Version=1.0.5+EmotiBit.0.0.1
// EmotiBit FeatherWing: Version=1.12.1
// EmotiBit MAX30101: Version=2.0.3
// EmotiBit NCP5623: Version=0.1.0
// EmotiBit SI7013: Version=0.0.7
// EmotiBit SimpleFTPServer: Version=2.17+EmotiBit.0.0.1
// EmotiBit XPlat Utils: Version=1.6.0
// Firebase ESP32 Client: Version=4.4.14
// SdFat by Bill Greiman: Version=2.2.0
// Time by Michael Margolis: Version=1.6.1

EmotiBit emotibit;

const size_t numSamplesPPG = 100;
const size_t numSamplesEDA = 15;
const size_t numSamplesTemp = 8;

float dataPPG[numSamplesPPG];
float dataEDA[numSamplesEDA];
float dataTemp[numSamplesTemp];

size_t countPPG = 0;
size_t countEDA = 0;
size_t countTemp = 0;

float averagePPG = 0;
float averageEDA = 0;
float averageTemp = 0;

float avgEDAList = 0;
float avgPPGList = 0;
float avgSTList = 0;

float heartRate = 0;

unsigned long lastTime_1s = 0;
const unsigned long interval_1s = 1000; // 1 second
unsigned long lastTime_1min = 0; // ตัวแปรเก็บเวลา 1 นาที
const unsigned long interval_1min = 60000; // 1 นาที (60,000 ms)
unsigned long lastTime_5min = 0; // ตัวแปรเก็บเวลา 5 นาที
const unsigned long interval_5min = 300000; // 5 นาที (300,000 ms)

const char* ssid = "IoTstudio_148";
const char* password = "IoT@admin";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

const char* FIREBASE_HOST = "https://inewgenweb-default-rtdb.asia-southeast1.firebasedatabase.app/";
const char* FIREBASE_AUTH = "euw2n2EBYABgGk9S0K7oT95nKrswcpN6Txs3Ghor";

// เพิ่ม List สำหรับเก็บค่าของ EDA, PPG, ST และ HeartRate
#define MAX_LIST_SIZE 10  // ขนาดสูงสุดของ List ที่เก็บข้อมูลล่าสุด 10 ครั้ง
float listEDA[MAX_LIST_SIZE];
float listPPG[MAX_LIST_SIZE];
float listST[MAX_LIST_SIZE];
float listHeartRate[MAX_LIST_SIZE];
float avgHeartRateList = 0;
size_t listIndex = 0;  // ตัวแปรที่ใช้ในการอัปเดต index ของ List

// ฟังก์ชันในการคำนวณค่าเฉลี่ยจาก List
float calculateMean(float list[], size_t listSize) {
  float sum = 0;
  for (size_t i = 0; i < listSize; i++) {
    sum += list[i];
  }
  return sum / listSize;
}

// เพิ่มฟังก์ชันคำนวณค่าเบี่ยงเบนมาตรฐาน
float calculateSD(float list[], size_t listSize) {
  float mean = calculateMean(list, listSize);
  float sumSquaredDiff = 0;
  
  // คำนวณผลรวมของ (x - mean)^2
  for (size_t i = 0; i < listSize; i++) {
    float diff = list[i] - mean;
    sumSquaredDiff += diff * diff;
  }
  
  // คำนวณค่า SD = sqrt((Σ(x - mean)^2) / (n-1))
  return sqrt(sumSquaredDiff / (listSize - 1));
}


// ฟังก์ชันการคำนวณ HeartRate
float calculateHeartRate(const float* dataPPG, size_t numSamplesPPG, float samplingRate) {
  std::vector<int> peaks;
  float threshold = 0.7 * *std::max_element(dataPPG, dataPPG + numSamplesPPG); // ใช้ 70% ของค่าสูงสุดใน PPG เป็น threshold
  
  // ค้นหาพีคในข้อมูล PPG
  for (size_t i = 1; i < numSamplesPPG - 1; i++) {
    if (dataPPG[i] > dataPPG[i - 1] && dataPPG[i] > dataPPG[i + 1] && dataPPG[i] > threshold) {
      peaks.push_back(i);  // เพิ่มพีคที่พบ
    }
  }
  
  // คำนวณอัตราการเต้นของหัวใจจากพีค
  float heartRate = 0;
  if (peaks.size() > 1) {
    float totalInterval = 0;
    for (size_t i = 1; i < peaks.size(); i++) {
      totalInterval += (peaks[i] - peaks[i - 1]);
    }
    float avgInterval = totalInterval / (peaks.size() - 1); // ค่าเฉลี่ยของ interval
    heartRate = (samplingRate / avgInterval) * 3; // แปลงเป็น bpm
  }
  return heartRate;
}

void connectToWiFi();
void connectToFirebase();
void readSensorData();
void sendToFirebase(const String& intervalType);

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
    inoFilename = inoFilename.substring((inoFilename.lastIndexOf("\\")) + 1, (inoFilename.indexOf(".")));
  }
  emotibit.setup(inoFilename);
}

void loop() {
  timeClient.update();
  setTime(timeClient.getEpochTime());
  emotibit.update();

  readSensorData();

  unsigned long currentTime = millis();

  // ส่งข้อมูลทุก 1 วินาที
  if (currentTime - lastTime_1s >= interval_1s) {
    lastTime_1s = currentTime;
    sendToFirebase("1s");
  }

  // ส่งข้อมูลทุก 1 นาที
  if (currentTime - lastTime_1min >= interval_1min) {
    lastTime_1min = currentTime;
    sendToFirebase("1min");
  }

  // ส่งข้อมูลทุก 5 นาที
  if (currentTime - lastTime_5min >= interval_5min) {
    lastTime_5min = currentTime;
    sendToFirebase("5min");
  }
}

void readSensorData() {
  size_t dataAvailablePPG = emotibit.readData(EmotiBit::DataType::PPG_GREEN, &dataPPG[countPPG], numSamplesPPG - countPPG);
  countPPG += dataAvailablePPG;
  if (countPPG >= numSamplesPPG) {
    float sumPPG = 0;
    for (size_t i = 0; i < numSamplesPPG; i++) {
      sumPPG += dataPPG[i];
    }
    averagePPG = sumPPG / numSamplesPPG;

    // คำนวณ HeartRate
    heartRate = calculateHeartRate(dataPPG, numSamplesPPG, 100);
    
    // เพิ่มค่า HeartRate ลงใน List
    listHeartRate[listIndex] = heartRate;
    
    // คำนวณค่าเฉลี่ยจาก List สำหรับ HeartRate
    avgHeartRateList = calculateMean(listHeartRate, MAX_LIST_SIZE);

    Serial.print("HeartRate: ");
    Serial.println(heartRate);
    Serial.print("Average HeartRate from List: ");
    Serial.println(avgHeartRateList);

    // เพิ่มค่า PPG ลงใน List
    listPPG[listIndex] = averagePPG;

    // คำนวณค่าเฉลี่ยจาก List
    avgPPGList = calculateMean(listPPG, MAX_LIST_SIZE);
    Serial.print("Average PPG from List: ");
    Serial.println(avgPPGList);

    countPPG = 0;
  }

  size_t dataAvailableEDA = emotibit.readData(EmotiBit::DataType::EDA, &dataEDA[countEDA], numSamplesEDA - countEDA);
  countEDA += dataAvailableEDA;
  if (countEDA >= numSamplesEDA) {
    float sumEDA = 0;
    for (size_t i = 0; i < numSamplesEDA; i++) {
      sumEDA += dataEDA[i];
    }
    averageEDA = sumEDA / numSamplesEDA;

    // เพิ่มค่า EDA ลงใน List
    listEDA[listIndex] = averageEDA;

    // คำนวณค่าเฉลี่ยจาก List
    avgEDAList = calculateMean(listEDA, MAX_LIST_SIZE);
    Serial.print("Average EDA from List: ");
    Serial.println(avgEDAList);

    countEDA = 0;
  }

  size_t dataAvailableTemp = emotibit.readData(EmotiBit::DataType::THERMOPILE, &dataTemp[countTemp], numSamplesTemp - countTemp);
  countTemp += dataAvailableTemp;

  if (countTemp >= numSamplesTemp) {
    float sumTemp = 0;
    for (size_t i = 0; i < numSamplesTemp; i++) {
      sumTemp += dataTemp[i];
    }
    averageTemp = sumTemp / numSamplesTemp;

    // เพิ่มค่า ST ลงใน List
    listST[listIndex] = averageTemp;

    // คำนวณค่าเฉลี่ยจาก List
    avgSTList = calculateMean(listST, MAX_LIST_SIZE);
    Serial.print("Average ST from List: ");
    Serial.println(avgSTList);

    countTemp = 0;
  }

  // อัปเดต index ใน List
  listIndex++;
  if (listIndex >= MAX_LIST_SIZE) {
    listIndex = 0;  // รีเซ็ต index เมื่อถึงขีดจำกัดของ List
  }
}

void sendToFirebase(const String& intervalType) {
  if (Firebase.ready()) {
    FirebaseJson json;
    json.set("EDA", averageEDA);
    json.set("PPG", averagePPG);
    json.set("ST", averageTemp);
    json.set("HeartRate", heartRate);

    // ส่งค่า SD (Standard Deviation) สำหรับทุกพารามิเตอร์
    json.set("SD-EDA", calculateSD(listEDA, MAX_LIST_SIZE));
    json.set("SD-PPG", calculateSD(listPPG, MAX_LIST_SIZE));
    json.set("SD-ST", calculateSD(listST, MAX_LIST_SIZE));
    json.set("SD-HeartRate", calculateSD(listHeartRate, MAX_LIST_SIZE));

    // สร้าง path ที่จะส่งข้อมูล
    String path = "Device/Inpatient/MD-V5-0000804/" + intervalType + "/";

    // พยายามส่งข้อมูล
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