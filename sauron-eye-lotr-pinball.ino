#include <Arduino_GFX_Library.h>
#include <AnimatedGIF.h>
#include "FS.h"
#include "SPIFFS.h"

// --- PINOUT: Spotpear / Type A ---
#define TFT_BL   40
#define TFT_RST  12
#define TFT_MOSI 11
#define TFT_SCK  10
#define TFT_CS   9
#define TFT_DC   8
#define TFT_MISO 12 

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST, 0, true);

AnimatedGIF gif;
File f;

// --- THE FRAME BUFFER (Short Term Memory) ---
// 120x120 pixels x 2 bytes per pixel = ~28KB RAM. 
// The ESP32-S3 has plenty of RAM for this.
uint16_t frameBuffer[120 * 120]; 

// Center Offset
int x_offset = 60;
int y_offset = 60;

// --- FILE HANDLERS ---
void * GIFOpenFile(const char *fname, int32_t *pSize) {
  f = SPIFFS.open(fname);
  if (f) {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
}
void GIFCloseFile(void *pHandle) {
  if (f) f.close();
}
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

// --- SMART DRAW FUNCTION ---
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth = pDraw->iWidth;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // Current Line number (0-119)
  s = pDraw->pPixels;

  // 1. Point to the correct line in our internal RAM buffer
  // We calculate the exact memory address for this row of pixels
  uint16_t *lineBuffer = &frameBuffer[y * 120];

  // 2. Update the RAM buffer
  if (pDraw->ucHasTransparency) {
    uint8_t c, ucTransparent = pDraw->ucTransparent;
    for (x = 0; x < iWidth; x++) {
      c = *s++;
      if (c == ucTransparent) {
        // DO NOTHING. 
        // We leave the pixel in 'lineBuffer' exactly as it was in the previous frame.
        // This effectively "shows the layer underneath."
      } else {
        // Overwrite with new color
        lineBuffer[pDraw->iX + x] = usPalette[c];
      }
    }
  } else {
    // No transparency? Just overwrite the whole line.
    for (x = 0; x < iWidth; x++) {
      lineBuffer[pDraw->iX + x] = usPalette[*s++];
    }
  }

  // 3. Send the updated RAM buffer line to the screen
  // We draw the *entire* line from memory, which now contains 
  // a mix of new pixels and "remembered" pixels.
  gfx->draw16bitRGBBitmap(pDraw->iX + x_offset, y + y_offset, lineBuffer, iWidth, 1);
}

void setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  gfx->begin(20000000); 
  
  // Clear screen and Buffer to Black
  gfx->fillScreen(BLACK);
  memset(frameBuffer, 0, sizeof(frameBuffer)); // Wipe memory clean

  if (!SPIFFS.begin(false)) {
    return;
  }
  
  gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop() {
  // Try to open whatever file we have
  if (gif.open("/eye.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    while (gif.playFrame(true, NULL)) {
       // Loop play
    }
    gif.close();
  } else if (gif.open("/sauron.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
     while (gif.playFrame(true, NULL)) {}
     gif.close();
  }
  
  // Note: We don't clear the screen between loops anymore!
  // This prevents the "Flash to Black" when the video loops.
}
