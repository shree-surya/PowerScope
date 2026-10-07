// PowerScope bring-up tests, stages 1 to 3
//   STAGE 1 = I2C scan + 1.3" OLED text
//   STAGE 2 = ADC idle check on both sensor pins (no mains, no load)
//   STAGE 3 = ACS712-20A test with a low-voltage DC load
// Change STAGE, upload, open Serial Monitor at 115200 baud.
// Library needed: U8g2 (Library Manager).
#define STAGE 1

#include <Wire.h>
#include <U8g2lib.h>

const int   PIN_V = 34;              // ZMPT101B divider output
const int   PIN_I = 35;              // ACS712 divider output
const float DIV = 2.0f / 3.0f;       // 10k in series, 20k to GND
const float SENS_MV_PER_A = 100.0f;  // ACS712-20A

// The 1.3" OLED is usually SH1106. If the picture is garbled or shifted by 2 px,
// replace the line below with:
//   U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

struct Stat { float mean; int mn, mx; };

Stat sampleMv(int pin, int n) {
  long sum = 0;
  int mn = 9999, mx = 0;
  for (int i = 0; i < n; i++) {
    int v = analogReadMilliVolts(pin);
    sum += v;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    delayMicroseconds(200);
  }
  return { (float)sum / n, mn, mx };
}

void show(const char* a, const char* b = "", const char* c = "", const char* d = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13_tf);
  u8g2.drawStr(0, 13, a);
  u8g2.drawStr(0, 29, b);
  u8g2.drawStr(0, 45, c);
  u8g2.drawStr(0, 61, d);
  u8g2.sendBuffer();
}

void i2cScan() {
  Serial.println("I2C scan (expect 0x3C for the OLED):");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found device at 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) Serial.println("  nothing found. Check SDA=GPIO21, SCL=GPIO22, 3.3V, GND.");
}

float offsetMv = 1650;
uint32_t counter = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  Wire.begin(21, 22);
  u8g2.begin();

#if STAGE == 1
  i2cScan();
  show("PowerScope", "Stage 1: OLED OK");
#elif STAGE == 2
  show("Stage 2", "ADC idle check");
  delay(1000);
#elif STAGE == 3
  show("Stage 3: ACS712", "Keep load OFF", "Zeroing...");
  delay(3000);
  offsetMv = sampleMv(PIN_I, 2000).mean;
  Serial.printf("Zero offset at the pin: %.1f mV (expect about 1650)\n", offsetMv);
  show("Zero done", "Now switch load", "ON");
  delay(1500);
#endif
}

void loop() {
  char l1[24], l2[24], l3[24], l4[24];

#if STAGE == 1
  snprintf(l3, sizeof(l3), "up %lus", counter++);
  show("PowerScope", "Stage 1: OLED OK", l3);
  Serial.println(l3);
  delay(1000);

#elif STAGE == 2
  Stat v = sampleMv(PIN_V, 500);
  Stat i = sampleMv(PIN_I, 500);
  Serial.printf("V pin: mean %.1f mV, min %d, max %d, p-p %d\n", v.mean, v.mn, v.mx, v.mx - v.mn);
  Serial.printf("I pin: mean %.1f mV, min %d, max %d, p-p %d\n", i.mean, i.mn, i.mx, i.mx - i.mn);
  snprintf(l1, sizeof(l1), "V %.0f mV", v.mean);
  snprintf(l2, sizeof(l2), "  p-p %d", v.mx - v.mn);
  snprintf(l3, sizeof(l3), "I %.0f mV", i.mean);
  snprintf(l4, sizeof(l4), "  p-p %d", i.mx - i.mn);
  show(l1, l2, l3, l4);
  delay(1000);

#elif STAGE == 3
  Stat m = sampleMv(PIN_I, 400);
  float amps = (m.mean - offsetMv) / (DIV * SENS_MV_PER_A);
  Serial.printf("pin %.1f mV, change %.1f mV, current %.3f A, noise p-p %d mV\n",
                m.mean, m.mean - offsetMv, amps, m.mx - m.mn);
  snprintf(l1, sizeof(l1), "ACS712 DC test");
  snprintf(l2, sizeof(l2), "pin %.0f mV", m.mean);
  snprintf(l3, sizeof(l3), "I = %.3f A", amps);
  snprintf(l4, sizeof(l4), "noise %d mV", m.mx - m.mn);
  show(l1, l2, l3, l4);
  delay(500);
#endif
}
