// Step 1: ESP32 + ZMPT101B, Serial Monitor only (115200 baud)
// Sensor output goes through a 10k/20k divider to GPIO34.
// No mains connected to the sensor yet.

const int PIN_V = 34;

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);   // input range up to about 3.1 V
  analogReadResolution(12);
  delay(500);
  Serial.println("Voltage sensor idle test. Expect about 1650 mV.");
}

void loop() {
  const int N = 500;
  long sum = 0;
  int mn = 9999, mx = 0;
  for (int i = 0; i < N; i++) {
    int v = analogReadMilliVolts(PIN_V);
    sum += v;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    delayMicroseconds(200);
  }
  Serial.printf("mean %.1f mV   min %d   max %d   p-p %d\n",
                (float)sum / N, mn, mx, mx - mn);
  delay(1000);
}
