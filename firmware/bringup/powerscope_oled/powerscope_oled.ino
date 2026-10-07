// PowerScope: ESP32 + ZMPT101B + ACS712-20A + 1.3" OLED (SH1106, I2C)
// Five pages that change every 5 seconds: Live, Power triangle, Power trend (30 s), Scope, Energy.
// Wiring: OLED VCC 3V3, GND GND, SDA GPIO21, SCL GPIO22.
//         ZMPT101B OUT -> 10k -> node -> GPIO34;  ACS712 OUT -> 10k -> node -> GPIO35.
//         Each node: 20k to GND and 100nF to GND.
// Library: U8g2 (Library Manager). Serial Monitor at 115200 shows the same numbers.

#include <Wire.h>
#include <U8g2lib.h>

// If the picture is shifted by 2 px or garbled, use this line instead:
//   U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------------- calibration ----------------
float V_GAIN = 0.946f;             // volts per mV at the pin (your calibration)
float I_TRIM = 1.00f;              // clamp meter A / displayed A
const float DIV = 2.0f / 3.0f;     // 10k + 20k divider
const float SENS_MV_PER_A = 100;   // ACS712-20A
int   SHIFT  = 0;                  // phase alignment in samples (1 sample = 2.25 deg)
int   I_SIGN = -1;                 // -1 because your real power read negative
const float I_FLOOR = 0.08f;       // below this the current shows as zero
const float V_FLOOR = 20.0f;

// ---------------- sampling ----------------
const int      PIN_V = 34;
const int      PIN_I = 35;
const int      N     = 1600;       // 1600 samples at 8 kHz = 200 ms = 10 cycles
const uint32_t TS_US = 125;

uint16_t vr[N], ir[N];
double   g_vm = 0, g_im = 0;       // DC midpoints of the last window
float    g_amp = 0;                // voltage amplitude in mV (for the scope trigger)

// ---------------- results ----------------
struct Meas { float V, I, P, Q, S, PF, f, phi; } M;

// ---------------- pages ----------------
const int      PAGES   = 5;
const uint32_t PAGE_MS = 5000;

// ---------------- history (30 s) ----------------
const int      HN = 120;           // 120 points, one every 250 ms = 30 s
float          hist[HN];
int            hHead = 0, hCount = 0;
uint32_t       lastHist = 0;

// ---------------- energy ----------------
double   kWh = 0, onSeconds = 0;
float    maxP = 0;
uint32_t lastMs = 0;

float iPerMv() { return (1.0f / (DIV * SENS_MV_PER_A)) * I_TRIM; }   // amps per mV at the pin

void measure() {
  uint32_t start = micros();
  uint32_t t = start;
  for (int k = 0; k < N; k++) {
    vr[k] = analogReadMilliVolts(PIN_V);
    ir[k] = analogReadMilliVolts(PIN_I);
    t += TS_US;
    while ((int32_t)(micros() - t) < 0) { }
  }
  float elapsedUs = (float)(micros() - start);

  double vm = 0, im = 0;
  int vmn = 9999, vmx = 0;
  for (int k = 0; k < N; k++) {
    vm += vr[k];
    im += ir[k];
    if (vr[k] < vmn) vmn = vr[k];
    if (vr[k] > vmx) vmx = vr[k];
  }
  vm /= N;
  im /= N;
  g_vm = vm;
  g_im = im;
  g_amp = (vmx - vmn) / 2.0f;

  const double IPM = iPerMv();
  double v2 = 0, i2 = 0;
  for (int k = 0; k < N; k++) {
    double v = (vr[k] - vm) * V_GAIN;
    double i = (ir[k] - im) * IPM;
    v2 += v * v;
    i2 += i * i;
  }
  float Vrms = sqrt(v2 / N);
  float Irms = sqrt(i2 / N);

  double pSum = 0;
  int cnt = 0;
  for (int k = 0; k < N; k++) {
    int j = k + SHIFT;
    if (j < 0 || j >= N) continue;
    pSum += ((vr[k] - vm) * V_GAIN) * ((ir[j] - im) * IPM);
    cnt++;
  }
  float P = I_SIGN * (float)(pSum / cnt);

  if (Vrms < V_FLOOR) Vrms = 0;
  if (Irms < I_FLOOR) { Irms = 0; P = 0; }
  float S = Vrms * Irms;
  float PF = (S > 1.0f) ? constrain(fabs(P) / S, 0.0f, 1.0f) : 0;
  float Q = sqrt(max(0.0f, S * S - P * P));
  float phi = acos(PF) * 57.29578f;            // degrees
  if (S <= 1.0f) phi = 0;

  // frequency from rising crossings
  float hy = 0.2f * g_amp;
  bool armed = false;
  int crossings = 0, first = 0, last = 0;
  if (g_amp > 100) {
    for (int k = 0; k < N; k++) {
      float d = vr[k] - vm;
      if (d < -hy) armed = true;
      else if (armed && d > hy) {
        armed = false;
        crossings++;
        if (crossings == 1) first = k;
        last = k;
      }
    }
  }
  float f = 0;
  if (crossings > 1) {
    float periodUs = (last - first) * (elapsedUs / N) / (crossings - 1);
    f = 1e6f / periodUs;
  }

  M = { Vrms, Irms, P, Q, S, PF, f, phi };
}

