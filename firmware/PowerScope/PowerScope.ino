/*
  PowerScope: ESP32 + ZMPT101B + ACS712-20A + 1.3" OLED + WiFi dashboard
  =====================================================================
  Measures Vrms, Irms, P, Q, S, power factor, phase angle, crest factor,
  frequency and energy. Shows them on the OLED (six pages, 5 s each) and
  hosts its own WiFi hotspot with a live dashboard:

      WiFi  "PowerScope"   password  "powerscope"      ->  http://192.168.4.1

  WIRING
      ZMPT101B  VCC -> VIN (5V), GND -> GND, OUT -> 10k -> node -> GPIO34
      ACS712    VCC -> VIN (5V), GND -> GND, OUT -> 10k -> node -> GPIO35
                (each node: 20k to GND and 100nF to GND)
                IP+ / IP- in series with the LIVE wire of the load
      ZMPT101B  L / N across live and neutral (after the MCB)
      OLED 1.3" VCC -> 3V3, GND -> GND, SDA -> GPIO21, SCL -> GPIO22

  ARDUINO IDE
      Board:             "ESP32 Dev Module"  (esp32 by Espressif)
      Partition Scheme:  "Huge APP (3MB No OTA/1MB SPIFFS)"
      Libraries:         ESPAsyncWebServer and AsyncTCP (ESP32Async, GitHub -> Download ZIP
                         -> Sketch -> Include Library -> Add .ZIP Library), and U8g2 (Library Manager)
      Files:             PowerScope.ino and web_index.h must sit in a folder named PowerScope
      Serial Monitor:    115200 baud

  The page is app/index.html. After editing it, run  python tools/embed_web.py  to
  rebuild web_index.h, then upload again.

  HTTP API used by the page
      GET  /                 the dashboard (gzip)
      GET  /data             live values (JSON)
      GET  /history?r=SEC    trend arrays for 60, 600, 3600 or 21600 s
      GET  /scope            two mains cycles of voltage and current
      POST /rate?x=7.5       tariff rate in rupees per kWh (saved in flash)
      GET  /cal              calibration values, their defaults and the state of the last zeroing
      POST /unlock?pin=      check the PIN (the page asks for it before showing calibration)
      POST /cal?pin=&...     change calibration: vref= (multimeter volts), iref= (clamp meter amps),
                             vgain=, itrim=, shift=, isign=, or defaults=1
      POST /zero?pin=        re-measure the idle current noise (load unplugged)
      POST /pin?pin=&new=    change the 4-digit PIN (default 1234)

  CALIBRATION
      V_GAIN, I_TRIM, SHIFT, I_SIGN and I_NOISE live in flash and are set from the Calibrate section of the
      page. The values below are only the defaults (used on a fresh board and by "Restore defaults").
      Five wrong PINs in a row lock calibration for 60 s.

  IDLE NOISE
      The current sensor shows about 0.19 A rms of random noise even with no load. It adds to a real
      current in quadrature, so it is removed as  I = sqrt(Imeasured^2 - Inoise^2), and a reading
      close to the noise (below 1.35 x Inoise) is treated as "no load" (I, P, Q, S, PF = 0).
      To re-measure it: unplug the load, then send  Z  in the Serial Monitor (or POST /zero).
*/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "web_index.h"

#define FW_VERSION "1.2.0"

// ======================= CONFIG =======================================
const char *WIFI_SSID = "PowerScope";
const char *WIFI_PASS = "powerscope";   // WPA2: at least 8 characters

// If the picture is shifted by 2 px or garbled, use this line instead:
//   U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// calibration defaults (the live values are in flash, set from the Calibrate section of the page)
const float V_GAIN_DEF  = 0.946f;   // volts per mV at the pin
const float I_TRIM_DEF  = 1.00f;    // clamp meter A / displayed A
const int   SHIFT_DEF   = 0;        // phase alignment in samples (1 sample = 2.25 deg)
const int   I_SIGN_DEF  = -1;       // -1 because real power read negative on a plain load
const float I_NOISE_DEF = 0.195f;   // idle noise in A rms, measured from your idle log (0.194 quiet, 0.228 in bursts)
const char *PIN_DEF     = "1234";
const float DIV = 2.0f / 3.0f;      // 10k + 20k divider
const float SENS_MV_PER_A = 100;    // ACS712-20A
const float I_FLOOR = 0.08f;        // below this the current shows as zero
const float V_FLOOR = 20.0f;
const float GATE_FACTOR = 1.35f;    // readings below I_NOISE x this are "no load"
const int   SHIFT_MAX = 20;         // +-45 degrees

