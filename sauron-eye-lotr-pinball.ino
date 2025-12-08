#include <Arduino_GFX_Library.h>
#include <AnimatedGIF.h>
#include "FS.h"
#include "SPIFFS.h" 

// --- CONFIGURATION ---
// Offsets for 180x180 GIF on 240x240 Screen
#define GIF_OFFSET_X  30
#define GIF_OFFSET_Y  30

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define FILENAME "/eye.gif"

// --- PINOUT: Spotpear / Type A / Waveshare ESP32-S3-LCD-1.28 ---
#define TFT_BL   40
#define TFT_RST  12
#define TFT_MOSI 11
#define TFT_SCK  10
#define TFT_CS   9
#define TFT_DC   8
#define TFT_MISO GFX_NOT_DEFINED 

// Setup the DataBus & Driver
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST, 0, true);

AnimatedGIF gif;
File f;
uint16_t *frameBuffer; // Persistent buffer for transparency

// --- FILE HELPERS ---
void * GIFOpenFile(const char *fname, int32_t *pSize) {
  f = SPIFFS.open(fname);
  if (f) {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
}
void GIFCloseFile(void *pHandle) { if (f) f.close(); }
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen) iBytesRead = pFile->iSize - pFile->iPos - 1;
  if (iBytesRead <= 0) return 0;
  iBytesRead = f.read(pBuf, iBytesRead);
  pFile->iPos = f.position();
  return iBytesRead;
}
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  f.seek(iPosition);
  pFile->iPos = (int32_t)f.position();
  return pFile->iPos;
}

// --- DRAW FUNCTION ---
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *usPalette;
  int x, iWidth = pDraw->iWidth;
  int y = pDraw->iY + pDraw->y;
  
  if (y >= SCREEN_HEIGHT || pDraw->iX >= SCREEN_WIDTH) return;
  
  // Get pointer to current line in persistent buffer
  uint16_t *lineBuffer = &frameBuffer[(y * SCREEN_WIDTH) + pDraw->iX];
  
  usPalette = pDraw->pPalette;
  s = pDraw->pPixels;

  if (pDraw->ucHasTransparency) {
    uint8_t c, ucTransparent = pDraw->ucTransparent;
    for (x = 0; x < iWidth; x++) {
      c = *s++;
      if (c != ucTransparent) {
        lineBuffer[x] = usPalette[c];
      }
    }
  } else {
    for (x = 0; x < iWidth; x++) {
      lineBuffer[x] = usPalette[*s++];
    }
  }

  // Draw updated line to screen
  gfx->draw16bitRGBBitmap(
    pDraw->iX + GIF_OFFSET_X, 
    y + GIF_OFFSET_Y, 
    lineBuffer, 
    iWidth, 
    1
  );
}