void updateStats() {
  uint32_t now = millis();
  if (lastMs) {
    float dt = (now - lastMs) / 1000.0f;
    if (M.P > 0) kWh += (double)M.P * dt / 3.6e6;
    if (M.I > 0) onSeconds += dt;
  }
  lastMs = now;
  if (M.P > maxP) maxP = M.P;

  if (now - lastHist >= 250) {
    lastHist = now;
    hist[hHead] = M.P;
    hHead = (hHead + 1) % HN;
    if (hCount < HN) hCount++;
  }
}

// ---------------- drawing helpers ----------------
void header(const char* title, int page) {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 9, title);
  for (int p = 0; p < PAGES; p++) {
    int x = 125 - (PAGES - 1 - p) * 8;
    if (p == page) u8g2.drawDisc(x, 4, 2);
    else u8g2.drawCircle(x, 4, 2);
  }
}

void drawRight(const char* s, int xr, int y) {
  u8g2.drawStr(xr - u8g2.getStrWidth(s), y, s);
}

float niceCeil(float v) {
  const float steps[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
  for (float s : steps) if (v <= s) return s;
  return 10000;
}

// ---------------- pages ----------------
void pageLive() {
  char b[24];
  header("LIVE", 0);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 21, "VOLTAGE V");  u8g2.drawStr(66, 21, "CURRENT A");
  u8g2.drawStr(0, 48, "POWER W");    u8g2.drawStr(66, 48, "FREQ Hz");
  u8g2.setFont(u8g2_font_helvB12_tr);
  snprintf(b, sizeof(b), "%.1f", M.V);  u8g2.drawStr(0, 36, b);
  snprintf(b, sizeof(b), "%.3f", M.I);  u8g2.drawStr(66, 36, b);
  snprintf(b, sizeof(b), "%.1f", M.P);  u8g2.drawStr(0, 63, b);
  snprintf(b, sizeof(b), "%.2f", M.f);  u8g2.drawStr(66, 63, b);
}