float V_GAIN  = V_GAIN_DEF;
float I_TRIM  = I_TRIM_DEF;
int   SHIFT   = SHIFT_DEF;
int   I_SIGN  = I_SIGN_DEF;
float I_NOISE = I_NOISE_DEF;

// pins and sampling
const int      PIN_V = 34;
const int      PIN_I = 35;
const int      N     = 1600;        // 1600 samples at 8 kHz = 200 ms = 10 cycles at 50 Hz
const uint32_t TS_US = 125;
const int      SPAN  = 320;         // two cycles for the scope
// ======================================================================

AsyncWebServer server(80);
Preferences prefs;
DNSServer dns;

uint16_t vr[N], ir[N];
double   g_vm = 0, g_im = 0;
float    g_amp = 0;
float    scopeV[SPAN], scopeI[SPAN];     // volts and amps, two cycles from the last good window

struct Meas { float V, I, P, Q, S, PF, f, phi, cf; };
Meas M = {};
float g_rawI = 0, g_rawV = 0, g_rawPF = 0;     // before the noise correction (used when zeroing)
volatile bool zeroRequest = false;
volatile int  zeroState = 0;                   // 0 never run, 1 running, 2 done, 3 refused
const char   *zeroMsg = "";

// ---------------- calibration changes from the page (applied in loop) ----------------
struct Cal { float vgain, itrim, inoise; int shift, isign; };
Cal           calNext;
volatile bool calPending = false;
char          pinCode[5] = "1234";
int           pinFails = 0;
uint32_t      pinLockUntil = 0;

// ---------------- energy (kept in flash) ----------------
double   kWh = 0, onSeconds = 0;
float    maxP = 0;
float    rateRs = 7.0f;
uint32_t lastMs = 0, lastSave = 0;
bool     dirty = false;

// ---------------- history rings (for the web page) ----------------
#define R1_CAP 600      // 1 s steps, 10 minutes
#define R2_CAP 720      // 5 s steps, 1 hour
#define R3_CAP 720      // 30 s steps, 6 hours
struct Ring { uint16_t *v; uint16_t *i; int16_t *p; int cap; int dt; volatile int head; volatile int count; };
static uint16_t a1v[R1_CAP], a1i[R1_CAP]; static int16_t a1p[R1_CAP];
static uint16_t a2v[R2_CAP], a2i[R2_CAP]; static int16_t a2p[R2_CAP];
static uint16_t a3v[R3_CAP], a3i[R3_CAP]; static int16_t a3p[R3_CAP];
Ring ring1 = { a1v, a1i, a1p, R1_CAP, 1, 0, 0 };
Ring ring5 = { a2v, a2i, a2p, R2_CAP, 5, 0, 0 };
Ring ring30 = { a3v, a3i, a3p, R3_CAP, 30, 0, 0 };

struct Acc { double v = 0, i = 0, p = 0; int n = 0; uint32_t t0 = 0; };
Acc acc1, acc5, acc30;

void ringPush(Ring &r, float v, float i, float p) {
  int h = r.head;
  r.v[h] = (uint16_t)constrain((int)(v * 10.0f + 0.5f), 0, 65535);
  r.i[h] = (uint16_t)constrain((int)(i * 1000.0f + 0.5f), 0, 65535);
  r.p[h] = (int16_t)constrain((int)(p * 10.0f), -32768, 32767);
  r.head = (h + 1) % r.cap;
  if (r.count < r.cap) r.count++;
}
void accStep(Acc &a, Ring &r, uint32_t now) {
  a.v += M.V; a.i += M.I; a.p += M.P; a.n++;
  if (now - a.t0 >= (uint32_t)r.dt * 1000UL) {
    if (a.n) ringPush(r, a.v / a.n, a.i / a.n, a.p / a.n);
    a = Acc();
    a.t0 = now;
  }
}

