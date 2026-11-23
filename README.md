# IR Thermal Vision

Live MLX90640 thermal imaging from an **ESP32 DevKit V1** (recommended) or **ESP8266 NodeMCU** to a Windows/Linux/macOS computer over Wi-Fi. The computer converts each 32 × 24 temperature frame into a false-colour MJPEG stream using Python, Flask and OpenCV.

> **This project requires an MLX90640 thermal array.** An MLX90614 / DFRobot SEN0206 is a single-point infrared thermometer and cannot produce a thermal image. Do not buy the SEN0206 for this project.

## Features

- Live browser-based false-colour thermal stream (no OLED required)
- ESP32 and ESP8266 firmware in one Arduino sketch, selected at compile time
- Wi-Fi frame transfer using a compact binary protocol
- Live minimum, average and maximum temperature readings
- Hottest-pixel crosshair and stale-connection indicator
- Local dashboard; no cloud service or API key required

## Important limitations

- The MLX90640 is a **32 × 24 pixel** thermal imager. The dashboard enlarges the image with interpolation for viewing; it does not create extra thermal detail.
- The ESP32 firmware is the recommended option. The ESP8266 has less RAM and processing headroom; its firmware is deliberately configured for a slower sensor refresh rate and the stream may be slow or unreliable on some ESP8266 boards.
- The displayed crosshair identifies the hottest pixel. This starter project does **not** identify people or provide reliable human detection / bounding boxes.
- This is a thermal video-like stream made from successive sensor frames, not an ordinary RGB camera stream. The MLX90640 refresh rate and I²C transfer speed limit the frame rate.
- Temperature readings depend on sensor calibration, emissivity, distance, field of view and the target surface. Do not use this project as a certified medical or safety instrument.

## Hardware

1. ESP32 DevKit V1 **recommended**, or ESP8266 NodeMCU
2. MLX90640 32 × 24 thermal camera breakout (I²C, address normally `0x33`)
3. USB cable and computer
4. Jumper wires

No OLED, display, buzzer or separate camera is required.

### Wiring

Use the breakout board's pin labels. Power the breakout according to its own documentation; use 3.3 V logic with ESP boards. If the board has a `VIN` pin with an onboard regulator, follow its specified input range. Never connect a 5 V I²C signal to an ESP GPIO.

| MLX90640 breakout | ESP32 DevKit V1 | ESP8266 NodeMCU |
|---|---|---|
| VIN / 3V3 | 3V3 (or breakout-specified VIN) | 3V3 (or breakout-specified VIN) |
| GND | GND | GND |
| SDA | GPIO 21 | D2 / GPIO 4 |
| SCL | GPIO 22 | D1 / GPIO 5 |

Keep I²C wires short. If the sensor is not detected, check the breakout's power requirements, SDA/SCL orientation, common ground and I²C pull-ups. The firmware expects address `0x33`.

## Repository structure

```text
ir-thermal-vision/
├── firmware/
│   └── IR_Thermal_Vision/
│       └── IR_Thermal_Vision.ino
├── desktop/
│   ├── app.py
│   ├── requirements.txt
│   └── templates/
│       └── index.html
├── .gitignore
├── LICENSE
└── README.md
```

## 1. Install Arduino IDE and libraries

1. Install Arduino IDE 2.x.
2. Open **Library Manager** and install:
   - `Adafruit MLX90640`
   - `Adafruit BusIO` (usually installed as a dependency)
3. For ESP32: install **esp32 by Espressif Systems** in Boards Manager.
4. For ESP8266: add this Boards Manager URL in Preferences:

   ```text
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```

   Then install **esp8266 by ESP8266 Community** in Boards Manager.

## 2. Start the computer dashboard first

Install Python 3.10 or newer. Open a terminal in the `desktop` folder.

### Windows PowerShell

```powershell
py -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements.txt
python app.py
```

If PowerShell blocks activation, run this once in the current terminal:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.venv\Scripts\Activate.ps1
```

### Linux / macOS

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
pip install -r requirements.txt
python app.py
```

The server listens on port `5000`. Keep this terminal open.

### Find your computer's local IPv4 address

On Windows, run:

```powershell
ipconfig
```

Find the **IPv4 Address** under the Wi-Fi adapter connected to the same network as the ESP board. Example: `192.168.1.100` (your address will be different).

On Linux, use `hostname -I`. On macOS, use `ipconfig getifaddr en0` for Wi-Fi in a typical setup.

Open the dashboard on your computer at:

```text
http://127.0.0.1:5000
```

