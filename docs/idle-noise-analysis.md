# Idle current noise (PowerScope, WiFi hotspot on, no load)

Source: serial log of about 184 consecutive readings with nothing plugged in (first part of a longer capture).

| Quantity | Result |
|---|---|
| Irms, quiet periods (147 readings) | mean 0.194 A, sd 0.009, range 0.166 to 0.211 |
| Irms, bursts (37 readings) | mean 0.228 A, sd 0.007, max 0.244 |
| Real power | mean -0.9 W, sd 2.8 W: zero on average, so noise, not load |
| Power factor / phase | 0.0 to 0.1 / 85 to 90 degrees: uncorrelated with voltage |
| Crest factor | about 5.2 (a sine is 1.41): spiky noise |
| Energy counter | adds positive power only, so about 0.8 W of phantom power, roughly 19 Wh a day |

Findings
- It is noise, not an offset. It adds in quadrature, so the fix is I = sqrt(Iraw^2 - Inoise^2) with Inoise = 0.195 A, plus a "no load" gate at 1.35 x Inoise = 0.263 A (above the highest idle reading).
- Maths check: true 0.39 A reads 0.436 A raw and 0.390 A corrected; a true 0.20 A reads 0.279 A raw and 0.200 A corrected.
- Bursts of a few tens of readings, about 15 % higher, probably coincide with WiFi activity. Before the hotspot was on, the same channel showed about 13 mV p-p, which is roughly 0.03 A rms (an estimate from p-p), so the radio is the likely main cause.
- Repeated identical lines in the log are most likely windows dropped by the 196 to 204 ms timing guard.
- Consequence: loads below about 0.17 to 0.2 A (roughly 40 W) cannot be read reliably with the 20 A sensor and WiFi on.

Mitigation, in order: decoupling at the ACS712 supply (10 uF + 100 nF), keep the sensor and its wires away from the antenna, ACS712-5A module (185 mV/A, about 1.85 times less noise in amps), median-of-3 ADC reads (untested, check the window timing first).
Re-measure on the device with the load unplugged: Serial `Z` or `POST /zero`. The value is stored in flash as "inoise".