// ---------------- OLED pages ----------------
const int      PAGES   = 6;
const uint32_t PAGE_MS = 5000;
const int      HN = 120;                 // OLED trend: 120 points, one per 250 ms = 30 s
float          ohist[HN];
int            oHead = 0, oCount = 0;
uint32_t       lastO = 0;

float iPerMv() { return (1.0f / (DIV * SENS_MV_PER_A)) * I_TRIM; }

bool measure() {
  uint32_t start = micros();
  uint32_t t = start;
  for (int k = 0; k < N; k++) {
    vr[k] = analogReadMilliVolts(PIN_V);
    ir[k] = analogReadMilliVolts(PIN_I);
    t += TS_US;
    while ((int32_t)(micros() - t) < 0) { }
  }
  float elapsedUs = (float)(micros() - start);
  // a WiFi interrupt can stretch the window; skip it rather than show a wrong value
  if (elapsedUs < 196000.0f || elapsedUs > 204000.0f) return false;

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
  double v2 = 0, i2 = 0, ipk = 0;
  for (int k = 0; k < N; k++) {
    double v = (vr[k] - vm) * V_GAIN;
    double i = (ir[k] - im) * IPM;
    v2 += v * v;
    i2 += i * i;
    if (fabs(i) > ipk) ipk = fabs(i);
  }
  float Vrms = sqrt(v2 / N);
  float IrmsRaw = sqrt(i2 / N);

  double pSum = 0;
  int cnt = 0;
  for (int k = 0; k < N; k++) {
    int j = k + SHIFT;
    if (j < 0 || j >= N) continue;
    pSum += ((vr[k] - vm) * V_GAIN) * ((ir[j] - im) * IPM);
    cnt++;
  }
  float Praw = I_SIGN * (float)(pSum / cnt);

  if (Vrms < V_FLOOR) Vrms = 0;
  g_rawI = IrmsRaw;
  g_rawV = Vrms;
  g_rawPF = (Vrms * IrmsRaw > 1.0f) ? fabs(Praw) / (Vrms * IrmsRaw) : 0;

  // The idle noise adds to a real current in quadrature: remove it, and call anything near it "no load".
  float gate = max(I_FLOOR, I_NOISE * GATE_FACTOR);
  float Irms = 0, P = 0, cf = 0;
  if (IrmsRaw >= gate) {
    Irms = sqrt(max(0.0f, IrmsRaw * IrmsRaw - I_NOISE * I_NOISE));
    P = Praw;
    cf = (float)(ipk / IrmsRaw);
  }
  float S = Vrms * Irms;
  float PF = (S > 1.0f) ? constrain(fabs(P) / S, 0.0f, 1.0f) : 0;
  float Q = sqrt(max(0.0f, S * S - P * P));
  float phi = (S > 1.0f) ? acos(PF) * 57.29578f : 0;

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

  // two cycles, starting on a rising voltage crossing, for the scope views
  int k0 = 0;
  if (g_amp > 100) {
    bool arm = false;
    for (int k = 1; k < N - SPAN - 1; k++) {
      float d = vr[k] - vm;
      if (d < -hy) arm = true;
      else if (arm && d > 0) { k0 = k; break; }
    }
  }
  for (int k = 0; k < SPAN; k++) {
    scopeV[k] = (vr[k0 + k] - vm) * V_GAIN;
    scopeI[k] = (Irms > 0) ? I_SIGN * (ir[k0 + k] - im) * IPM : 0;   // flat line when there is no load
  }

  Meas m = { Vrms, Irms, P, Q, S, PF, f, phi, cf };
  if (!isfinite(m.V + m.I + m.P + m.Q + m.S + m.PF + m.f + m.phi + m.cf)) return false;
  M = m;
  return true;
}

