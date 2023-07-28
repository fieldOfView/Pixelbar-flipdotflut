flipdotflut ESP32
=================

This implementation for an ESP32 is a proof of concept. In reality the ESP32 is not capable of receiving small packets via UDP anywhere near fast enough to make a useful display server. Hence the implementation now supports batching up to 720 dots into a single UDP package.