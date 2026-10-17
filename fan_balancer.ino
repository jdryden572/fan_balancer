// Fan/rotor vibration monitor
// ESP32-C6 + SparkFun SEN-10724 (ADXL345 accel, I2C). No tachometer.
//
// Samples the accelerometer at a fixed 800 Hz (using the ADXL345 FIFO so the
// sample spacing is uniform), then finds the dominant vibration frequency with an
// FFT. That frequency is taken as the 1x rotation speed. Once per window it reports:
//   RPM, and for each axis the amplitude (g) of the vibration at that frequency,
//   plus the Y and Z phase relative to X (deg).
// Without a tach there is no absolute rotor phase, so the phase is only useful for
// comparing the axes with each other. Amplitude is what you compare between
// trial-weight runs.
//
// Output goes to both USB serial and a BLE UART (Nordic UART Service). The C6 has
// no Classic Bluetooth, so "Bluetooth serial" has to be BLE. Connect with a BLE
// terminal app (e.g. nRF Connect, Serial Bluetooth Terminal in BLE mode) to the
// device named BLE_NAME.
//
// Arduino IDE: enable Tools > USB CDC On Boot for the USB serial output.

#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---- Adjust to your wiring / setup ----
const int SDA_PIN = 22;               // ESP32-C6-DevKitC-1 default SDA
const int BOOT_PIN = 9;               // BOOT button on the ESP32-C6-DevKitC-1 (active low)
const int SCL_PIN = 23;              // ESP32-C6-DevKitC-1 default SCL
const char *BLE_NAME = "FanBalancer";
const float MIN_HZ = 5.0f;           // search band for the rotation frequency
const float MAX_HZ = 100.0f;         // (300 - 6000 RPM)
// ---------------------------------------

const uint8_t ADXL_ADDR = 0x53;      // SEN-10724 default address
const float   G_PER_LSB = 0.0039f;   // full-resolution mode, 3.9 mg/LSB
const float   NOMINAL_FS = 800.0f;   // BW_RATE 0x0D

const int N = 4096;                  // samples per window (~5.1 s), power of 2

// NUS UUIDs
#define NUS_SERVICE_UUID "6E400001-B5A0-F393-E254-63E1E7C0B2E8"
#define NUS_RX_UUID      "6E400002-B5A0-F393-E254-63E1E7C0B2E8"
#define NUS_TX_UUID      "6E400003-B5A0-F393-E254-63E1E7C0B2E8"

BLEServer *bleServer = nullptr;
BLECharacteristic *bleTx = nullptr;
volatile bool bleConnected = false;

class ServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { bleConnected = true; }
  void onDisconnect(BLEServer *s) override {
    bleConnected = false;
    s->startAdvertising();
  }
};

// Write a string to USB serial and, if a client is connected, BLE.
void out(const char *s) {
  Serial.print(s);
  if (!bleConnected) return;
  size_t len = strlen(s);
  for (size_t i = 0; i < len; i += 20) {          // default ATT MTU payload is 20
    bleTx->setValue((uint8_t *)(s + i), min((size_t)20, len - i));
    bleTx->notify();
    delay(8);                                      // let the BLE stack drain
  }
}