// Measures the idle noise with the load unplugged and stores it. Refuses if a real load or no mains is seen.
bool calibrateZero() {
  Serial.println("[zero] measuring idle noise, keep the load unplugged...");
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB14_tr);
  u8g2.drawStr(8, 30, "Zeroing...");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(8, 50, "unplug the load");
  u8g2.sendBuffer();
  double sumI = 0, sumPF = 0, sumV = 0;
  int ok = 0;
  for (int w = 0; w < 40 && ok < 20; w++) {
    if (measure()) { sumI += g_rawI; sumPF += g_rawPF; sumV += g_rawV; ok++; }
  }
  zeroState = 3;
  if (ok < 15) { zeroMsg = "Too few clean readings. Try again."; Serial.println("[zero] too few clean windows, try again"); return false; }
  float mi = sumI / ok, mpf = sumPF / ok, mv = sumV / ok;
  if (mv < 100) { zeroMsg = "No mains voltage seen. Nothing changed."; Serial.println("[zero] no mains voltage seen, nothing changed"); return false; }
  if (mi > 0.30f || mpf > 0.35f) {
    zeroMsg = "A load seems to be connected. Unplug it and try again.";
    Serial.printf("[zero] a load seems to be connected (I %.3f A, PF %.2f), nothing changed\n", mi, mpf);
    return false;
  }
  I_NOISE = mi;
  prefs.putFloat("inoise", mi);
  zeroState = 2;
  zeroMsg = "Done. Idle noise saved.";
  Serial.printf("[zero] idle noise set to %.3f A rms (gate %.3f A), saved\n", mi, max(I_FLOOR, mi * GATE_FACTOR));
  return true;
}

// ---------------- calibration ----------------
Cal liveCal() { return Cal{ V_GAIN, I_TRIM, I_NOISE, SHIFT, I_SIGN }; }
Cal defaultCal() { return Cal{ V_GAIN_DEF, I_TRIM_DEF, I_NOISE_DEF, SHIFT_DEF, I_SIGN_DEF }; }

// Checks a PIN typed on the page. Returns nullptr if it is right, otherwise the message to show.
const char *pinCheck(const char *given) {
  if (pinLockUntil) {
    if ((int32_t)(millis() - pinLockUntil) < 0) return "Too many wrong tries. Wait a minute.";
    pinLockUntil = 0;
  }
  if (given && strcmp(given, pinCode) == 0) { pinFails = 0; return nullptr; }
  if (++pinFails >= 5) { pinFails = 0; pinLockUntil = (millis() + 60000UL) | 1; return "Too many wrong tries. Wait a minute."; }
  return "Wrong PIN";
}

bool validPin(const char *s) {
  if (!s || strlen(s) != 4) return false;
  for (int k = 0; k < 4; k++) if (s[k] < '0' || s[k] > '9') return false;
  return true;
}

// The calibration requests from the page. Each takes the live values in c, changes them and returns
// nullptr, or returns the message to show and leaves c alone.
const char *calVoltRef(Cal &c, float meterV) {
  if (M.V < 100) return "No mains voltage seen";
  if (!(meterV >= 150 && meterV <= 300)) return "Enter the multimeter reading in volts, such as 231";
  float g = c.vgain * meterV / M.V;
  if (!(g >= 0.5f && g <= 2.0f)) return "That is too far from the reading. Check the meter and the wiring.";
  c.vgain = g;
  return nullptr;
}
const char *calCurrentRef(Cal &c, float meterA) {
  if (M.I < 0.5f) return "Connect a kettle, iron or heater first (at least 0.5 A)";
  if (!(meterA >= 0.1f && meterA <= 10)) return "Enter the clamp meter reading in amps, such as 4.2";
  float k = meterA / M.I;
  if (!(c.itrim * k >= 0.5f && c.itrim * k <= 2.0f)) return "That is too far from the reading. Check the meter and the wiring.";
  c.itrim *= k;
  c.inoise *= k;   // the noise was measured in amps with the old trim
  return nullptr;
}
const char *calVoltGain(Cal &c, float g) {
  if (!(g >= 0.5f && g <= 2.0f)) return "The voltage gain must be between 0.5 and 2.0";
  c.vgain = g;
  return nullptr;
}
const char *calCurrentTrim(Cal &c, float t) {
  if (!(t >= 0.5f && t <= 2.0f)) return "The current trim must be between 0.5 and 2.0";
  c.inoise *= t / c.itrim;
  c.itrim = t;
  return nullptr;
}
const char *calShift(Cal &c, int sh) {
  if (sh < -SHIFT_MAX || sh > SHIFT_MAX) return "The phase shift must be between -20 and 20";
  c.shift = sh;
  return nullptr;
}
const char *calSign(Cal &c, int sg) {
  if (sg != 1 && sg != -1) return "The current direction must be 1 or -1";
  c.isign = sg;
  return nullptr;
}