void pageTriangle() {
  char b[28];
  header("POWER TRIANGLE", 1);
  u8g2.setFont(u8g2_font_6x12_tr);
  snprintf(b, sizeof(b), "P %.1f W", M.P);    u8g2.drawStr(0, 23, b);
  snprintf(b, sizeof(b), "Q %.1f var", M.Q);  u8g2.drawStr(0, 36, b);
  snprintf(b, sizeof(b), "S %.1f VA", M.S);   u8g2.drawStr(0, 49, b);
  snprintf(b, sizeof(b), "PF %.2f  %.1f deg", M.PF, M.phi);
  u8g2.drawStr(0, 63, b);

  // small triangle: base is P, height is Q, long side is S
  float base = 32.0f * M.PF;
  float height = 32.0f * sin(M.phi / 57.29578f);
  int x0 = 88, y0 = 46;
  u8g2.drawLine(x0, y0, x0 + (int)base, y0);
  u8g2.drawLine(x0 + (int)base, y0, x0 + (int)base, y0 - (int)height);
  u8g2.drawLine(x0, y0, x0 + (int)base, y0 - (int)height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(x0 + (int)base / 2 - 2, y0 + 9, "P");
  u8g2.drawStr(min(x0 + (int)base + 3, 122), y0 - (int)height / 2 + 3, "Q");
}

void pageTrend() {
  char b[24];
  header("POWER 30 s", 2);
  float mx = 10;
  for (int i = 0; i < hCount; i++) if (hist[i] > mx) mx = hist[i];
  float ymax = niceCeil(mx * 1.1f);

  u8g2.setFont(u8g2_font_5x8_tr);
  snprintf(b, sizeof(b), "max %.0f W", ymax);  u8g2.drawStr(0, 19, b);
  snprintf(b, sizeof(b), "%.1f W", M.P);       drawRight(b, 127, 19);

  u8g2.drawHLine(0, 63, 128);
  int prevX = -1, prevY = 0;
  for (int i = 0; i < hCount; i++) {
    int idx = (hHead - hCount + i + HN) % HN;
    int x = (int)(i * 127.0f / (HN - 1));
    float v = max(0.0f, hist[idx]);
    int y = 62 - (int)(v / ymax * 38.0f);
    if (prevX >= 0) u8g2.drawLine(prevX, prevY, x, y);
    prevX = x;
    prevY = y;
  }
}

void pageScope() {
  char b[24];
  header("V solid I dot", 3);
  const int cy = 36, half = 19, span = 320;      // 2 cycles = 320 samples at 8 kHz
  for (int x = 0; x < 128; x += 4) u8g2.drawPixel(x, cy);

  // start on a rising voltage crossing so the picture stays still
  int k0 = 0;
  if (g_amp > 100) {
    float hy = 0.2f * g_amp;
    bool armed = false;
    for (int k = 1; k < N - span - 1; k++) {
      float d = vr[k] - g_vm;
      if (d < -hy) armed = true;
      else if (armed && d > 0) { k0 = k; break; }
    }
  }

  const float ipm = iPerMv();
  float ipk = 0;
  for (int k = 0; k < span; k++) {
    float i = fabs((ir[k0 + k] - g_im) * ipm);
    if (i > ipk) ipk = i;
  }
  float iscale = half / max(ipk, 0.3f);

  int pvy = cy, piy = cy;
  for (int x = 0; x < 128; x++) {
    int j = k0 + (int)(x * (float)span / 127.0f);
    float v = (vr[j] - g_vm) * V_GAIN;
    float i = I_SIGN * (ir[j] - g_im) * ipm;
    int yv = constrain(cy - (int)(v / 350.0f * half), 14, 57);
    int yi = constrain(cy - (int)(i * iscale), 14, 57);
    if (x > 0) {
      u8g2.drawLine(x - 1, pvy, x, yv);
      if ((x / 2) % 2 == 0) u8g2.drawLine(x - 1, piy, x, yi);
    }
    pvy = yv;
    piy = yi;
  }
  u8g2.setFont(u8g2_font_5x8_tr);
  snprintf(b, sizeof(b), "Ipk %.2f A", ipk);
  drawRight(b, 127, 63);
}

void pageEnergy() {
  char b[28];
  header("ENERGY", 4);
  u8g2.setFont(u8g2_font_6x12_tr);
  snprintf(b, sizeof(b), "Energy %.4f kWh", kWh);   u8g2.drawStr(0, 23, b);
  uint32_t s = (uint32_t)onSeconds;
  snprintf(b, sizeof(b), "On %02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  u8g2.drawStr(0, 36, b);
  float avg = (onSeconds > 1) ? (float)(kWh * 3.6e6 / onSeconds) : 0;
  snprintf(b, sizeof(b), "Avg P %.1f W", avg);      u8g2.drawStr(0, 49, b);
  snprintf(b, sizeof(b), "Max P %.1f W", maxP);     u8g2.drawStr(0, 63, b);
}

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  Wire.begin(21, 22);
  u8g2.begin();
  u8g2.setBusClock(400000);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB14_tr);
  u8g2.drawStr(10, 34, "PowerScope");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(10, 52, "see where every watt goes");
  u8g2.sendBuffer();
  delay(1500);
}

void loop() {
  measure();
  updateStats();

  int page = (millis() / PAGE_MS) % PAGES;
  u8g2.clearBuffer();
  switch (page) {
    case 0: pageLive();     break;
    case 1: pageTriangle(); break;
    case 2: pageTrend();    break;
    case 3: pageScope();    break;
    default: pageEnergy();  break;
  }
  u8g2.sendBuffer();

  Serial.printf("V %.1f | I %.3f | P %.1f | Q %.1f | S %.1f | PF %.2f | %.1f deg | f %.2f | %.4f kWh\n",
                M.V, M.I, M.P, M.Q, M.S, M.PF, M.phi, M.f, kWh);
}
