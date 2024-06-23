#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <ESP8266WiFi.h>
#include <ThingerESP8266.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240  

#define MINOR_GRID tft.color565(100, 100, 255)
#define MAJOR_GRID tft.color565(0, 0, 255)
#define GRID_EDGE tft.color565(255, 0, 0)

#define TFT_CS 0
#define TFT_DC 15
#define TFT_RST 16
#define TFT_MOSI 13
#define TFT_SCK 14
#define TFT_MISO 12

#define USERNAME "elektroallan"
#define DEVICE_ID "esp8266"
#define DEVICE_CREDENTIAL "Sinyal EKG"
#define SSID "YOYOI"
#define SSID_PASSWORD "Keling123"

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST, TFT_MISO);
ThingerESP8266 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

const int sensorPin = A0;
const int heartRatePin = A0; // Sesuaikan dengan pin yang digunakan untuk sensor detak jantung

int x = 0;
int lastx = 0;
int lasty = 150;

// Variabel untuk menyimpan waktu saat ini
unsigned long currentTime = 0;
unsigned long lastDisplayUpdateTime = 0; // Waktu terakhir kali tampilan diperbarui
unsigned long displayUpdateInterval = 1500; // Interval perubahan tampilan (1.5 detik)

// Deklarasi prototipe fungsi
void drawGrid();

// Variabel untuk menyimpan data sensor
int sensorValue = 0;
int threshold = 600; // Threshold untuk mendeteksi detak jantung (disesuaikan dengan kalibrasi)

// Variabel untuk menghitung BPM
int beatsPerMinute = 0;
unsigned long lastBeatTime = 0;
unsigned long interval = 0;

// Variabel untuk filter rata-rata bergerak
const int numReadings = 10;
int readings[numReadings]; // Array untuk menyimpan pembacaan sensor
int readIndex = 0; // Indeks untuk pembacaan saat ini
int total = 0; // Total pembacaan
int average = 0; // Rata-rata pembacaan

// Variabel untuk deteksi waktu terakhir detak jantung dan timeout error
unsigned long lastDetectionTime = 0; // Waktu deteksi detak jantung terakhir
unsigned long errorTimeout = 5000; // Waktu timeout dalam milidetik (5 detik)