// --- SERIAL UPLOAD LOGIC ---
void handleSerialUpload() {
  Serial.println("READY_TO_RECEIVE");
  
  // Wait for 4-byte size header
  unsigned long startTime = millis();
  while(Serial.available() < 4) {
    if (millis() - startTime > 5000) return; // Timeout
    delay(1);
  }
  
  uint32_t fileSize = 0;
  fileSize |= Serial.read();
  fileSize |= (Serial.read() << 8);
  fileSize |= (Serial.read() << 16);
  fileSize |= (Serial.read() << 24);
  
  Serial.print("SIZE:");
  Serial.println(fileSize);

  // Open file for writing
  File file = SPIFFS.open(FILENAME, "w");
  if(!file) {
    Serial.println("ERROR:OPEN_FAIL");
    return;
  }

  // We use a fixed chunk size for the protocol
  const int CHUNK_SIZE = 1024;
  uint8_t buf[CHUNK_SIZE]; 
  uint32_t bytesRead = 0;
  
  // UI Update - Initial Layout
  gfx->fillScreen(BLACK);
  gfx->setCursor(20, 60);
  gfx->setTextColor(GREEN);
  gfx->setTextSize(2);
  gfx->println("DOWNLOADING");
  
  gfx->setTextSize(1);
  gfx->setCursor(20, 90);
  gfx->print("Size: ");
  gfx->print(fileSize / 1024);
  gfx->println(" KB");

  // Draw Empty Progress Bar Container
  gfx->drawRect(20, 140, 200, 20, WHITE);

  int lastPercent = -1;

  // --- ROBUST BLOCK-BASED TRANSFER LOOP ---
  while (bytesRead < fileSize) {
    
    // 1. Request Data from PC
    Serial.println("NEXT"); 

    // 2. Calculate how many bytes we expect in this chunk
    uint32_t remaining = fileSize - bytesRead;
    uint32_t bytesToReceive = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;

    // 3. Read exact amount with timeout
    uint32_t chunkReceived = 0;
    unsigned long chunkStart = millis();
    
    while(chunkReceived < bytesToReceive) {
      if (Serial.available()) {
        buf[chunkReceived++] = Serial.read();
        chunkStart = millis(); // Reset timeout on successful read
      } else {
        if (millis() - chunkStart > 3000) { // 3 Second Timeout
           gfx->setTextColor(RED);
           gfx->setCursor(20, 170);
           gfx->print("TIMEOUT!");
           file.close();
           return;
        }
      }
    }
    
    // 4. Write Block to Disk
    file.write(buf, bytesToReceive);
    bytesRead += bytesToReceive;

    // 5. Update UI
    int percent = (int)((bytesRead * 100) / fileSize);
    
    // Update screen only if percent changed by 5% increments
    if (percent != lastPercent && percent % 5 == 0) {
      lastPercent = percent;
      
      // Update Text
      gfx->fillRect(20, 110, 100, 15, BLACK); 
      gfx->setCursor(20, 115);
      gfx->print("Progress: ");
      gfx->print(percent);
      gfx->print("%");

      // Update Bar
      int barWidth = map(percent, 0, 100, 0, 196);
      if(barWidth > 0) {
        gfx->fillRect(22, 142, barWidth, 16, GREEN);
      }
    }
  }
  
  file.close();
  Serial.println("\nDONE");
  
  // Success Screen
  gfx->fillScreen(BLACK);
  gfx->setCursor(50, 100);
  gfx->setTextColor(BLUE);
  gfx->setTextSize(2);
  gfx->println("COMPLETE!");
  gfx->setCursor(50, 130);
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->println("Rebooting...");
  
  delay(1000);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  
  // 1. Allocate Framebuffer (~115KB)
  frameBuffer = (uint16_t *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));
  if (!frameBuffer) {
    Serial.println("ALLOC FAIL");
    while(1);
  }
  memset(frameBuffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // 2. Mount SPIFFS (Format if failed)
  if (!SPIFFS.begin(true)) { 
    Serial.println("SPIFFS Mount Failed");
  } else {
    Serial.println("SPIFFS Mounted");
    Serial.print("Total Space: ");
    Serial.println(SPIFFS.totalBytes());
  }

  // 3. Init Display
  gfx->begin(20000000); 
  gfx->fillScreen(BLACK);
  gif.begin(LITTLE_ENDIAN_PIXELS);

  // 4. Check for GIF
  if (!SPIFFS.exists(FILENAME)) {
    Serial.println("FILE_MISSING");
    
    gfx->setCursor(30, 80);
    gfx->setTextColor(RED);
    gfx->setTextSize(2);
    gfx->println("WAITING FOR");
    
    gfx->setCursor(50, 110);
    gfx->setTextColor(ORANGE);
    gfx->println("UPLOAD");
    
    gfx->setTextSize(1);
    gfx->setTextColor(WHITE);
    gfx->setCursor(20, 150);
    gfx->println("Run python script now...");
    
    // Blocking loop: Wait for Python script
    while(true) {
      if(Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        // Trim whitespace just in case
        cmd.trim(); 
        if(cmd == "START_UPLOAD") {
          handleSerialUpload();
          break;
        }
      }
      // Re-broadcast status every second so Python catches it
      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 1000) {
        Serial.println("FILE_MISSING");
        lastPrint = millis();
      }
      delay(10);
    }
  }
}

void loop() {
  if (gif.open(FILENAME, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
     while (gif.playFrame(true, NULL)) { }
     gif.close();
  } else {
    delay(1000);
  }
}
