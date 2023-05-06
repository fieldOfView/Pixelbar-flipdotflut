flipdotflut
===========

An ESP32 display server for the Pixelbar [flipdot display controller](https://github.com/keoni29/flipdot).

The display server is loosely based on the concept of [pixelflut](https://github.com/defnull/pixelflut), though its name should not be seen as an invitation to flood the server (and the network) with traffic in DOS-style.

Because the server is intended to run on an ESP32 via WiFi, it uses a compact binary packet structure to save on bandwidth and processing power. It also uses UDP instead of TCP to lower network communication overhead.

The server is designed to be accessed by multiple clients concurrently as long as the ESP32, the display and the WiFi network can keep up. Currently no single client can claim permanent ownership of the display (though this functionality may be added at some point).

The server listens on UDP port 1234. The UDP packet structure closely follows the serial command format used by the flipdot controller: Two consecutive bytes containing x,y coordinates and dot polarity (on/off.)

```
1CCC CCCC 0xxP RRRR

C = column address
R = row address
P = dot polarity (1= on/ 0=off)
x = reserved for future use, ignored for now
```

Flipping a dot on the flipdot display is a slow, expensive process compared to receiving UDP packets, so the server can receive multiple updates on a dot before it gets round to sending it to the display. Internally the server caches the state of each dot, and only sends changed dots to the flipdot controller. Sending dots to the display happens asynchronously to receiving UDP packets. This way only the last received state for a dot is used when the display loop gets round to see if the dot needs to be flipped.

This is currently very much a work in progress. I have not even tried to compile it.
