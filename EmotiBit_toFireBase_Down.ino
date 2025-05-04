#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "EmotiBit.h"
#include <FirebaseESP32.h>
#include <TimeLib.h>
#include <math.h>

// Firebase Data object
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// WiFi credentials
#define WIFI_SSID "IoTstudio_148"
#define WIFI_PASSWORD "IoT@admin"

// Firebase project settings
#define API_KEY "euw2n2EBYABgGk9S0K7oT95nKrswcpN6Txs3Ghor"
#define DATABASE_URL "https://inewgenweb-default-rtdb.asia-southeast1.firebasedatabase.app"

// สร้างออบเจ็กต์สำหรับ EmotiBit
EmotiBit emotibit;

// ขนาดอาเรย์และตัวแปรอื่นๆ คงเดิม...
const size_t WINDOW_SIZE_ARRAY = 100;
const size_t numSamplesPPG = 100;
const unsigned long SAMPLE_PERIOD = 40;

float dataPPGInfared[numSamplesPPG];
float dataPPGRed[numSamplesPPG];
size_t countPPG_Infared = 0;
size_t countPPG_Red = 0;
float averagePPG_Infared = 0;
float averagePPG_Red = 0;
float listPPGInfared[WINDOW_SIZE_ARRAY];
float listPPGRed[WINDOW_SIZE_ARRAY];
size_t listIndex = 0;

void setup() {
  Serial.begin(115200);
  
  // เชื่อมต่อ WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // กำหนดค่า Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  Serial.println("Connecting to Firebase...");
  
  Firebase.begin(DATABASE_URL, API_KEY);
  Firebase.reconnectWiFi(true);
  
  // Set database read timeout to 1 minute
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  // Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");
  
  Serial.println("Firebase connection status: " + String(Firebase.ready() ? "Connected" : "Failed"));
  
  // Initialize EmotiBit
  // if (!emotibit.begin()) {
  //   Serial.println("EmotiBit initialization failed!");
  //   while (1);
  // }
}

void loop() {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready, attempting to reconnect...");
    delay(1000);
    return;
  }
  
  readSensorData();
  
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime >= 1000) {
    sendToFirebase("realtime");
    lastSendTime = millis();
  }
  
  delay(SAMPLE_PERIOD);
}

void sendToFirebase(const String& intervalType) {
  if (Firebase.ready()) {
    FirebaseJson json;
    
    // ส่งข้อมูลดิบ PPG Infrared
    FirebaseJsonArray ppgInfaredArray;
    for (size_t i = 0; i < numSamplesPPG; i++) {
      ppgInfaredArray.add(dataPPGInfared[i]);
    }
    json.set("rawPPG_Infared", ppgInfaredArray);

    // ส่งข้อมูลดิบ PPG Red
    FirebaseJsonArray ppgRedArray;
    for (size_t i = 0; i < numSamplesPPG; i++) {
      ppgRedArray.add(dataPPGRed[i]);
    }
    json.set("rawPPG_Red", ppgRedArray);
    
    // เพิ่ม timestamp
    json.set("timestamp", (int)time(nullptr));
    
    String path = "Device/Inpatient/MD-V5-0000205/" + intervalType;
    
    if (Firebase.setJSON(firebaseData, path, json)) {
      Serial.println("ส่งข้อมูลสำเร็จที่ path: " + path);
    } else {
      Serial.println("ส่งข้อมูลไม่สำเร็จ: " + firebaseData.errorReason());
      Serial.println("Error code: " + String(firebaseData.errorCode()));
    }
  } else {
    Serial.println("Firebase ไม่พร้อมใช้งาน กำลัง retry...");
    delay(1000);
  }
}