void applyCal(const Cal &c) {
  V_GAIN = c.vgain; I_TRIM = c.itrim; I_NOISE = c.inoise; SHIFT = c.shift; I_SIGN = c.isign;
  prefs.putFloat("vgain", V_GAIN);
  prefs.putFloat("itrim", I_TRIM);
  prefs.putFloat("inoise", I_NOISE);
  prefs.putInt("shift", SHIFT);
  prefs.putInt("isign", I_SIGN);
  Serial.printf("[cal] V_GAIN %.4f  I_TRIM %.4f  I_NOISE %.3f  SHIFT %d  I_SIGN %d, saved\n", V_GAIN, I_TRIM, I_NOISE, SHIFT, I_SIGN);
}

void saveEnergy() {
  prefs.putFloat("kwh", (float)kWh);
  prefs.putFloat("on", (float)onSeconds);
  prefs.putFloat("max", maxP);
  dirty = false;
}

void updateStats() {
  uint32_t now = millis();
  if (lastMs) {
    float dt = (now - lastMs) / 1000.0f;
    if (M.P > 0) { kWh += (double)M.P * dt / 3.6e6; dirty = true; }
    if (M.I > 0) onSeconds += dt;
  }
  lastMs = now;
  if (M.P > maxP) maxP = M.P;

  if (now - lastO >= 250) {
    lastO = now;
    ohist[oHead] = M.P;
    oHead = (oHead + 1) % HN;
    if (oCount < HN) oCount++;
  }
  accStep(acc1, ring1, now);
  accStep(acc5, ring5, now);
  accStep(acc30, ring30, now);

  if (dirty && now - lastSave > 600000UL) {   // keep flash writes to once per 10 minutes
    lastSave = now;
    saveEnergy();
  }
}

// ---------------- drawing helpers ----------------
void header(const char *title, int page) {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 9, title);
  for (int p = 0; p < PAGES; p++) {
    int x = 125 - (PAGES - 1 - p) * 7;
    if (p == page) u8g2.drawDisc(x, 4, 2);
    else u8g2.drawCircle(x, 4, 2);
  }
}
void drawRight(const char *s, int xr, int y) { u8g2.drawStr(xr - u8g2.getStrWidth(s), y, s); }
float niceCeil(float v) {
  const float steps[] = { 10, 20, 50, 100, 200, 500, 1000, 2000, 5000 };
  for (float s : steps) if (v <= s) return s;
  return 10000;
}

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
  for (int i = 0; i < oCount; i++) if (ohist[i] > mx) mx = ohist[i];
  float ymax = niceCeil(mx * 1.1f);
  u8g2.setFont(u8g2_font_5x8_tr);
  snprintf(b, sizeof(b), "max %.0f W", ymax);  u8g2.drawStr(0, 19, b);
  snprintf(b, sizeof(b), "%.1f W", M.P);       drawRight(b, 127, 19);
  u8g2.drawHLine(0, 63, 128);
  int prevX = -1, prevY = 0;
  for (int i = 0; i < oCount; i++) {
    int idx = (oHead - oCount + i + HN) % HN;
    int x = (int)(i * 127.0f / (HN - 1));
    float v = max(0.0f, ohist[idx]);
    int y = 62 - (int)(v / ymax * 38.0f);
    if (prevX >= 0) u8g2.drawLine(prevX, prevY, x, y);
    prevX = x;
    prevY = y;
  }
}

