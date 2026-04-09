// 24-Sound Audio Recorder
// Created with the assistance of Claude (Anthropic) — claude.ai
// Hardware: ESP32 + MAX9814 Microphone + MicroSD Card Module
//
// Wiring:
//   MAX9814  →  ESP32
//   VCC      →  3V3
//   GND      →  GND
//   OUT      →  GPIO32
//   Gain     →  N/C (leaves gain at 60dB)
//
//   SD Module  →  ESP32
//   GND        →  GND
//   VCC        →  VIN (5V)
//   MISO       →  D19
//   MOSI       →  D23
//   SCK        →  D18
//   CS         →  D5

// Libraries
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// Constants
// Pins
#define MIC_PIN          32
#define SD_CS_PIN         5
#define LED_PIN           2
#define SAMPLE_RATE    8000             // Samples per second
#define RECORD_SECS      10             // Length of recording
#define SAMPLE_INTERVAL_US  125         // Microseconds between each sample

// Global variables
File     audioFile;
uint32_t sampleCount = 0;

// Setting up WAV file header
void writeWavHeader(File &f, uint32_t numSamples) {
  uint32_t dataSize  = numSamples;
  uint32_t chunkSize = 36 + dataSize;
  f.write((uint8_t*)"RIFF", 4);
  f.write((uint8_t*)&chunkSize, 4);
  f.write((uint8_t*)"WAVE", 4);
  f.write((uint8_t*)"fmt ", 4);
  uint32_t sub1 = 16;           f.write((uint8_t*)&sub1, 4);
  uint16_t fmt  = 1;            f.write((uint8_t*)&fmt,  2);
  uint16_t ch   = 1;            f.write((uint8_t*)&ch,   2);
  uint32_t sr   = SAMPLE_RATE;  f.write((uint8_t*)&sr,   4);
  uint32_t br   = SAMPLE_RATE;  f.write((uint8_t*)&br,   4);
  uint16_t ba   = 1;            f.write((uint8_t*)&ba,   2);
  uint16_t bps  = 8;            f.write((uint8_t*)&bps,  2);
  f.write((uint8_t*)"data", 4);
  f.write((uint8_t*)&dataSize, 4);
}

// Rapid flashing lights to show error
void errorHalt() {
  while (true) {
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW);  delay(100);
  }
}

void setup() {
  // Setting up LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(500);

  analogSetAttenuation(ADC_11db); // Changes expected voltage from 1V to 3.3V
  analogReadResolution(12);       // Each reading is 0-4095, converted to 0-255 for WAV later

  // Mounting SD card and setting SPI clock to 4 MHz
  Serial.println("Mounting SD card...");
  if (!SD.begin(SD_CS_PIN, SPI, 4000000)) {
    Serial.println("ERROR: SD card mount failed!");
    Serial.flush();
    errorHalt();
  }
  Serial.println("SD card mounted.");

  // Creating file on the SD card
  audioFile = SD.open("/REC000.wav", FILE_WRITE); // Must manually change file name each time or new file will be lost
  if (!audioFile) {
    Serial.println("ERROR: Could not create REC000.wav");
    Serial.flush();
    errorHalt();
  }
  Serial.println("File created OK.");
  Serial.flush();

  writeWavHeader(audioFile, 0); // Write placeholder WAV file header

  // Blinking 3 times before recording starts
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(300);
    digitalWrite(LED_PIN, LOW);  delay(300);
  }
  delay(500);

  // Starting recording
  digitalWrite(LED_PIN, HIGH);
  Serial.println("Recording...");
  Serial.flush();
  Serial.end();
}

void loop() {
  // Stops loop function after recording is done
  static uint32_t lastSampleTime = 0;
  static bool     done           = false;
  if (done) return;

  // Records sample every sample interval
  uint32_t now = micros();
  if ((now - lastSampleTime) >= SAMPLE_INTERVAL_US) {
    lastSampleTime = now;
    int raw = analogRead(MIC_PIN);
    uint8_t sample = (uint8_t)(raw >> 4); // Microphone reading from 12-bit to 8-bit for WAV file
    audioFile.write(sample);
    sampleCount++;

    // Blinking every half second during recording
    if ((sampleCount % (SAMPLE_RATE / 2)) == 0) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    // After length of recording is passed
    if (sampleCount >= (uint32_t)(SAMPLE_RATE * RECORD_SECS)) {
      audioFile.seek(0);                          // Rewind to start of file to overwrite placeholder header
      writeWavHeader(audioFile, sampleCount);     // Overwriting placeholder WAV header
      audioFile.flush();                          // Forces sitting data to SD card
      audioFile.close();                          // Closes file
      digitalWrite(LED_PIN, LOW);                 // Turn off light
      done = true;
    }
  }
}