void setup() {
  Serial.begin(115200);
  
  pinMode(sensorPin, INPUT); // Atur pin sensor ECG sebagai input
  pinMode(heartRatePin, INPUT); // Atur pin sensor detak jantung sebagai input

  tft.begin();
  tft.setRotation(1);  
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  pinMode(heartRatePin, INPUT); // Atur pin sensor sebagai input

  // Inisialisasi array pembacaan ke nol
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  drawGrid();

  // Connect to WiFi
  WiFi.begin(SSID, SSID_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initialize Thinger.io
  thing.add_wifi(SSID, SSID_PASSWORD);

  // Define Thinger.io resources
  thing["ecg_data"] >> [](pson & out){
      out["ecg"] = analogRead(sensorPin);
      out["bpm"] = beatsPerMinute;
  };
}

void loop() {
  currentTime = millis(); // Ambil waktu saat ini
  
  // Cek apakah sudah waktunya memperbarui tampilan
  if (currentTime - lastDisplayUpdateTime >= displayUpdateInterval) {
    // Masukkan kode untuk memperbarui tampilan di sini
    int lebar = tft.width();
    int tinggi = tft.height();
    tft.drawLine(0, tinggi / 2, lebar, tinggi / 2, ILI9341_RED);

    if (x > lebar) {
      tft.fillRect(0, 50, lebar, 120, ILI9341_BLACK);
      x = 0;
      lastx = x;
      drawGrid();
    }

    int dataecg = analogRead(sensorPin);

    // Filter rata-rata bergerak untuk sensor ECG
    total -= readings[readIndex];
    readings[readIndex] = dataecg;
    total += readings[readIndex];
    readIndex = (readIndex + 1) % numReadings;
    average = total / numReadings;

    // Filter rata-rata bergerak untuk sensor detak jantung
    total -= readings[readIndex];
    readings[readIndex] = analogRead(heartRatePin);
    total += readings[readIndex];
    readIndex = (readIndex + 1) % numReadings;
    average = total / numReadings;

    // Jika nilai sensor melebihi threshold, detak jantung terdeteksi
    if (average > threshold) {
      if (millis() - lastBeatTime > 600) { // Menjaga interval minimum antara detak jantung untuk menghindari noise
        unsigned long interval = millis() - lastBeatTime; // Hitung interval waktu antara dua detak jantung
        lastBeatTime = millis(); // Perbarui waktu detak jantung terakhir
        beatsPerMinute = 60000 / interval; // Perbarui nilai variabel global BPM (60000 milidetik dalam satu menit)

        // Tampilkan BPM pada serial monitor
        Serial.print("BPM: ");
        Serial.println(beatsPerMinute);
      }
    }

    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);  
    int y = 150 - (average / 10);
    tft.drawLine(lastx, lasty, x, y, ILI9341_WHITE);
    lasty = y;
    lastx = x;

    x += 1;

    // Clear the previous BPM value
    tft.fillRect(10, 10, 100, 20, ILI9341_BLACK); // Adjust the rectangle size as needed

      // Tampilkan BPM pada layar TFT
    tft.setCursor(10, 10);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.print("BPM: ");
    tft.println(beatsPerMinute);

    // Kirim data ke Thinger.io
    thing.handle();

    // Perbarui waktu terakhir tampilan diperbarui
    lastDisplayUpdateTime = currentTime;
  }
  
  // Sisanya dari loop() tetap sama
  int lebar = tft.width();
  int tinggi = tft.height();
  tft.drawLine(0, tinggi / 2, lebar, tinggi / 2, ILI9341_RED);

  if (x > lebar) {
    tft.fillRect(0, 50, lebar, 120, ILI9341_BLACK);
    x = 0;
    lastx = x;
    drawGrid();
  }

  int dataecg = analogRead(sensorPin);

  // Filter rata-rata bergerak untuk sensor ECG
  total -= readings[readIndex];
  readings[readIndex] = dataecg;
  total += readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  average = total / numReadings;

  // Filter rata-rata bergerak untuk sensor detak jantung
  total -= readings[readIndex];
  readings[readIndex] = analogRead(heartRatePin);
  total += readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  average = total / numReadings;

  // Jika nilai sensor melebihi threshold, detak jantung terdeteksi
  if (average > threshold) {
    if (millis() - lastBeatTime > 600) { // Menjaga interval minimum antara detak jantung untuk menghindari noise
      unsigned long interval = millis() - lastBeatTime; // Hitung interval waktu antara dua detak jantung
      lastBeatTime = millis(); // Perbarui waktu detak jantung terakhir
      beatsPerMinute = 60000 / interval; // Perbarui nilai variabel global BPM (60000 milidetik dalam satu menit)

      // Tampilkan BPM pada serial monitor
      Serial.print("BPM: ");
      Serial.println(beatsPerMinute);
    }
  }

  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);  
  int y = 150 - (average / 10);
  tft.drawLine(lastx, lasty, x, y, ILI9341_WHITE);
  lasty = y;
  lastx = x;

  x += 1;

  // Clear the previous BPM value
  tft.fillRect(10, 10, 100, 20, ILI9341_BLACK); // Adjust the rectangle size as needed

  // Tampilkan BPM pada layar TFT
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("BPM: ");
  tft.println(beatsPerMinute);

  // Kirim data ke Thinger.io
  thing.handle();

  delay(50); // Add a delay to slow down the loop
}

void drawGrid() {
  for (int v = 5; v < SCREEN_WIDTH; v += 10) {
    tft.drawFastVLine(v, 0, SCREEN_HEIGHT, MINOR_GRID);
  }
  for (int h = 5; h < SCREEN_HEIGHT; h += 10) {
    tft.drawFastHLine(0, h, SCREEN_WIDTH, MINOR_GRID);
  }

  for (int v = 10; v < SCREEN_WIDTH; v += 10) {
    tft.drawFastVLine(v, 0, SCREEN_HEIGHT, MAJOR_GRID);
  }
  for (int h = 10; h < SCREEN_HEIGHT; h += 10) {
    tft.drawFastHLine(0, h, SCREEN_WIDTH, MAJOR_GRID);
  }

  tft.drawFastVLine(0, 0, SCREEN_HEIGHT - 1, GRID_EDGE);
  tft.drawFastVLine(SCREEN_WIDTH - 1, 0, SCREEN_HEIGHT - 1, GRID_EDGE);
  tft.drawFastHLine(0, 0, SCREEN_WIDTH - 1, GRID_EDGE);
  tft.drawFastHLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, GRID_EDGE);
}
