# Bill of materials (one PowerScope, 6 A)

Prices are approximate rupees. (est) marks estimates, not listings. Shared items bought in packs (wire, perfboard, hardware) are shown at half of the earlier two-unit figure.

## To buy
| # | Item | Qty | Have | To buy | Rs |
|---|---|---|---|---|---|
| 1 | ESP32 38-pin | 1 | 1 | 0 | 0 |
| 2 | ZMPT101B voltage sensor | 1 | 0 | 1 | 150 |
| 3 | ACS712-20A current sensor | 1 | 0 | 1 | 120 |
| 4 | Resistors 1%: 10k x2, 20k x2 | 4 | 0 | 4 | 20 |
| 5 | Ceramic capacitors 100nF | 2 | 0 | 2 | 10 |
| 6 | TTP223 touch module | 1 | 1 | 0 | 0 |
| 7 | Hi-Link HLK-5M05 (5 V 1 A) | 1 | 0 | 1 | 225 |
| 8 | 1 A slow-blow fuse + holder | 1 | 0 | 1 | 40 |
| 9 | 1N5819 Schottky diode | 1 | 0 | 1 | 5 |
| 10 | 6 A 3-pin plug | 1 | 1 | 0 | 0 |
| 11 | 3-core flexible cord, 2 m | 2 m | 0 | 2 m | 50 (est) |
| 12 | Blank module (touch cover) | 1 | 0 | 1 | 30 (est) |
| 13 | Internal wire, terminal blocks, wire lugs | 1 set | 0 | 1 | 150 |
| 14 | Perfboard, female headers, jumper wires | | 0 | | 150 (est) |
| 15 | Standoffs, cable glands, heat shrink, zip ties, insulated terminal covers | 1 set | 0 | 1 | 100 |
| 16 | Printed nameplate and colour strip | 1 | 0 | 1 | 50 (est) |
| 17 | 60 to 100 W test bulb + holder | 1 | 0 | 1 | 100 |
| | **Total** | | | | **about 1,200** |

## Already owned
Modular plate + surface box, 10 A MCB module, 5-hole universal socket, rocker switch module, OLED 0.96" and 1.3", ESP32, ZMPT101B, ACS712-20A (dev unit), TTP223.

## Optional
MOV 14D471K (Rs 10, surge protection, always upstream of an MCB), 470 uF capacitor on the 5 V rail if readings are noisy, ACS712-5A for small loads, 10 uF + 100 nF at the ACS712 supply.

## Checks before building
- Confirm the socket marking says 6 A. The owned MCB is 10 A, so keep loads at 6 A or less; a 6 A MCB is more correct.
- OLED drivers differ: 0.96" is SSD1306, 1.3" is usually SH1106.
- A dev-unit tool list: multimeter, clamp meter, soldering kit, socket tester.
