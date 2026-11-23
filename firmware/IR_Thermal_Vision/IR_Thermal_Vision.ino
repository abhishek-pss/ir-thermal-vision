/*
 * IR Thermal Vision - MLX90640 Wi-Fi frame transmitter
 * Supports ESP32 DevKit V1 and ESP8266 NodeMCU (with reduced frame rate).
 * Sends 32x24 temperatures as 768 little-endian int16 values in centi-degrees C.
 * Payload size: 1536 bytes. No OLED required.
 *
 * Libraries: Adafruit MLX90640, Adafruit BusIO
 */

#include <Wire.h>
#include <Adafruit_MLX90640.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #define I2C_SDA_PIN D2
  #define I2C_SCL_PIN D1
#elif defined(ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
  #define I2C_SDA_PIN 21
  #define I2C_SCL_PIN 22
#else
  #error "Select an ESP32 or ESP8266 board in Arduino IDE."
#endif

// -------- USER CONFIGURATION --------
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// Use your computer's LAN IPv4 address, not 127.0.0.1 / localhost.
const char* FRAME_URL = "http://192.168.1.100:5000/api/frame";
// ------------------------------------

static const uint16_t PIXEL_COUNT = 32 * 24;
static const uint8_t MLX_ADDRESS = 0x33;
static const uint32_t WIFI_RETRY_MS = 10000;

Adafruit_MLX90640 mlx;
float thermalFrame[PIXEL_COUNT];
int16_t packedFrame[PIXEL_COUNT];
uint32_t lastWiFiAttempt = 0;
uint32_t sentFrames = 0;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected. ESP IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection timed out; will retry.");
  }
}

bool startThermalSensor() {
#if defined(ESP8266)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000); // conservative clock for ESP8266 stability
#else
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
#endif

  Serial.println("Initializing MLX90640 at I2C address 0x33...");
  if (!mlx.begin(MLX_ADDRESS, &Wire)) {
    Serial.println("ERROR: MLX90640 not found. Check power, SDA, SCL and sensor model.");
    return false;
  }

  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_18BIT);
#if defined(ESP8266)
  // ESP8266 has less RAM/CPU headroom; keep the sensor refresh rate low.
  mlx.setRefreshRate(MLX90640_1_HZ);
#else
  mlx.setRefreshRate(MLX90640_4_HZ);
#endif
  Serial.println("MLX90640 initialized.");
  return true;
}

bool transmitFrame() {
  if (WiFi.status() != WL_CONNECTED) return false;

  // Convert Celsius to signed centi-degrees Celsius. -32768 marks invalid pixels.
  for (uint16_t i = 0; i < PIXEL_COUNT; ++i) {
    const float value = thermalFrame[i];
    if (!isfinite(value) || value < -100.0f || value > 300.0f) {
      packedFrame[i] = (int16_t)-32768;
    } else {
      packedFrame[i] = (int16_t)lroundf(value * 100.0f);
    }
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(2500);
  http.setReuse(false);

  if (!http.begin(client, FRAME_URL)) {
    Serial.println("HTTP begin failed. Check FRAME_URL.");
    return false;
  }
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Thermal-Width", "32");
  http.addHeader("X-Thermal-Height", "24");

  uint8_t* bytes = reinterpret_cast<uint8_t*>(packedFrame);
  const int responseCode = http.POST(bytes, sizeof(packedFrame));
  http.end();

  if (responseCode >= 200 && responseCode < 300) {
    ++sentFrames;
    if ((sentFrames % 10) == 0) {
      Serial.printf("Frames sent: %lu | HTTP %d\n", (unsigned long)sentFrames, responseCode);
    }
    return true;
  }

  Serial.printf("Frame upload failed. HTTP code: %d\n", responseCode);
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=================================");
  Serial.println("       IR THERMAL VISION");
  Serial.println("=================================");

  connectWiFi();
  if (!startThermalSensor()) {
    Serial.println("Sensor startup failed. Restart after checking wiring.");
    while (true) delay(1000);
  }
  Serial.println("Ready. Sending thermal frames to the computer dashboard.");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiAttempt >= WIFI_RETRY_MS) {
      lastWiFiAttempt = millis();
      connectWiFi();
    }
    delay(100);
    return;
  }

  const int result = mlx.getFrame(thermalFrame);
  if (result != 0) {
    Serial.println("MLX90640 frame read failed; retrying.");
    delay(100);
    return;
  }

  transmitFrame();
  yield();
}
