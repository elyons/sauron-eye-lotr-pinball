import serial
import time
import os
import sys
import struct

#Purpose:  upload the files in ./data to the ESP32 after the main .ino file has been written and the ESP32 has undergone its internal formatting for the filesyste.  These are usually animated gifs


# --- CONFIGURATION ---
SERIAL_PORT = '/dev/ttyACM0'  # Change this to your port (e.g., COM3 on Windows)
BAUD_RATE = 115200
FILE_PATH = os.path.join('data', 'eye.gif') 
CHUNK_SIZE = 1024 # Must match the CHUNK_SIZE in the Arduino Sketch
# ---------------------

def upload_file():
    if not os.path.exists(FILE_PATH):
        print(f"Error: {FILE_PATH} not found.")
        print("Ensure 'eye.gif' is inside a 'data' folder next to this script.")
        return

    file_size = os.path.getsize(FILE_PATH)
    print(f"Found file: {FILE_PATH} ({file_size} bytes)")
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
        
        # --- RESET ESP32 ---
        ser.dtr = False; ser.rts = False
        print("Resetting ESP32...")
        ser.dtr = True; time.sleep(0.1); ser.dtr = False
        time.sleep(2) # Allow boot time
        
        print("Waiting for handshake...")
        ser.reset_input_buffer()
        connected = False
        
        # --- HANDSHAKE LOOP ---
        last_wait_msg = time.time()
        while not connected:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if "FILE_MISSING" in line:
                    print("Handshake: Sending Start Command...")
                    ser.write(b"START_UPLOAD\n")
                    ser.flush()
                    time.sleep(0.5)
                elif "READY_TO_RECEIVE" in line:
                    print("Handshake: Connected!")
                    connected = True
            
            # Feedback for long wait times (formatting)
            if time.time() - last_wait_msg > 5:
                print("...Still waiting for ESP32 (Formatting 10MB FS can take 60s+)...")
                last_wait_msg = time.time()
                
            time.sleep(0.01)

        # --- SEND SIZE ---
        print("Sending File Size...")
        ser.write(struct.pack('<I', file_size))
        ser.flush()
        
        # Wait for Size Ack
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if "SIZE:" in line: 
                print(f"ESP32 Confirmed Size: {line}")
                break

        print("Starting Robust Transfer...")
        
        # --- CHUNK TRANSFER LOOP ---
        with open(FILE_PATH, 'rb') as f:
            sent = 0
            while sent < file_size:
                # 1. Wait for explicit "NEXT" signal from ESP32
                # This prevents buffer overflow by ensuring ESP32 has finished writing the previous chunk
                while True:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line == "NEXT":
                        break
                    # Optional: Print other debug messages from ESP32
                    # if line: print(f"ESP32: {line}")
                
                # 2. Read and Send Chunk
                chunk = f.read(CHUNK_SIZE)
                if not chunk: break
                
                ser.write(chunk)
                ser.flush()
                sent += len(chunk)
                
                # 3. Update Progress UI
                percent = (sent / file_size) * 100
                sys.stdout.write(f"\rProgress: {percent:.1f}%")
                sys.stdout.flush()

        # --- COMPLETION ---
        print("\nWaiting for verification...")
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line == "DONE":
                print("Success! ESP32 is restarting.")
                break

    except Exception as e:
        print(f"\nError: {e}")
    finally:
        if 'ser' in locals() and ser.is_open: ser.close()

if __name__ == "__main__":
    upload_file()
