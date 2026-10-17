// Single-plane fan/rotor balancing helper
// ESP32-C6 + SparkFun SEN-10724 (ADXL345 accel, I2C) + hall/reed tach on a GPIO.
//
// Prints, once per window: RPM, sample count, and for each axis the amplitude (g)
// and phase (deg) of the 1x-RPM vibration, measured relative to the tach pulse.
// Absolute phase has a constant offset (sensor latency etc.), which cancels in the
// influence-coefficient math because you only ever compare runs to each other.

#include <Wire.h>

// ---- Adjust to your wiring ----
const int SDA_PIN  = 6;      // ESP32-C6-DevKitC-1 default SDA
const int SCL_PIN  = 7;      // ESP32-C6-DevKitC-1 default SCL
const int TACH_PIN = 4;      // hall sensor / reed switch to GND, internal pull-up
const uint32_t WINDOW_MS = 5000;   // averaging window
// --------------------------------

const uint8_t ADXL_ADDR = 0x53;    // SEN-10724 default address
const float   G_PER_LSB = 0.0039f; // full-resolution mode, 3.9 mg/LSB

volatile uint32_t lastPulseUs = 0;
volatile uint32_t periodUs    = 0;
volatile uint32_t pulseCount  = 0;

void IRAM_ATTR tachISR() {
  uint32_t now = micros();
  if (now - lastPulseUs < 5000) return;   // debounce (caps at 12000 RPM)
  periodUs = now - lastPulseUs;
  lastPulseUs = now;
  pulseCount++;
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

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  uint8_t id = 0;
  adxlRead(0x00, &id, 1);
  if (id != 0xE5) {
    Serial.printf("ADXL345 not found (read 0x%02X, expected 0xE5). Check wiring.\n", id);
    while (true) delay(1000);
  }
  adxlWrite(0x31, 0x08);   // DATA_FORMAT: full resolution, +/-2 g
  adxlWrite(0x2C, 0x0D);   // BW_RATE: 800 Hz output rate
  adxlWrite(0x2D, 0x08);   // POWER_CTL: measure mode

  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);

  Serial.println("Ready. Spin the rotor at a steady speed.");
  Serial.println("rpm, samples, Ax_g, Ax_deg, Ay_g, Ay_deg, Az_g, Az_deg");
}

struct Acc {
  double sumA = 0, sumC = 0, sumS = 0, sumAC = 0, sumAS = 0;
  void add(double a, double c, double s) {
    sumA += a; sumC += c; sumS += s; sumAC += a * c; sumAS += a * s;
  }
};

void loop() {
  Acc acc[3];
  uint32_t n = 0;
  uint32_t startMs = millis();
  double rpmSum = 0;

  while (millis() - startMs < WINDOW_MS) {
    uint8_t src;
    if (!adxlRead(0x30, &src, 1) || !(src & 0x80)) continue;   // wait for DATA_READY
    uint32_t t = micros();

    uint8_t raw[6];
    if (!adxlRead(0x32, raw, 6)) continue;

    // Snapshot the tach state consistently
    uint32_t lp, per;
    do { lp = lastPulseUs; per = periodUs; } while (lp != lastPulseUs);
    if (pulseCount < 3 || per == 0) continue;

    uint32_t dt = t - lp;
    if (dt > per + per / 2) continue;                          // tach went stale

    double phi = 2.0 * PI * (double)dt / (double)per;
    double c = cos(phi), s = sin(phi);

    int16_t v[3] = {
      (int16_t)(raw[1] << 8 | raw[0]),
      (int16_t)(raw[3] << 8 | raw[2]),
      (int16_t)(raw[5] << 8 | raw[4]),
    };
    for (int i = 0; i < 3; i++) acc[i].add(v[i] * G_PER_LSB, c, s);

    rpmSum += 60e6 / per;
    n++;
  }

  if (n < 200) {
    Serial.println("Too few samples. Is the rotor spinning and the tach triggering?");
    return;
  }

  Serial.printf("%.0f, %u", rpmSum / n, (unsigned)n);
  for (int i = 0; i < 3; i++) {
    double meanA = acc[i].sumA / n;
    double re = acc[i].sumAC - meanA * acc[i].sumC;   // DC-corrected
    double im = acc[i].sumAS - meanA * acc[i].sumS;
    double amp = 2.0 / n * sqrt(re * re + im * im);
    double deg = atan2(im, re) * 180.0 / PI;
    if (deg < 0) deg += 360.0;
    Serial.printf(", %.4f, %.1f", amp, deg);
  }
  Serial.println();
}
