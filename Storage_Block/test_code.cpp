/*
PINS
SD        GPIO

CS        5
SCK        18
MOSI    23
MISO    19
VCC        5V
GND        GND
*/

#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define SD_CS 5 // chip select
SPIClass spi = SPIClass(HSPI); // spi class

// benchmarks
const long fileSize = 1024 * 1024; // 1 MB
const int bufferSize = 4096; // 4 KB buffer

void setup() {
    Serial.begin(115200);

    // initialize spi
    spi.begin(18, 19, 23, 5); // SCK, MISO, MOSI, CS

    // initialize SD card
    int clock_speed = 20000000;
    if (!SD.begin(SD_CS, spi, clock_speed)) {
        Serial.println("Card mount failed!");
        return;
    }
    Serial.println("Card mounted...");
}

void loop() {
    Serial.println("\nSpeed test...");

    // buffer for data to write
    byte buffer[bufferSize];
    for (int i = 0; i < bufferSize; i++) {
        buffer[i] = 'X';
    }

    // Open file for writing
    File file = SD.open("/bench.txt", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing!");
        return;
    }

    // time write
    long startTime = micros();
    for (long i = 0; i < fileSize; i += bufferSize) {
        file.write(buffer, bufferSize);
    }
    long endTime = micros();

    file.close();

    // print write time
    long duration = endTime - startTime;
    float writeSpeed = (float)fileSize / (float)duration * 1000000.0 / 1024.0; // in KB/s
    Serial.printf("Wrote %ld bytes in %.2f seconds.\n", fileSize, duration / 1000000.0);
    Serial.printf("Write speed: %.2f KB/s.\n", writeSpeed);

    // open file for reading
    file = SD.open("/bench.txt", FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading!");
        return;
    }

    // time read
    startTime = micros();
    while (file.read(buffer, bufferSize) > 0) {
        //
    }
    endTime = micros();

    file.close();

    // print read time
    duration = endTime - startTime;
    float readSpeed = (float)fileSize / (float)duration * 1000000.0 / 1024.0; // in KB/s
    Serial.printf("Read %ld bytes in %.2f seconds.\n", fileSize, duration / 1000000.0);
    Serial.printf("Read speed: %.2f KB/s.\n", readSpeed);

    // remove file
    SD.remove("/bench.txt");
}
