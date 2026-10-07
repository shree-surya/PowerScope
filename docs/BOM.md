# Bill of materials (6 A, both products)

Prices are approximate rupees. (est) marks estimates, not listings. Plates, MCBs, sockets, rocker switches, lens modules, surface boxes and both OLEDs are already owned.

## To buy
| # | Item | Qty (both) | Split | Have | To buy | Rs |
|---|---|---|---|---|---|---|
| 1 | ESP32 38-pin | 2 | 1 + 1 | 1 | 1 | 425 |
| 2 | ZMPT101B voltage sensor | 2 | 1 + 1 | 0 | 2 | 300 |
| 3 | ACS712-20A current sensor | 2 | 1 + 1 | 0 | 2 | 240 |
| 4 | Resistors 1%: 10k x4, 20k x4 | 8 | 4 + 4 | 0 | 8 | 40 |
| 5 | Ceramic capacitors 100nF | 4 | 2 + 2 | 0 | 4 | 20 |
| 6 | TTP223 touch module | 2 | 1 + 1 | 1 | 1 | 30 (est) |
| 7 | Active buzzer 5 V | 1 | GridGuard | 0 | 1 | 20 |
| 8 | 5 mm LEDs red + green with two 330 ohm | 1 set | GridGuard | 0 | 1 | 10 (est) |
| 9 | Hi-Link HLK-5M05 (5 V 1 A) | 2 | 1 + 1 | 0 | 2 | 450 |
| 10 | 1 A slow-blow fuse + holder | 2 | 1 + 1 | 0 | 2 | 80 |
| 11 | 1N5819 Schottky diode | 2 | 1 + 1 | 0 | 2 | 10 |
| 12 | 6 A 3-pin plug | 2 | 1 + 1 | 1 | 1 | 100 (est, with cord, item 13) |
| 13 | 3-core flexible cord, 2 m each | 4 m | | 0 | 4 m | (in item 12) |
| 14 | Blank module (PowerScope touch cover) | 1 | PowerScope | 0 | 1 | 30 (est) |
| 15 | Internal wire, terminal blocks, wire lugs | 2 sets | 1 + 1 | 0 | 2 | 300 |
| 16 | Perfboard, female headers, jumper wires | | | 0 | | 300 |
| 17 | Standoffs, cable glands, heat shrink, zip ties, insulated terminal covers | 2 sets | | 0 | 2 | 200 |
| 18 | Printed nameplates and colour strips | 2 | 1 + 1 | 0 | 2 | 100 (est) |
| 19 | 60 to 100 W test bulb + holder | 1 | shared | 0 | 1 | 100 |
| | **Total** | | | | | **about 2,755** |

## Already owned
Modular plate + surface box x2, 10 A MCB module x2, 5-hole universal socket x2, rocker switch module x2, red lens module x2, OLED 0.96" and 1.3", relay module 5 V 10 A (GridGuard), ESP32 x1, ZMPT101B x1, ACS712-20A x1, TTP223 x1.

## Optional
MOV 14D471K (Rs 10 each, surge protection, always upstream of an MCB), 470 uF capacitor on the 5 V rail if readings are noisy, ACS712-5A for small loads, 10 uF + 100 nF at the ACS712 supply.

## Checks before building
- Confirm the socket marking says 6 A. The owned MCB is 10 A, so keep loads at 6 A or less; a 6 A MCB is more correct.
- The 10 A relay is only for 6 A boxes. Keep the over-current limit at 7 A or below.
- OLED drivers differ: 0.96" is SSD1306, 1.3" is usually SH1106.
- A dev-unit tool list: multimeter, clamp meter, soldering kit, socket tester.