void outf(const char *fmt, ...) {
  char buf[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  out(buf);
}

void adxlWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADXL_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool adxlRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(ADXL_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ADXL_ADDR, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

// ---- FFT ----
float cosTab[N / 2], sinTab[N / 2];
float hann[N];
float data[3][N];                    // DC-removed samples, g
float fr[N], fi[N];                  // FFT scratch
float power[N / 2];

void fft(float *re, float *im) {
  for (int i = 1, j = 0; i < N; i++) {             // bit reversal
    int bit = N >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
  }
  for (int len = 2; len <= N; len <<= 1) {
    int half = len >> 1, step = N / len;
    for (int i = 0; i < N; i += len) {
      for (int j = 0; j < half; j++) {
        float wr = cosTab[j * step], wi = -sinTab[j * step];
        int a = i + j, b = a + half;
        float tr = re[b] * wr - im[b] * wi;
        float ti = re[b] * wi + im[b] * wr;
        re[b] = re[a] - tr; im[b] = im[a] - ti;
        re[a] += tr;        im[a] += ti;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  for (int i = 0; i < N / 2; i++) {
    cosTab[i] = cosf(2.0f * PI * i / N);
    sinTab[i] = sinf(2.0f * PI * i / N);
  }
  for (int i = 0; i < N; i++) hann[i] = 0.5f - 0.5f * cosf(2.0f * PI * i / (N - 1));

  BLEDevice::init(BLE_NAME);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCb());
  BLEService *svc = bleServer->createService(NUS_SERVICE_UUID);
  bleTx = svc->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  bleTx->addDescriptor(new BLE2902());
  svc->createCharacteristic(NUS_RX_UUID, BLECharacteristic::PROPERTY_WRITE);
  svc->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  pinMode(BOOT_PIN, INPUT_PULLUP);

  uint8_t id = 0;
  adxlRead(0x00, &id, 1);
  if (id != 0xE5) {
    while (true) {
      outf("ADXL345 not found (read 0x%02X, expected 0xE5). Check wiring.\n", id);
      delay(2000);
    }
  }
  adxlWrite(0x31, 0x08);   // DATA_FORMAT: full resolution, +/-2 g
  adxlWrite(0x2C, 0x0D);   // BW_RATE: 800 Hz output rate
  adxlWrite(0x2D, 0x08);   // POWER_CTL: measure mode

  out("Ready. Spin the rotor at a steady speed.\n");
  out("rpm, Ax_g, Ay_g, Az_g, Y-X_deg, Z-X_deg\n");
}

// Collect N uniformly spaced samples via the FIFO. Returns the measured sample
// rate in Hz, or 0 on failure (I2C error or FIFO overflow).
float acquire() {
  adxlWrite(0x38, 0x00);          // FIFO bypass: flush
  adxlWrite(0x38, 0x80 | 0x1F);   // stream mode
  uint32_t t0 = micros();
  int n = 0;
  while (n < N) {
    checkReset();
    uint8_t st;
    if (!adxlRead(0x39, &st, 1)) return 0;
    uint8_t entries = st & 0x3F;
    if (entries >= 32) return 0;  // overflowed, samples were lost
    while (entries-- && n < N) {
      uint8_t raw[6];
      if (!adxlRead(0x32, raw, 6)) return 0;
      data[0][n] = (int16_t)(raw[1] << 8 | raw[0]) * G_PER_LSB;
      data[1][n] = (int16_t)(raw[3] << 8 | raw[2]) * G_PER_LSB;
      data[2][n] = (int16_t)(raw[5] << 8 | raw[4]) * G_PER_LSB;
      n++;
    }
  }
  uint32_t elapsed = micros() - t0;
  adxlWrite(0x38, 0x00);
  return N * 1e6f / elapsed;
}

// Restart the firmware when the BOOT button is pressed.
void checkReset() {
  if (digitalRead(BOOT_PIN) != LOW) return;
  delay(30);                                   // debounce
  if (digitalRead(BOOT_PIN) != LOW) return;
  out("Resetting...\n");
  while (digitalRead(BOOT_PIN) == LOW) delay(10);   // wait for release
  ESP.restart();
}

void loop() {
  checkReset();
  float fs = acquire();
  if (fs == 0 || fabsf(fs - NOMINAL_FS) > 0.1f * NOMINAL_FS) {
    out("Sampling error (I2C failure or FIFO overflow).\n");
    return;
  }

  // Remove DC per axis, then sum the Hann-windowed power spectra of all axes.
  for (int a = 0; a < 3; a++) {
    double m = 0;
    for (int i = 0; i < N; i++) m += data[a][i];
    float mean = m / N;
    for (int i = 0; i < N; i++) data[a][i] -= mean;
  }
  memset(power, 0, sizeof power);
  for (int a = 0; a < 3; a++) {
    for (int i = 0; i < N; i++) { fr[i] = data[a][i] * hann[i]; fi[i] = 0; }
    fft(fr, fi);
    for (int k = 0; k < N / 2; k++) power[k] += fr[k] * fr[k] + fi[k] * fi[k];
  }

  int kLo = max(2, (int)ceilf(MIN_HZ * N / fs));
  int kHi = min(N / 2 - 2, (int)floorf(MAX_HZ * N / fs));
  int kPk = kLo;
  for (int k = kLo; k <= kHi; k++) if (power[k] > power[kPk]) kPk = k;

  // Parabolic interpolation of the peak for sub-bin frequency.
  float a = sqrtf(power[kPk - 1]), b = sqrtf(power[kPk]), c = sqrtf(power[kPk + 1]);
  float denom = a - 2 * b + c;
  float delta = (denom != 0) ? 0.5f * (a - c) / denom : 0;
  float freq = (kPk + delta) * fs / N;

  // Amplitude and phase of each axis at that frequency (Hann-weighted DFT).
  double wSum = 0;
  for (int i = 0; i < N; i++) wSum += hann[i];
  double w = 2.0 * PI * freq / fs;
  double re[3] = {0, 0, 0}, im[3] = {0, 0, 0};
  for (int i = 0; i < N; i++) {
    double cs = cos(w * i), sn = sin(w * i);
    for (int ax = 0; ax < 3; ax++) {
      double v = data[ax][i] * hann[i];
      re[ax] += v * cs;
      im[ax] += v * sn;
    }
  }
  double amp[3], ph[3];
  for (int ax = 0; ax < 3; ax++) {
    amp[ax] = 2.0 * sqrt(re[ax] * re[ax] + im[ax] * im[ax]) / wSum;
    ph[ax] = atan2(-im[ax], re[ax]) * 180.0 / PI;   // x ~ A cos(wt + phase)
  }
  double dy = ph[1] - ph[0], dz = ph[2] - ph[0];
  dy = fmod(fmod(dy, 360.0) + 360.0, 360.0);
  dz = fmod(fmod(dz, 360.0) + 360.0, 360.0);

  outf("%.0f, %.4f, %.4f, %.4f, %.1f, %.1f\n", freq * 60.0f, amp[0], amp[1], amp[2], dy, dz);
}