If the ESP cannot reach the server, allow Python on private networks in Windows Firewall. Both devices must be on the same LAN; guest Wi-Fi/client isolation can block device-to-device traffic.

## 3. Configure and upload the firmware

Open:

```text
firmware/IR_Thermal_Vision/IR_Thermal_Vision.ino
```

Edit the three settings near the top:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* FRAME_URL = "http://192.168.1.100:5000/api/frame";
```

Replace the Wi-Fi name, password and example IP with your own values. Keep the computer and ESP on the same Wi-Fi network. Do not commit real Wi-Fi credentials to a public GitHub repository; use placeholders before pushing.

### Upload to ESP32 DevKit V1 (recommended)

1. Connect the ESP32 by USB.
2. Select **Tools → Board → ESP32 Arduino → ESP32 Dev Module** (or the exact DevKit board available).
3. Select the correct COM port.
4. Click Upload.
5. Open Serial Monitor at `115200` baud.

ESP32 pin mapping in the sketch: SDA `GPIO 21`, SCL `GPIO 22`. Sensor refresh is configured at `4` pages/second (about 2 complete frames/second, since an MLX90640 frame has two subpages).

### Upload to ESP8266 NodeMCU (experimental / slower)

1. Connect the NodeMCU by USB.
2. Select **Tools → Board → ESP8266 Boards → NodeMCU 1.0 (ESP-12E Module)**.
3. Select the correct COM port and upload.
4. Open Serial Monitor at `115200` baud.

ESP8266 pin mapping in the sketch: SDA `D2/GPIO4`, SCL `D1/GPIO5`. The firmware selects a lower sensor refresh rate to reduce load. If the MLX90640 library fails to initialize or the board resets due to memory pressure, use the ESP32 DevKit V1 instead.

## 4. View the live thermal stream

With the Python server running and the firmware uploaded:

1. Open the Serial Monitor and wait for `Wi-Fi connected` and `MLX90640 initialized`.
2. Confirm the ESP prints its IP address.
3. Open `http://127.0.0.1:5000` in your computer browser.
4. The dashboard should change from **WAITING FOR SENSOR** to **THERMAL SENSOR CONNECTED** after the first valid frame arrives.

The thermal image is also available at `http://127.0.0.1:5000/video_feed` and server health at `http://127.0.0.1:5000/health`.

## Data protocol

The ESP sends an HTTP `POST` to `/api/frame` with `Content-Type: application/octet-stream`. The body is exactly 1,536 bytes: 768 signed 16-bit little-endian values, each representing Celsius × 100. `-32768` is reserved for an invalid pixel. The server checks the payload size before accepting it.

## Troubleshooting

| Symptom | Checks |
|---|---|
| `MLX90640 not found` | Confirm the part is MLX90640 (not MLX90614), I²C wiring, power, common GND and address `0x33`. |
| ESP connects to Wi-Fi but no frames arrive | Check `FRAME_URL` uses the computer's LAN IP, port `5000`, same Wi-Fi, and firewall permissions. Do not use `localhost` in firmware. |
| Dashboard says waiting for sensor | Check Serial Monitor for HTTP errors; open `http://127.0.0.1:5000/health` on the computer. |
| `HTTP code: -1` | Server unreachable, wrong IP/port, firewall or Wi-Fi client isolation. |
| Unstable frames / I²C errors | Shorten wires, check power and pull-ups; try the ESP32; keep the sensor refresh rate low. |
| Image looks blocky | Expected: the sensor has only 32 × 24 pixels. Upscaling smooths display but cannot add detail. |
| ESP8266 resets or fails to compile | Use ESP32 DevKit V1 for the full project; ESP8266 support is best-effort due to memory/CPU constraints. |

## GitHub upload

From the project root:

```bash
git init
git add .
git commit -m "Initial IR Thermal Vision project"
git branch -M main
git remote add origin https://github.com/YOUR-USERNAME/ir-thermal-vision.git
git push -u origin main
```

Create an empty repository on GitHub first and replace `YOUR-USERNAME` with your GitHub username. Do not upload Wi-Fi passwords, personal network details, virtual environments or compiled build files.

## License

MIT License. See [LICENSE](LICENSE).

## References

- [Adafruit MLX90640 Arduino library](https://github.com/adafruit/Adafruit_MLX90640)
- [Adafruit MLX90640 API reference](https://adafruit.github.io/Adafruit_MLX90640/html/class_adafruit___m_l_x90640.html)
- [ESP8266 Arduino core](https://github.com/esp8266/Arduino)
