# **Sauron's Eye \- LOTR Pinball Mod (ESP32-S3)**

This project runs a looping, artifact-free animation of the Eye of Sauron on a round 1.28" ESP32-S3 display. It is designed to be installed as a visual mod in a *Lord of the Rings* pinball machine.

## **🛠 Hardware Configuration**

**Board:** Generic/Clone ESP32-S3 1.28" Round Touch LCD  
**Variant Identified:** "Spotpear" / Type A Clone (Requires non-standard pinout)

### **Pinout Definition**

| Function | GPIO Pin | Notes |
| :---- | :---- | :---- |
| **Backlight** | **40** | (Standard Waveshare is 2\) |
| **Reset** | **12** | (Standard Waveshare is 14\) |
| **MOSI** | 11 |  |
| **SCK** | 10 |  |
| **CS** | 9 |  |
| **DC** | 8 |  |
| **MISO** | 12 | (Shared/Unused) |

## **💻 Software Environment**

This project was built on **Ubuntu Linux** using **Arduino IDE 1.8.19**. The build pipeline is version-sensitive. **Do not update libraries or cores without backing up.**

### **Core & Libraries**

* **ESP32 Board Manager:** Version **2.0.17** (Do not use 3.0+ as it breaks the SPIFFS uploader).  
* **Arduino\_GFX\_Library:** Version **1.4.7** (Downgraded to work with Core 2.0.17).  
* **AnimatedGIF:** Latest version (by Larry Bank).  
* **Tool:** esptool.py (Required for manual filesystem upload on Linux).

## **⚙️ Critical Configuration Steps**

### **1\. Partition Table Hack (The "3MB Drive")**

To fit the high-quality GIF file, the default 1.4MB partition is too small. You must modify the default partition table in the Arduino hardware folder to allocate 3MB to SPIFFS.  
File Location (Linux):  
```
~/.arduino15/packages/esp32/hardware/esp32/2.0.17/tools/partitions/default.csv
```

**Replace contents with:**  
```
# Name,   Type, SubType, Offset,  Size, Flags  
nvs,      data, nvs,     0x9000,  0x5000,  
otadata,  data, ota,     0xe000,  0x2000,  
app0,     app,  ota\_0,   0x10000, 0x300000,  
spiffs,   data, spiffs,  0x310000,0x300000,
```

*Sets SPIFFS size to 3MB (0x300000).*

### **2\. Video Processing (FFmpeg)**

The source video must be processed to reduce noise, center the image, and handle transparency specifically for the ESP32 buffer logic.  
**Command:**  
```
ffmpeg \-i input.mp4 \-ss 00:00:10 \-t 3 \-vf "crop='min(iw,ih)':'min(iw,ih)',hqdn3d=4:4:6:6,eq=contrast=1.3:brightness=-0.08,fps=20,scale=120:120:flags=lanczos,split\[s0\]\[s1\];\[s0\]palettegen=reserve\_transparent=0\[p\];\[s1\]\[p\]paletteuse=dither=bayer:bayer\_scale=5" \-loop 0 eye.gif
```

* **Crop:** Centers the square.  
* **Resolution:** 120x120 (Centered via code offset).  
* **FPS:** 20\.  
* **Cleaning:** Denoised and contrast-boosted to prevent dithering dots.  
* **Transparency:** Disabled (reserve\_transparent=0) to prevent black dots.

## **🚀 Flashing Instructions**

### **Step 1: Upload the Code**

1. Open sauron-eye-lotr-pinball.ino in Arduino IDE.  
2. Select Board: **ESP32S3 Dev Module**.  
3. **Flash Mode:** **DIO** (Critical\! QIO causes bootloops/black screen).  
4. **PSRAM:** Disabled.  
5. Upload normally.

### **Step 2: Upload the Filesystem (GIF)**

*Note: The Arduino IDE Plugin "ESP32 Sketch Data Upload" fails on Linux via native USB. We generate the bin file and flash it manually.*

1. Ensure eye.gif is in the /data folder inside the sketch directory.  
2. In Arduino IDE, click **Tools \> ESP32 Sketch Data Upload**.  
   * **Let it fail.** (We just need it to compile the .bin file).  
3. Look at the error log and copy the file path (e.g., /tmp/arduino\_build\_xxxx/sauron.spiffs.bin).  
4. Put the board in **Bootloader Mode**:  
   * Hold **BOOT**, Click **RST**, Release **BOOT**.  
5. Run this command in Terminal (using 115200 baud for safety):

```python3 \-m esptool \--chip esp32s3 \--port /dev/ttyACM0 \--baud 115200 write\_flash 0x310000 /tmp/arduino\_build\_YOUR\_BUILD\_ID/sauron-eye-lotr-pinball.spiffs.bin ```

## **🧠 Code Architecture Notes**

* **Speed Limit:** gfx-\>begin(20000000) sets SPI to 20MHz. Higher speeds (40/80MHz) cause "snow" and artifacting on this specific clone board.  
* **Frame Buffer:** The code allocates a frameBuffer\[120\*120\] in RAM.  
  * It draws the GIF to this RAM buffer first.  
  * If a pixel is transparent, it retains the previous color in the buffer (fixing "black dot" holes).  
  * It sends the completed buffer to the screen centered at offset (60,60).  
* **Autoplay:** The code searches for any .gif on the SPIFFS partition and plays it automatically.  
* **Silent Failure:** Error screens (Yellow/Red) are disabled to prevent flashing during loops.

## **⚠️ Troubleshooting**

* **Yellow Screen / Flashing:** Hardware works, but file not found or header corrupted. Re-flash filesystem at slower baud rate.  
* **Red Screen:** SPIFFS mount failed. Check partition table csv matches the flash address.  
* **Black Screen:** Wrong pinout or Flash Mode set to QIO instead of DIO.  
* **Artifacts/Snow:** SPI frequency too high. Code is set to 20MHz to fix this.
