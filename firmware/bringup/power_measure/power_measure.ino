// ESP32 PowerScope core: Vrms, Irms, P, S, PF, frequency. Serial Monitor at 115200 baud.
// ZMPT101B OUT -> 10k -> node -> GPIO34;  ACS712-20A OUT -> 10k -> node -> GPIO35.
// Each node: 20k to GND and 100nF to GND.
// ACS712 IP+/IP- in series with the live wire of the load. ZMPT101B L/N across live and neutral.

// ---------------- calibration ----------------
float V_GAIN = 0.946f;            // volts per mV at the pin: multimeter V / displayed V * old gain
float I_TRIM = 1.00f;             // current trim: clamp meter A / displayed A
const float DIV = 2.0f / 3.0f;    // 10k + 20k divider
const float SENS_MV_PER_A = 100;  // ACS712-20A
int   SHIFT  = 0;                 // phase alignment in samples (1 sample = 2.25 deg), tune with a bulb or kettle
int   I_SIGN = -1;                // -1: real power read negative on a plain load with this wiring
const float I_FLOOR = 0.08f;      // below this the current is shown as zero
const float V_FLOOR = 20.0f;      // below this the voltage is shown as zero

// ---------------- sampling ----------------
const int      PIN_V = 34;
const int      PIN_I = 35;
const int      N     = 1600;      // 1600 samples at 8 kHz = 200 ms = 10 cycles at 50 Hz
const uint32_t TS_US = 125;

uint16_t vr[N], ir[N];

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  delay(500);
  Serial.println("PowerScope core: V, I, P, S, PF, f");
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

  // DC midpoints
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

  const double I_PER_MV = (1.0 / (DIV * SENS_MV_PER_A)) * I_TRIM;   // amps per mV at the pin

  // RMS values
  double v2 = 0, i2 = 0;
  for (int k = 0; k < N; k++) {
    double v = (vr[k] - vm) * V_GAIN;
    double i = (ir[k] - im) * I_PER_MV;
    v2 += v * v;
    i2 += i * i;
  }
  float Vrms = sqrt(v2 / N);
  float Irms = sqrt(i2 / N);

  // Real power with the phase alignment shift
  double pSum = 0;
  int cnt = 0;
  for (int k = 0; k < N; k++) {
    int j = k + SHIFT;
    if (j < 0 || j >= N) continue;
    double v = (vr[k] - vm) * V_GAIN;
    double i = (ir[j] - im) * I_PER_MV;
    pSum += v * i;
    cnt++;
  }
  float P = I_SIGN * (float)(pSum / cnt);

  if (Vrms < V_FLOOR) Vrms = 0;
  if (Irms < I_FLOOR) { Irms = 0; P = 0; }
  float S = Vrms * Irms;
  float PF = (S > 1.0f) ? constrain(fabs(P) / S, 0.0f, 1.0f) : 0;

  // Frequency from rising crossings
  float amp = (vmx - vmn) / 2.0f;
  float hy = 0.2f * amp;
  bool armed = false;
  int crossings = 0, first = 0, last = 0;
  if (amp > 100) {
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

  Serial.printf("V %.1f V | I %.3f A | P %.1f W | S %.1f VA | PF %.2f | f %.2f Hz\n",
                Vrms, Irms, P, S, PF, freq);
  delay(500);
}
