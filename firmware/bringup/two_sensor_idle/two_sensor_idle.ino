// ESP32: ZMPT101B on GPIO34 and ACS712 on GPIO35, Serial Monitor at 115200 baud.
// Each sensor OUT -> 10k -> node -> GPIO, node -> 20k -> GND, node -> 100nF -> GND.
// ACS712 IP+ / IP- stay empty for this test (zero current).

const int      PIN_V  = 34;
const int      PIN_I  = 35;
const int      N      = 1600;     // samples per window
const uint32_t TS_US  = 125;      // 8 kHz, so 1600 samples = 200 ms = 10 cycles at 50 Hz
float          V_GAIN = 1.0f;     // your calibration: 1.00

uint16_t vr[N], ir[N];

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  delay(500);
  Serial.println("Two sensor test. V on GPIO34, I on GPIO35.");
}

void loop() {
  uint32_t start = micros();
  uint32_t t = start;
  for (int k = 0; k < N; k++) {
    vr[k] = analogReadMilliVolts(PIN_V);
    ir[k] = analogReadMilliVolts(PIN_I);
    t += TS_US;
    while ((int32_t)(micros() - t) < 0) { }
  }
  float elapsedUs = (float)(micros() - start);

  // ---- voltage channel ----
  double vm = 0;
  int vmn = 9999, vmx = 0;
  for (int k = 0; k < N; k++) {
    vm += vr[k];
    if (vr[k] < vmn) vmn = vr[k];
    if (vr[k] > vmx) vmx = vr[k];
  }
  vm /= N;
  double s2 = 0;
  for (int k = 0; k < N; k++) {
    double d = vr[k] - vm;
    s2 += d * d;
  }
  float rawRms = sqrt(s2 / N);
  if (rawRms < 30) rawRms = 0;          // idle noise counts as zero
  float vrms = rawRms * V_GAIN;

  float amp = (vmx - vmn) / 2.0f;
  float hy = 0.2f * amp;
  bool armed = false;
  int crossings = 0, first = 0, last = 0;
  if (amp > 100) {                      // only with a real signal
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
  float freq = 0;
  if (crossings > 1) {
    float periodUs = (last - first) * (elapsedUs / N) / (crossings - 1);
    freq = 1e6f / periodUs;
  }

  // ---- current channel (idle stats only) ----
  double im = 0;
  int imn = 9999, imx = 0;
  for (int k = 0; k < N; k++) {
    im += ir[k];
    if (ir[k] < imn) imn = ir[k];
    if (ir[k] > imx) imx = ir[k];
  }
  im /= N;

  Serial.printf("V: %.1f V  f %.2f Hz  p-p %d mV  |  I pin: mean %.0f mV  p-p %d mV\n",
                vrms, freq, vmx - vmn, im, imx - imn);
  delay(500);
}