void pageScope() {
  char b[24];
  header("V solid I dot", 3);
  const int cy = 36, half = 19;
  for (int x = 0; x < 128; x += 4) u8g2.drawPixel(x, cy);
  float ipk = 0;
  for (int k = 0; k < SPAN; k++) if (fabs(scopeI[k]) > ipk) ipk = fabs(scopeI[k]);
  float iscale = half / max(ipk, 0.3f);
  int pvy = cy, piy = cy;
  for (int x = 0; x < 128; x++) {
    int j = (int)(x * (SPAN - 1) / 127.0f);
    int yv = constrain(cy - (int)(scopeV[j] / 350.0f * half), 14, 57);
    int yi = constrain(cy - (int)(scopeI[j] * iscale), 14, 57);
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

void pageConnect() {
  char b[28];
  header("WIFI", 5);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(0, 23, "Join WiFi:");
  u8g2.drawStr(0, 36, WIFI_SSID);
  u8g2.drawStr(0, 49, "Open 192.168.4.1");
  snprintf(b, sizeof(b), "Clients: %d", WiFi.softAPgetStationNum());
  u8g2.drawStr(0, 63, b);
}

// ---------------- web server ----------------
static void sendOk(AsyncWebServerRequest *req) {
  req->send(200, "application/json", "{\"ok\":1}");
}
static void sendErr(AsyncWebServerRequest *req, const char *msg) {
  char b[160];
  snprintf(b, sizeof(b), "{\"ok\":0,\"err\":\"%s\"}", msg);
  req->send(200, "application/json", b);
}
static const char *reqPin(AsyncWebServerRequest *req) {
  return pinCheck(req->hasParam("pin") ? req->getParam("pin")->value().c_str() : "");
}
static int calJson(char *b, size_t n, const char *head, const Cal &c) {
  Cal d = defaultCal();
  return snprintf(b, n,
                  "{%s\"vgain\":%.4f,\"itrim\":%.4f,\"inoise\":%.3f,\"shift\":%d,\"isign\":%d,\"zero\":%d,\"zmsg\":\"%s\","
                  "\"def\":{\"vgain\":%.4f,\"itrim\":%.4f,\"inoise\":%.3f,\"shift\":%d,\"isign\":%d}}",
                  head, c.vgain, c.itrim, c.inoise, c.shift, c.isign, (int)zeroState, zeroMsg,
                  d.vgain, d.itrim, d.inoise, d.shift, d.isign);
}

void dnsTask(void *) {
  for (;;) {
    dns.processNextRequest();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void setupWeb() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS, 6, 0, 4);
  WiFi.setSleep(false);   // modem sleep adds 100+ ms of latency
  // Phones probe e.g. connectivitycheck.gstatic.com/generate_204 and, if that fails, may send the
  // page over mobile data instead. Resolve every name to us and answer the probes as "internet OK".
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  xTaskCreatePinnedToCore(dnsTask, "dns", 3072, nullptr, 1, nullptr, 0);
  Serial.printf("[WiFi] AP '%s' (password %s) up -> open http://%s\n", WIFI_SSID, WIFI_PASS, WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasHeader("If-None-Match") && req->header("If-None-Match") == INDEX_HTML_ETAG) {
      AsyncWebServerResponse *r = req->beginResponse(304);
      r->addHeader("ETag", INDEX_HTML_ETAG);
      req->send(r);
      return;
    }
    AsyncWebServerResponse *r = req->beginResponse(200, "text/html", INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
    r->addHeader("Content-Encoding", "gzip");
    r->addHeader("Cache-Control", "no-cache");
    r->addHeader("ETag", INDEX_HTML_ETAG);
    req->send(r);
  });

  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/plain", "PowerScope OK " FW_VERSION);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req) {
    Meas m = M;
    float avg = (onSeconds > 1) ? (float)(kWh * 3.6e6 / onSeconds) : 0;
    char b[400];
    snprintf(b, sizeof(b),
             "{\"v\":%.1f,\"i\":%.3f,\"p\":%.1f,\"q\":%.1f,\"s\":%.1f,\"pf\":%.3f,\"phi\":%.1f,\"f\":%.2f,\"cf\":%.2f,"
             "\"kwh\":%.5f,\"on\":%.0f,\"avg\":%.1f,\"max\":%.1f,\"up\":%lu,\"rate\":%.2f,\"inoise\":%.3f}",
             m.V, m.I, m.P, m.Q, m.S, m.PF, m.phi, m.f, m.cf, kWh, onSeconds, avg, maxP,
             (unsigned long)(millis() / 1000), rateRs, I_NOISE);
    AsyncWebServerResponse *r = req->beginResponse(200, "application/json", b);
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });

  server.on("/history", HTTP_GET, [](AsyncWebServerRequest *req) {
    int rq = req->hasParam("r") ? req->getParam("r")->value().toInt() : 600;
    Ring *R = &ring1;
    int want = 600;
    if (rq <= 60) want = 60;
    else if (rq <= 600) want = 600;
    else if (rq <= 3600) { R = &ring5; want = 720; }
    else { R = &ring30; want = 720; }
    int cnt = min((int)R->count, want);
    int head = R->head;
    AsyncResponseStream *r = req->beginResponseStream("application/json");
    r->addHeader("Cache-Control", "no-store");
    r->printf("{\"dt\":%d,\"v\":[", R->dt);
    for (int k = 0; k < cnt; k++) {
      int idx = (head - cnt + k + R->cap) % R->cap;
      if (k) r->print(',');
      r->printf("%.1f", R->v[idx] / 10.0f);
    }
    r->print("],\"i\":[");
    for (int k = 0; k < cnt; k++) {
      int idx = (head - cnt + k + R->cap) % R->cap;
      if (k) r->print(',');
      r->printf("%.3f", R->i[idx] / 1000.0f);
    }
    r->print("],\"p\":[");
    for (int k = 0; k < cnt; k++) {
      int idx = (head - cnt + k + R->cap) % R->cap;
      if (k) r->print(',');
      r->printf("%.1f", R->p[idx] / 10.0f);
    }
    r->print("]}");
    req->send(r);
  });

  server.on("/scope", HTTP_GET, [](AsyncWebServerRequest *req) {
    AsyncResponseStream *r = req->beginResponseStream("application/json");
    r->addHeader("Cache-Control", "no-store");
    r->print("{\"v\":[");
    for (int k = 0; k < SPAN; k++) {
      if (k) r->print(',');
      r->printf("%.1f", scopeV[k]);
    }
    r->print("],\"i\":[");
    for (int k = 0; k < SPAN; k++) {
      if (k) r->print(',');
      r->printf("%.3f", scopeI[k]);
    }
    r->print("]}");
    req->send(r);
  });

  server.on("/rate", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (req->hasParam("x")) {
      float x = req->getParam("x")->value().toFloat();
      if (x >= 0 && x < 1000) {
        rateRs = x;
        prefs.putFloat("rate", x);
      }
    }
    sendOk(req);
  });

  server.on("/cal", HTTP_GET, [](AsyncWebServerRequest *req) {
    char b[400];
    calJson(b, sizeof(b), "", calPending ? calNext : liveCal());
    AsyncWebServerResponse *r = req->beginResponse(200, "application/json", b);
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });

  server.on("/unlock", HTTP_POST, [](AsyncWebServerRequest *req) {
    const char *e = reqPin(req);
    if (e) sendErr(req, e); else sendOk(req);
  });

  server.on("/cal", HTTP_POST, [](AsyncWebServerRequest *req) {
    const char *e = reqPin(req);
    if (e) { sendErr(req, e); return; }
    Cal c = liveCal();
    auto num = [&](const char *k) { return req->getParam(k)->value().toFloat(); };
    if (req->hasParam("defaults")) c = defaultCal();
    else if (req->hasParam("vref"))  e = calVoltRef(c, num("vref"));
    else if (req->hasParam("iref"))  e = calCurrentRef(c, num("iref"));
    else if (req->hasParam("vgain")) e = calVoltGain(c, num("vgain"));
    else if (req->hasParam("itrim")) e = calCurrentTrim(c, num("itrim"));
    else if (req->hasParam("shift")) e = calShift(c, req->getParam("shift")->value().toInt());
    else if (req->hasParam("isign")) e = calSign(c, req->getParam("isign")->value().toInt());
    else e = "Nothing to change";
    if (e) { sendErr(req, e); return; }
    calNext = c;
    calPending = true;   // applied in loop(), between two measuring windows
    char b[400];
    calJson(b, sizeof(b), "\"ok\":1,", c);
    req->send(200, "application/json", b);
  });

  server.on("/zero", HTTP_POST, [](AsyncWebServerRequest *req) {
    const char *e = reqPin(req);
    if (e) { sendErr(req, e); return; }
    zeroState = 1;
    zeroMsg = "Measuring, keep the load unplugged...";
    zeroRequest = true;   // done in loop(), because it needs the sampling loop for a few seconds
    sendOk(req);
  });

  server.on("/pin", HTTP_POST, [](AsyncWebServerRequest *req) {
    const char *e = reqPin(req);
    if (e) { sendErr(req, e); return; }
    String nw = req->hasParam("new") ? req->getParam("new")->value() : String();
    if (!validPin(nw.c_str())) { sendErr(req, "The new PIN must be 4 digits"); return; }
    strcpy(pinCode, nw.c_str());
    prefs.putString("pin", pinCode);
    sendOk(req);
  });

  // captive-portal probes: say "internet OK" so phones stay on this WiFi
  auto online204 = [](AsyncWebServerRequest *req) { req->send(204); };
  server.on("/generate_204", HTTP_GET, online204);   // Android
  server.on("/gen_204", HTTP_GET, online204);        // Android / Chrome
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/plain", "Microsoft Connect Test");
  });
  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/plain", "Microsoft NCSI");
  });
  server.onNotFound([](AsyncWebServerRequest *req) {
    req->redirect("http://192.168.4.1/");
  });
  server.begin();
}

