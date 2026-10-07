// ESP32 + ZMPT101B: Vrms and frequency, printed to the Serial Monitor (115200 baud)
// Sensor OUT -> 10k -> node -> GPIO34, node -> 20k -> GND, node -> 100nF -> GND.
//
// Calibration: leave V_GAIN = 1.0 first. The "raw rms" value is then the AC level at the pin in mV.
// With a known AC voltage on the sensor (read it on a multimeter):
//     V_GAIN = multimeter volts / raw rms mV
// Put that number below and upload again.
//
// PLOT = 1 sends the waveform to Tools > Serial Plotter instead of text.

#define PLOT 0

const int      PIN_V = 34;
const int      N     = 2000;    // samples per window
const uint32_t TS_US = 100;     // 10 kHz sampling, so 2000 samples = 200 ms = 10 cycles at 50 Hz
float          V_GAIN = 1.0f;   // volts per mV of AC at the pin

uint16_t vr[N];

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);   // input range up to about 3.1 V
  analogReadResolution(12);
  delay(500);
  Serial.println("ZMPT101B voltage measurement");
}

void loop() {
  uint32_t start = micros();
  uint32_t t = start;
  for (int k = 0; k < N; k++) {
    vr[k] = analogReadMilliVolts(PIN_V);
    t += TS_US;
    while ((int32_t)(micros() - t) < 0) { }
  }
  float elapsedUs = (float)(micros() - start);

  // DC offset (the 1.65 V midpoint) and min/max
  double mean = 0;
  int mn = 9999, mx = 0;
  for (int k = 0; k < N; k++) {
    mean += vr[k];
    if (vr[k] < mn) mn = vr[k];
    if (vr[k] > mx) mx = vr[k];
  }
  mean /= N;

#if PLOT
  for (int k = 0; k < N; k += 5) Serial.println((float)(vr[k] - mean));
  delay(50);
  return;
#endif

  // RMS of the AC part
  double s2 = 0;
  for (int k = 0; k < N; k++) {
    double d = vr[k] - mean;
    s2 += d * d;
  }
  float rawRms = sqrt(s2 / N);       // mV at the pin
  if (rawRms < 30) rawRms = 0;       // idle noise counts as zero
  float vrms   = rawRms * V_GAIN;    // volts after calibration

  // Frequency from rising crossings, with hysteresis against noise
  float amp = (mx - mn) / 2.0f;
  float hy = 0.2f * amp;
  bool armed = false;
  int crossings = 0, first = 0, last = 0;
  if (amp > 100) {                   // only if there is a real signal (idle noise is about 35 mV)
    for (int k = 0; k < N; k++) {
      float d = vr[k] - mean;
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

  Serial.printf("Vrms %.1f V   raw rms %.1f mV   mean %.0f mV   p-p %d mV   f %.2f Hz\n",
                vrms, rawRms, mean, mx - mn, freq);
  delay(500);
}
