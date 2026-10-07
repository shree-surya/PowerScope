# Hardware layout (modular plates)

Each product is one modular plate on a surface box. Modules snap in and can be reordered. The plate is horizontal; the order below is left to right.

```
PowerScope  [ socket ][ rocker switch ][ 0.96" OLED ][ blank + touch pad ][ MCB ]
GridGuard   [ MCB ][ red TRIP lens ][ 1.3" OLED ][ rocker switch ][ socket ]
```

Decisions
- No clear or smoked cover: the plate is the finished front, and a cover would block the MCB and switch.
- Frames stay white. The products are told apart by mirrored module order, the lens, colour strips (cyan for PowerScope, yellow and black for GridGuard) and printed nameplates.
- The rocker switch becomes a manual load ON/OFF on the live wire just before the socket.
- PowerScope has no red lens (red means alarm). Its touch pad sits behind the blank module with an icon sticker.
- GridGuard keeps the red lens, with a red LED behind it (not the supplied neon) as the TRIP light, and a small green ARMED LED beside it. The touch pad sits behind the plate under the OLED window: short touch changes page, hold 2 s resets a trip. Test the touch through the plastic before gluing.
- Hi-Link supply and ZMPT101B are tapped before the relay so the controller stays alive after a trip.

Live-wire path
```
PowerScope: plug L -> MCB -> ACS712 IP+ / IP- -> rocker -> socket L
GridGuard:  plug L -> MCB -> relay COM / NO -> ACS712 IP+ / IP- -> rocker -> socket L
Neutral and earth go straight from plug to socket. ZMPT101B L/N across live (after MCB) and neutral.
The Hi-Link live comes from after the MCB through its own 1 A slow-blow fuse.
```
Only the live wire passes through the MCB, relay and ACS712. Test the wall socket for reversed live and neutral first, otherwise they sit on the neutral wire.

Single-pole MCB: supply on LINE (input), everything else on LOAD (output).
