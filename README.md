# **Sauron's Eye \- LOTR Pinball Mod (ESP32-S3)**

This project runs a looping, artifact-free animation of the Eye of Sauron on a round 1.28" ESP32-S3 display. It is designed to be installed as a visual mod in a *Lord of the Rings* pinball machine.

Unlike standard animated GIF sketches which are limited to small files (~1.5MB) and short loops, this project utilizes a **custom partition scheme** and a **robust Python serial uploader** to store and play massive **10.5MB+ GIF files** directly from the ESP32's flash memory

* [Link to Waveshare board](https://www.waveshare.com/esp32-s3-lcd-1.28.htm)

## Key Features

* **Massive Storage:** Unlocks 10.5MB of SPIFFS storage (up from the standard 1.4MB limit) using a custom partition table.
* **High Quality:** Supports 24fps, dithering, and long durations without crashing.
* **Smart Uploader:** Custom Python script with a handshake protocol ("Request-Response") to upload large files reliably without buffer overflows.
* **Persistent Framebuffer:** Custom C++ implementation handles GIF transparency ("Keep" disposal method) by maintaining the screen state in RAM.

## Hardware Requirements

* **Microcontroller:** ESP32-S3 with 16MB Flash (e.g., Waveshare ESP32-S3-LCD-1.28 or Spotpear).
* **Display:** GC9A01 240x240 Round LCD (Built-in).
* **Connection:** USB-C for programming/power.
 
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

## Installation Guide

### **Core & Libraries**

* **ESP32 Board Manager:** Version **2.0.17** (Do not use 3.0+ as it breaks the SPIFFS uploader).  
* **Arduino\_GFX\_Library:** Version **1.4.7** (Downgraded to work with Core 2.0.17).  
* **AnimatedGIF:** Latest version (by Larry Bank).  


### 1. Environment Setup
You will need:
* **Arduino IDE** with ESP32 Board Manager installed.
* **Python 3** installed on your computer. 
* **PySerial** library:
    ```bash
    pip install pyserial
    ```
* **FFMPEG** (to generate the GIF assets).
* **esptool.py** (Required for manual filesystem upload on Linux).
* **Arduino IDE Configruation** (That worked for me)
  * Board: ESP32S3 Dev Module
  * Upload Speed: 230400
  * USB Mode: Harware CDC and JTAG
  * USB CDC On Boot:  Disabled
  * USB Firmware MSC On Boot: Disabled
  * Upload Mode: UART0/Hardware CDC
  * CPU Frequency: 240MHz (WiFi)
  * Flash Mode: DIO 80MHz
  * Flash Size: 16MB (128Mb)
  * Partition Scheme: 8M with spiffs (3MB APP/1.5MB SPIFFS) **NOTE: Use the custom partition scheme in the partitions.csv file!!!**
  * Core Debug Level: None
  * PSRAM: Disabled
  * Arduino Runs On: Core 1
  * Events Run On: Core 1
  * Erase All Flash Before Sketch Upload: Disabled
  * JTAG Adapter: Disabled
  * Port: /dev/ttyACM0 **NOTE: you need to figure this out for your system!**
  
### 2. Custom Partition Scheme (Critical)
To fit the high-quality animation, we must override the default ESP32 memory map.

1.  Ensure the file `partitions.csv` exists in the root of the sketch folder (next to the `.ino` file).
2.  Content of `partitions.csv`:
    ```csv
    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     ,        0x5000,
    otadata,  data, ota,     ,        0x2000,
    app0,     app,  ota_0,   ,        0x400000,
    spiffs,   data, spiffs,  ,        0xB00000,
    ```
    *This allocates ~11MB to SPIFFS and 4MB to the App.*
3.  **Restart Arduino IDE** (Required for the IDE to detect the CSV).
4.  In **Tools > Partition Scheme**, the selection might look greyed out or default, but the IDE will use the local CSV during compilation.

### 3. Flashing the Firmware
0.  You may need to erase the ESP32 device prior to loading the code:
```
python3 -m esptool --chip esp32s3 --port /dev/ttyACM0 erase_flash
```
  **NOTE: replace with the correct port for your device**
1.  Open `sauron_eye_pinball.ino` in the Arduino IDE.
2.  Select your board (e.g., "ESP32S3 Dev Module") and configure (specs that worked for me are listed above).
3.  **Upload** the sketch.
 **Note:** On the very first boot, the ESP32 will hang for ~60 seconds while it formats the new 10.5MB partition. This is normal.

## Uploading the Animation

**Do NOT use the "ESP32 Sketch Data Upload" plugin in Arduino IDE.** It typically times out or fails on files larger than 2MB. Use the included Python script instead.

1.  **Prepare the GIF:** Place your `eye.gif` inside a folder named `data` inside the project directory.
2.  **Close Serial Monitor:** Ensure the Arduino Serial Monitor is CLOSED.
3.  **Run the Uploader:**
    ```bash
    python3 upload_gif.py
    ```
### How the Uploader Works
1.  **Reset:** The script toggles the DTR pin to reboot the ESP32.
2.  **Handshake:** It waits for the ESP32 to send `FILE_MISSING`.
3.  **Protocol:** * PC sends file size.
    * ESP32 acknowledges.
    * **Loop:** ESP32 requests `NEXT` -> PC sends 1024 bytes -> ESP32 writes to Flash.
    * *This prevents buffer overflows common with large files.*

## Generating the GIF

To get the best quality-to-size ratio (180x180px, 24fps, ~12 seconds), use this FFMPEG command on your source video:

```
bash
ffmpeg -i source_video.mp4 \
-ss 00:00:10 -t 12 \
-vf "crop='min(iw,ih)':'min(iw,ih)',hqdn3d=4:4:6:6,eq=contrast=1.3:brightness=-0.08,fps=24,scale=180:180:flags=lanczos,split[s0][s1];[s0]palettegen=stats_mode=diff[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3" \
-loop 0 data/eye.gif
```
### Features of this animated GIF
* Scale: 180x180 (centered on 240x240 screen).
* FPS: 24 (Smooth cinematic look).
* Dithering: Bayer Scale 3 (Prevents color banding).

## **⚠️ Troubleshooting**

* **"Mount Failed" or Long Wait**	First boot requires formatting the 10.5MB partition. Wait 60 seconds.
* **"FILE_MISSING" Loop**	The sketch is running but hasn't received data. Run python3 upload_gif.py.
* **Upload Stalls**	Ensure Arduino Serial Monitor is closed. Check USB cable.
* **Kernel Panic / Crash**	Usually caused by RAM overflow. Ensure frameBuffer is allocated correctly in setup().
* **Screen is Black	If FILE_MISSING is printing to Serial** the upload hasn't happened yet.
* **Flash ESP32 device:** To completely erase the ESP32 device put the device into BootLoader mode (hold BOOT, click RST, release BOOT) and then run:
  ```
  python3 -m esptool --chip esp32s3 --port /dev/<your ESP device> erase_flash
  ```
