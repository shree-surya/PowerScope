# Hardware layout (modular plates)

PowerScope is one modular plate on a surface box. Modules snap in and can be reordered. The plate is horizontal; the order below is left to right.

```
[ socket ][ rocker switch ][ 0.96" OLED ][ blank + touch pad ][ MCB ]
```

Decisions
- No clear or smoked cover: the plate is the finished front, and a cover would block the MCB and switch.
- Frame stays white, with a cyan colour strip and a printed nameplate.
- The rocker switch becomes a manual load ON/OFF on the live wire just before the socket.
- No red lens (red means alarm). The touch pad sits behind the blank module with an icon sticker. Test the touch through the plastic before gluing.

Live-wire path
```
plug L -> MCB -> ACS712 IP+ / IP- -> rocker -> socket L
Neutral and earth go straight from plug to socket. ZMPT101B L/N across live (after MCB) and neutral.
The Hi-Link live comes from after the MCB through its own 1 A slow-blow fuse.
```
Only the live wire passes through the MCB and ACS712. Test the wall socket for reversed live and neutral first, otherwise they sit on the neutral wire.

Single-pole MCB: supply on LINE (input), everything else on LOAD (output).