// ---------------- setup / loop ----------------
void splash(const char *line2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB14_tr);
  u8g2.drawStr(10, 34, "PowerScope");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(10, 52, line2);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  Wire.begin(21, 22);
  u8g2.begin();
  u8g2.setBusClock(400000);
  splash("starting WiFi...");
  Serial.printf("\n=== PowerScope v%s ===\n", FW_VERSION);

  prefs.begin("powerscope", false);
  kWh = prefs.getFloat("kwh", 0);
  onSeconds = prefs.getFloat("on", 0);
  maxP = prefs.getFloat("max", 0);
  rateRs = prefs.getFloat("rate", 7.0f);
  V_GAIN = prefs.getFloat("vgain", V_GAIN_DEF);
  I_TRIM = prefs.getFloat("itrim", I_TRIM_DEF);
  I_NOISE = prefs.getFloat("inoise", I_NOISE_DEF);
  SHIFT = constrain((int)prefs.getInt("shift", SHIFT_DEF), -SHIFT_MAX, SHIFT_MAX);
  I_SIGN = prefs.getInt("isign", I_SIGN_DEF) < 0 ? -1 : 1;
  String pin = prefs.getString("pin", PIN_DEF);
  strcpy(pinCode, validPin(pin.c_str()) ? pin.c_str() : PIN_DEF);
  Serial.printf("[cal] V_GAIN %.4f  I_TRIM %.4f  I_NOISE %.3f  SHIFT %d  I_SIGN %d\n", V_GAIN, I_TRIM, I_NOISE, SHIFT, I_SIGN);

  setupWeb();
  splash("see where every watt goes");
  delay(1200);
}

// Work asked for by the page or the Serial Monitor, done here between two measuring windows.
void handleRequests() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'z' || c == 'Z') zeroRequest = true;
  }
  if (zeroRequest) { zeroRequest = false; zeroState = 1; calibrateZero(); lastMs = millis(); }
  if (calPending) { Cal c = calNext; calPending = false; applyCal(c); }
}

void loop() {
  handleRequests();

  measure();       // a skipped window simply keeps the last good values
  updateStats();

  int page = (millis() / PAGE_MS) % PAGES;
  u8g2.clearBuffer();
  switch (page) {
    case 0: pageLive();     break;
    case 1: pageTriangle(); break;
    case 2: pageTrend();    break;
    case 3: pageScope();    break;
    case 4: pageEnergy();   break;
    default: pageConnect(); break;
  }
  u8g2.sendBuffer();

  Serial.printf("V %.1f | I %.3f (raw %.3f) | P %.1f | Q %.1f | S %.1f | PF %.2f | %.1f deg | CF %.2f | f %.2f | %.4f kWh | clients %d\n",
                M.V, M.I, g_rawI, M.P, M.Q, M.S, M.PF, M.phi, M.cf, M.f, kWh, WiFi.softAPgetStationNum());
}
