TSL2591_AutoAdjust
Reading the TSL2591 light sensor with automatic gain control

This sketch communicates with the TSL2591 light sensor via I2C and transmits its measurements over serial. Gain is automatically adjusted as appropriate.

The sensor has four gain settings (1x, 25x, 428x, and 9876x) and six integration settings (100ms, 200ms, 300ms, 400ms, 500ms, 600ms).
Its nominal range is 188 uLux up to 88,000 lux. For reference, 188 uLux is just above a starlit moonless sky, and 88 kLux is near the upper end of direct sunlight.
If we look at the upper limit and gain together we can say the maximum lux for each gain's setting is 88 kLux, 3.52 kLux, 205 lux, and 8.9 lux respectively. Based on this, some experimental data, and a bit of arbitrary decision making, the following thresholds were selected.
1x gain: > 1700 lux
25x gain: 100 - 1700 lux
428x gain: 5 - 100 lux
9876x gain: < 5 lux
Additionally, an arbitrary amount of hysteresis was included to prevent a gain-tuning loop from occuring when the ammount of light present is near a threshold.
For each of the above gain settings, integration time is set to 600ms; this integration setting (the longest) seems to give the most accurate, stable results. Presumably, the device is doing some form of averaging internally. However, in bright-light conditions, a lower integration time is desirable. For measurements greater than 60,000 lux, the sketch will reduce gain to 1x and integration time to 100ms.


This sketch uses libraries from Adafruit - check them out!
https://github.com/adafruit/Adafruit_TSL2591_Library

Lux reference: https://en.wikipedia.org/wiki/Lux
