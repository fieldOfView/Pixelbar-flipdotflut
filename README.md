flipdotflut
===========

A set of display servers and example clients for the Pixelbar [flipdot display controller](https://github.com/keoni29/flipdot), based loosely on the concept of [pixelflut](https://github.com/defnull/pixelflut). Note that its name should not necessarily be seen as an invitation to flood the server (and the network) with traffic in DOS-style.

Because the server is intended to run on simple hardware (think Raspberry Pi or even ESP32), it uses a compact binary packet structure to save on bandwidth and processing power. It also uses UDP instead of TCP to lower network communication overhead. Clients do not have a guarantee a packet has arrived at the server, so to be sure they may have to either send pixels slow enough that packets are not lost due to congestion, or they have to send packets mutliple times. This is the way of the flut.

The server is designed to be accessed by multiple clients concurrently as long as the server, the display and the network can keep up. Currently no single client can claim permanent ownership of the display (though this functionality may be added at some point).

The server listens on UDP port 1337. The UDP packet structure closely follows the serial command format used by the flipdot controller: Two consecutive bytes containing x,y coordinates and dot polarity (on/off.)

```
1CCC CCCC 0xxP RRRR

C = column address
R = row address
P = dot polarity (1= on/ 0=off)
x = reserved for future use, ignored for now
```

Flipping a dot on the flipdot display is a slow, expensive process compared to receiving UDP packets, so the server can receive multiple updates on a dot before it gets round to sending it to the display. Internally the server caches the state of each dot, and only sends changed dots to the flipdot controller. Sending dots to the display happens asynchronously to receiving UDP packets. This way only the last received state for a dot is used when the display loop gets round to see if the dot needs to be flipped.
