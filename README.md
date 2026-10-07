# PowerScope and GridGuard

Two ESP32 products for single-phase 230 V loads up to 6 A, built into modular wall-plate boxes.

| | PowerScope | GridGuard |
|---|---|---|
| Job | Monitor only | Monitor and protect (relay cut-off) |
| Shows | Vrms, Irms, P, Q, S, PF, phase angle, crest factor, frequency, kWh | Voltage and current against limits you set |
| Display | 0.96" OLED (1.3" on the dev unit) | 1.3" OLED |
| Web | Own WiFi hotspot, live dashboard, trends, scope, energy and cost | Own WiFi hotspot, settings page, event log |
| Protection | 6A MCB only | 6A MCB plus software trip on a 10A relay |
| Status | Running on the bench | Designed, firmware not started |

Both use the same sensing core: ZMPT101B (voltage) and ACS712-20A (current) into an ESP32, 200 ms windows of ten mains cycles.

## Layout

```
firmware/PowerScope/     the finished sketch (PowerScope.ino) and generated web_index.h
firmware/bringup/        small test sketches used to bring the sensors up, in order
web/index.html           the dashboard page (self-contained, font embedded, demo mode)
tools/embed_web.py       web/index.html -> firmware/PowerScope/web_index.h (gzip in PROGMEM)
tests/host/              builds the real sketch on a PC against stub libraries, no hardware
docs/                    BOM, hardware layout, GridGuard spec, idle-noise analysis, project pages
CLAUDE.md                context for Claude Code: decisions, constants, open tasks
```

## Flash PowerScope

1. Arduino IDE, board **ESP32 Dev Module**, Partition Scheme **Huge APP (3MB No OTA/1MB SPIFFS)**.
2. Libraries: ESPAsyncWebServer and AsyncTCP (ESP32Async, install from the GitHub ZIPs), and U8g2 (Library Manager).
3. Open `firmware/PowerScope/PowerScope.ino` (`web_index.h` sits beside it) and upload. Serial Monitor at 115200.
4. Join the WiFi **PowerScope**, password **powerscope**, open **http://192.168.4.1**.

After editing `web/index.html`: `python tools/embed_web.py`, then upload again.

## Check the maths without hardware

```
sh tests/host/run.sh
```

## Licences

The dashboard embeds a subset of Bai Jamjuree (SIL Open Font License 1.1, Cadson Demak). No licence file has been chosen for the rest of the repo yet: add one before publishing.

## Safety

This works on 230 V mains. Mount every module in a closed box, cover live terminals, switch the live wire only, keep the MCB as the real protection, and test the wall socket first. It is a learning and demonstration project, accurate to a few percent after calibration, not a billing meter and not a certified protection device.
