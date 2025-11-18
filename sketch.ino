#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <DIYables_TFT_Round.h>
#include "html.h"
#include <Fonts/FreeSans24pt7b.h>
#include <time.h>

#define BLACK     DIYables_TFT::colorRGB(0, 0, 0)

#define PIN_RST 38 // The Arduino pin connected to the RST pin of the circular TFT display
#define PIN_DC 33  // The Arduino pin connected to the DC pin of the circular TFT display
#define PIN_CS 12 // The Arduino pin connected to the CS pin of the circular TFT display

DIYables_TFT_GC9A01_Round TFT_display(PIN_RST, PIN_DC, PIN_CS);

AsyncWebServer server(80);
File uploadFile;

// Image storage
uint16_t *imageData = nullptr;
size_t imageSize = 0; // number of pixels

bool updateImage = false;

// function to load background image to memory (imageData pointer)
void storeBinAsRGBArray(const char *path) {
  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file!");
    return;
  }

  size_t fileSize = file.size();
  Serial.printf("File size: %u bytes\n", fileSize);

  if (fileSize % 2 != 0) {
    Serial.println("ERROR: RGB565 data must be even-numbered bytes");
    file.close();
    return;
  }

  imageSize = fileSize / 2;      // number of RGB565 pixels
  imageData = (uint16_t*)malloc(fileSize);
  if (!imageData) {
    Serial.println("Failed to allocate memory!");
    file.close();
    return;
  }

  size_t index = 0;
  while (file.available()) {
    uint8_t low  = file.read();    // BMP RGB565 is little-endian: LSB first
    uint8_t high = file.read();
    uint16_t color = (high << 8) | low;

    imageData[index++] = color;
  }

  file.close();
  Serial.printf("Stored %u RGB565 pixels (%u bytes)\n", imageSize, imageSize * 2);
  updateImage = true;
}

void restoreBackground(int x, int y, int w, int h) {
    if (!imageData) return;

    for (int row = 0; row < h; row++) {
        int srcIndex = (y + row) * 240 + x;
        TFT_display.drawRGBBitmap(
            x,
            y + row,
            &imageData[srcIndex],
            w,
            1
        );
    }
}

/// BEGIN LOGIC
void setup() {
  Serial.begin(115200);
  WiFi.softAP("avengers32-ap-tinkertanker");
  Serial.println(WiFi.softAPIP());

  TFT_display.begin();

  TFT_display.setFont(&FreeSans24pt7b);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed!");
    return;
  }

  // BACKGROUND IMAGE HANDLER
  server.on("/upload", HTTP_POST, 
    [](AsyncWebServerRequest *req){
        req->send(200, "text/plain", "OK");
        Serial.println("Upload finished.");
        storeBinAsRGBArray("/raw.rgb565");
    },
    NULL,
    // This handler will write the data sent by the client into a file in the FS called raw.rgb565
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      
      if (index == 0) {
        Serial.printf("Begin upload, total: %u bytes\n", total);
        uploadFile = SPIFFS.open("/raw.rgb565", "w");
      }

      if (uploadFile) {
        uploadFile.write(data, len);
      }

      if (index + len == total) {
        Serial.println("Upload complete!");
        uploadFile.close();
      }
    }
  );

  // TIME SYNCHRONISATION HANDLER
  server.on("/settime", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!req->hasParam("t", true)) {
      req->send(400, "text/plain", "Missing timestamp");
      return;
    }

    String ts = req->getParam("t", true)->value();
    uint64_t ms = ts.toInt();

    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    settimeofday(&tv, nullptr);

    Serial.println("RTC updated from browser!");
    req->send(200, "text/plain", "Time set");
  });

  // Root page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", index_html);//"<form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='file'><input type='submit'></form>");
  });

  server.begin();

  storeBinAsRGBArray("/raw.rgb565");
}

// Drawing the on screen
void loop() {
  if (updateImage) {
    TFT_display.fillScreen(BLACK);

    TFT_display.drawRGBBitmap(0, 0, imageData, 240, 240);

    //imageData = nullptr;
    updateImage = false;
  }
  delay(1000);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      Serial.println("No RTC time set.");
      return;
  }

  char buf_h[3];
  strftime(buf_h, sizeof(buf_h), "%H", &timeinfo);

  char buf_m[3];
  strftime(buf_m, sizeof(buf_m), "%M", &timeinfo);

  Serial.println(buf_h);
  Serial.println(buf_m);

  TFT_display.setCursor(80, 120);
  TFT_display.print(buf_h);
  restoreBackground(80, 120, 160, 40);

  TFT_display.setCursor(80, 170);
  TFT_display.print(buf_m);
}
