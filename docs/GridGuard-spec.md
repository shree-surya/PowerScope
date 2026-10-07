# GridGuard spec

Reference: the working simulator in docs/GridGuard-project-page.html (state machine in its script).

## Settings (set on the web page, saved in flash)
| Setting | Default | Note |
|---|---|---|
| Over-voltage (OVV) | 250 V | |
| Under-voltage (UVV) | 190 V | |
| Over-current (OCV) | 6 A | keep at 7 A or below, the relay is 10 A |
| Under-current (UCV) | 0.5 A, off by default | not below about 0.3 A, sensor noise (see idle-noise-analysis.md; with WiFi on, the floor is higher) |
| Trip delay | 2 s | a fault must last this long |
| Restart delay | 10 s demo default | fridge preset 180 s |
| After a trip | auto reconnect or wait for reset | one global setting |
| Hysteresis | 2 % | a voltage counts as normal again inside OVV x 0.98 and UVV x 1.02 |

Presets (starting points): Fridge (190 to 250 V, 6 A, restart 180 s), Water pump (190 to 250 V, 6 A, under-current 0.5 A, restart 30 s), Charger (190 to 250 V, 3 A, restart 10 s).

## States
Armed -> Tripping (delay counting) -> Tripped (relay open, buzzer, red LED, reason logged) -> Waiting (auto mode, restart delay counting) -> Armed.
- With the relay open the load current cannot be seen. After a current trip the unit simply retries when the restart delay ends and trips again if the fault persists.
- A voltage fault only reconnects once the supply is back inside the hysteresis band.
- After closing the relay, ignore the under-current check for 1.5 s.
- Manual reset refused while the supply is still out of range.
- Relay wired normally open: an unpowered or crashed controller means the load is off.

## Hardware and I/O
Relay module 5 V (10 A) COM/NO on live, IN GPIO26. Buzzer GPIO25. Touch GPIO27 (short: page, 2 s hold: reset). Red LED behind the lens on GPIO33 and green ARMED LED on GPIO32 (both pins proposed, not tested). 1.3" SH1106 OLED on 21/22. Sensors as in CLAUDE.md. Check the relay logic polarity with a bulb before relying on it.

## OLED
Status page (ARMED / WARNING / TRIPPED / countdown), graph page with the limit lines, trip page (reason, value, time), connect page.

## Web (own hotspot GridGuard-XXXX, WPA2)
Settings page behind a PIN, event log with reason and readings (flash, time from the phone, uptime until a phone connects), live graph with limit lines, manual relay ON/OFF, calibration page shared with PowerScope, trip counter.

## Open questions
Event-log size and format in flash; whether the relay should open on over-power (watts) as a fifth limit; buzzer pattern per fault.
