# CLAUDE.md: PowerScope and GridGuard

Context for continuing this project. Read README.md first for the layout.

## What this is
Two ESP32 products for single-phase 230 V, 6 A loads, sold as student/demo builds.
PowerScope measures and shows. GridGuard measures and cuts the relay on over/under voltage and current.
Discuss design questions with the owner before generating code; they confirm scope step by step.

## Hardware (dev unit)
| Part | Pin / note |
|---|---|
| ESP32 38-pin DevKit, CP2102 | board "ESP32 Dev Module" |
| ZMPT101B OUT | 10k -> node -> GPIO34, node: 20k to GND, 100nF to GND. Powered from VIN (5 V). Trimmer left alone; module gain is 1 mV at the pin per 1 V of mains. |
| ACS712-20A OUT | same divider -> GPIO35. IP+/IP- in series with LIVE. |
| OLED 1.3" SH1106 I2C | SDA 21, SCL 22, 3V3. Final PowerScope uses a 0.96" SSD1306: swap the U8g2 constructor line. |
| Divider ratio | 2/3, so ACS712 sensitivity at the pin is 100 mV/A x 2/3 = 66.7 mV/A |
| Planned for GridGuard | relay IN GPIO26 (normally open, live only), buzzer GPIO25, touch pad GPIO27, LEDs GPIO32 green / GPIO33 red (proposed) |
| Power | Hi-Link HLK-5M05 with 1 A slow-blow fuse, tapped before the relay |

GPIO34/35 are ADC1, input-only: correct for WiFi use. Do not use ADC2 pins.

## Measurement method (firmware/PowerScope/PowerScope.ino)
- 1600 samples per window at 8 kHz = 200 ms = 10 cycles at 50 Hz, both channels read back to back.
- A window whose elapsed time is outside 196 to 204 ms is dropped (WiFi can stretch it). The previous values stay.
- DC midpoint removed per window. Vrms, Irms, P = mean(v*i), S, PF = |P|/S, Q = sqrt(S^2 - P^2), phase = acos(PF), crest factor = Ipk/Irms_raw, frequency from rising crossings with hysteresis.
- Scope arrays: two cycles (320 samples) starting on a rising voltage crossing.
- History rings for the web page: 1 s x 600, 5 s x 720, 30 s x 720 (RAM only, lost on reboot).
- Energy, load-on time, max power and the tariff are saved to flash (Preferences "powerscope") at most every 10 minutes.

## Calibration constants (currently compile-time, top of PowerScope.ino)
| Constant | Value | Source |
|---|---|---|
| V_GAIN | 0.946 | multimeter 242 V against 256 V displayed. An earlier calibration gave 1.00 at 234 V, so the trimmer may have been nudged: recheck. |
| I_SIGN | -1 | real power read negative on the air cooler with the wiring as built |
| I_TRIM | 1.00 | NOT calibrated yet. Needs a resistive load (kettle/heater) and a clamp meter. |
| SHIFT | 0 | NOT tuned. A resistive load should read PF 1.00; adjust by whole samples (2.25 degrees each). |
| I_NOISE | 0.195 A | idle log, see docs/idle-noise-analysis.md. Persisted as "inoise"; re-measure with Serial `Z` or `POST /zero`. |
| GATE_FACTOR | 1.35 | raw current below I_NOISE x 1.35 is "no load": I, P, Q, S, PF, angle, crest factor all 0 |

Idle noise adds to current in quadrature, so the code uses I = sqrt(Iraw^2 - Inoise^2). Do not subtract it linearly.
With WiFi on, loads under about 0.17 to 0.2 A (roughly 40 W) cannot be read reliably. Options: decoupling caps at the ACS712 supply, an ACS712-5A module (185 mV/A), or a median-of-3 ADC read (not tried; check the window timing on hardware first).

## Web API (served by the ESP32, consumed by web/index.html)
| Route | |
|---|---|
| GET / | gzip page from web_index.h, ETag, no-cache |
| GET /data | v,i,p,q,s,pf,phi,f,cf,kwh,on,avg,max,up,rate,inoise |
| GET /history?r=60 / 600 / 3600 / 21600 | {dt,v[],i[],p[]}, newest last |
| GET /scope | {v[],i[]}, 320 samples, volts and amps |
| POST /rate?x= | tariff in rupees per kWh |
| POST /reset | zero the energy counter (done in loop) |
| POST /zero | re-measure idle noise (done in loop) |
| captive-portal probes | generate_204, gen_204, hotspot-detect.html, connecttest.txt, ncsi.txt, wildcard DNS, notFound redirects to 192.168.4.1 |

The page falls back to demo data only if it has never reached the device. Once it has seen live data it shows "Offline" instead, so fake numbers can never pass as real.
No internet in AP mode: no CDN, no Google Fonts. Bai Jamjuree (500, 700) is embedded as base64 WOFF2. Palette: bg #0F1115, voltage #35D0FF, current #FF8A3D, power #B6F24A.

## Agreed next tasks
1. **Calibration page** (firmware + web). Zero-current button. Voltage and current calibration in both ways: type the multimeter/clamp reading and let the device compute the gain/trim, or type the numbers. Advanced: SHIFT and I_SIGN with a live PF check. Move V_GAIN, I_TRIM, SHIFT, I_SIGN into flash. Restore defaults. 4-digit PIN (default 1234, changeable) for calibration changes and resets.
2. **Energy history and bill estimate.** Phone sends the time when the page opens (`POST /time?epoch=&tz=`), device keeps it with millis(). Energy used before the first time sync is held aside and added to "today" when the time arrives. Keep 62 days and 12 months in flash. Show today and this month in kWh and rupees, a 30-day chart, a 12-month chart, CSV download (client side), and a month-end projection. Flat rate only. Calendar-month billing with an optional start day.
3. **GridGuard firmware and page**, from docs/GridGuard-spec.md. The simulator inside docs/GridGuard-project-page.html is the reference for the state machine.
4. Update the two project pages in docs/ for the modular-plate layout (docs/hardware-layout.md): no clear cover, new module order.
5. Optional noise work: decoupling caps, ACS712-5A, median filter.

## Testing
- `sh tests/host/run.sh` builds the real PowerScope.ino against stubs and feeds it synthetic waves with noise: idle, bursts, 0.39 A air cooler, 2 A heater, small load, and the zero calibration. Extend the stubs when adding firmware features. Last run: idle shows 0 A and 0 kWh, the cooler reads 0.387 A at PF 0.82, the heater 1.998 A.
- The web page was exercised in jsdom with a mocked device (demo mode, live mode, canvases, range buttons). It has not been looked at on every phone.
- Not yet verified on hardware: heavy simultaneous phone connections, the timing guard rate with several clients, ACS712 heating near 6 A.
- Dev unit uses a 20A ACS712 and the owner's own air cooler as the first load. Calibrate current on a resistive load.

## Conventions
- Arduino sketches live in a folder with the same name as the .ino.
- Keep the page self-contained (one file). Regenerate web_index.h after every edit with tools/embed_web.py and commit both.
- Safety rules for any mains-facing change: switch live only, relay normally open, MCB stays, no bare modules outside a closed box.
