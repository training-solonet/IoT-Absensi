#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

// ================= USER CONFIG =================
const char* ssid     = "";
const char* password = "";

String apiHost = "";
String apiPath = "";
String apiToken = "";

// ================= PIN SETUP =================
// Shared SPI
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

// Chip Select
#define RFID_SS   5
#define RFID_RST  2
#define W5500_CS  26

// LED & BUZZER
#define LED_HIJAU 16
#define LED_MERAH 27
#define BUZZER    33

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// RFID
MFRC522 rfid(RFID_SS, RFID_RST);

// Ethernet MAC
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

bool lanUp = false;
bool wifiUp = false;
String lastConn = "";   // Track pergantian koneksi


// ================= NOTIFIER =================
void notifSukses() {
  digitalWrite(LED_HIJAU, HIGH);
  tone(BUZZER, 2000);
  delay(200);
  noTone(BUZZER);
  digitalWrite(LED_HIJAU, LOW);
}

void notifGagal() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_MERAH, HIGH);
    tone(BUZZER, 1500);
    delay(200);
    noTone(BUZZER);
    digitalWrite(LED_MERAH, LOW);
    delay(150);
  }
}

// ================= ETH INIT =================
void initEthernet() {
  Serial.println("Init LAN...");

  digitalWrite(RFID_SS, HIGH);
  digitalWrite(W5500_CS, HIGH);
  delay(10);

  Ethernet.init(W5500_CS);

  if (Ethernet.begin(mac) == 0) {
    Serial.println("LAN DHCP gagal");
    lanUp = false;
  } else {
    Serial.println("LAN OK");
    lanUp = true;

    lcd.clear();
    lcd.print("LAN Terhubung");
    delay(800);
    lcd.clear();
    lcd.print("Tempel Kartu");
  }
}

// ================= WIFI INIT =================
void initWiFi() {

  Serial.println("Init WiFi...");

  WiFi.disconnect(true);
  delay(200);

  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 25) {
    Serial.print(".");
    delay(300);
    tries++;
  }
  Serial.println();

  wifiUp = (WiFi.status() == WL_CONNECTED);

  if (wifiUp) {
    Serial.println("WiFi OK");

    lcd.clear();
    lcd.print("WiFi Terhubung");
    delay(800);
    lcd.clear();
    lcd.print("Tempel Kartu");
  } else {
    Serial.println("WiFi gagal");
  }
}


// ================= CHECK CONNECTIONS =================
void checkConnections() {

  int link = Ethernet.linkStatus();

  // === LAN ON ===
  if (link == LinkON) {

    if (!lanUp) {
      Serial.println("LAN kembali!");
      initEthernet();

      if (lastConn != "LAN") {
        lcd.clear();
        lcd.print("Pindah ke LAN");
        delay(800);
        lcd.clear();
        lcd.print("Tempel Kartu");
        lastConn = "LAN";
      }
    }
  }

  // === LAN OFF ===
  else {

    if (lanUp) {
      Serial.println("LAN putus → WiFi");
      lanUp = false;

      lcd.clear();
      lcd.print("LAN putus");
      lcd.setCursor(0,1);
      lcd.print("Ke WiFi...");
      delay(800);

      initWiFi();
      lastConn = "WIFI";
    }
  }

  // === WIFI CHECK ===
  if (!lanUp) {
    if (WiFi.status() != WL_CONNECTED) {
      wifiUp = false;
      initWiFi();
    }

    if (wifiUp && lastConn != "WIFI") {
      lcd.clear();
      lcd.print("Pindah ke WiFi");
      delay(800);
      lcd.clear();
      lcd.print("Tempel Kartu");
      lastConn = "WIFI";
    }
  }
}


// ================= SEND DATA =================
bool sendPost(String body, String &resOut) {

  WiFiClient wifiClient;
  EthernetClient lanClient;
  Client *client;

  if (lanUp) {
    client = &lanClient;
    Serial.println("Kirim via LAN");
  } else if (wifiUp) {
    client = &wifiClient;
    Serial.println("Kirim via WiFi");
  } else {
    Serial.println("Tidak ada koneksi");
    return false;
  }

  if (!client->connect(apiHost.c_str(), 80)) {
    Serial.println("Gagal connect server");
    return false;
  }

  client->print("POST " + apiPath + " HTTP/1.1\r\n");
  client->print("Host: " + apiHost + "\r\n");
  client->print("Content-Type: application/json\r\n");
  client->print("X-Authorization: " + apiToken + "\r\n");
  client->print("Content-Length: " + String(body.length()) + "\r\n");
  client->print("Connection: close\r\n\r\n");
  client->print(body);

  unsigned long timeout = millis();
  while (!client->available()) {
    if (millis() - timeout > 5000) return false;
  }

  String res = client->readString();
  Serial.println("RAW RESPONSE:");
  Serial.println(res);

  int jsonStart = res.indexOf("{");
  if (jsonStart == -1) return false;

  resOut = res.substring(jsonStart);
  return true;
}


// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RFID_SS, OUTPUT);
  pinMode(W5500_CS, OUTPUT);

  digitalWrite(RFID_SS, HIGH);
  digitalWrite(W5500_CS, HIGH);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SPI.setFrequency(4000000);

  // RFID
  digitalWrite(W5500_CS, HIGH);
  digitalWrite(RFID_SS, LOW);
  delay(10);
  rfid.PCD_Init();
  digitalWrite(RFID_SS, HIGH);

  lcd.clear();
  lcd.print("Inisialisasi...");
  delay(1000);

  // WAIT AGAR LAN STABIL
  delay(1500);

  // Start LAN then WiFi fallback
  initEthernet();

  if (!lanUp) {
    initWiFi();
    lastConn = "WIFI";
  } else {
    lastConn = "LAN";
  }

  lcd.clear();
  lcd.print("Tempel Kartu");
}


// ================= LOOP =================
void loop() {

  checkConnections();

  digitalWrite(W5500_CS, HIGH);
  digitalWrite(RFID_SS, LOW);
  delayMicroseconds(50);

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    digitalWrite(RFID_SS, HIGH);
    return;
  }

  // Ambil UID
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.println("UID: " + uid);

  digitalWrite(RFID_SS, HIGH);

  lcd.clear();
  lcd.print("Memproses...");

  // JSON BODY
  StaticJsonDocument<128> doc;
  doc["uid"] = uid;
  String body;
  serializeJson(doc, body);

  String resJson;
  bool ok = sendPost(body, resJson);

  lcd.clear();

  if (!ok) {
    lcd.print("Gagal koneksi");
    notifGagal();
    delay(1500);
    lcd.clear();
    lcd.print("Tempel Kartu");
    return;
  }

  StaticJsonDocument<256> resDoc;
  DeserializationError err = deserializeJson(resDoc, resJson);

  if (err) {
    lcd.print("JSON Error");
    notifGagal();
    delay(1500);
    lcd.clear();
    lcd.print("Tempel Kartu");
    return;
  }

  String status = resDoc["status"] | "NULL";
  String message = resDoc["message"] | "-";

  lcd.setCursor(0,0); lcd.print(status);
  lcd.setCursor(0,1); lcd.print(message);

  if (status == "Berhasil Absen") {
    notifSukses();
  } else {
    notifGagal();
  }

  delay(1500);
  lcd.clear();
  lcd.print("Tempel Kartu");
}
