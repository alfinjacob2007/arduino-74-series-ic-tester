# How it works

## Pin control

The ZIF socket's 16 pins map to Arduino pins D2–D13 and A0–A3 (`zifPins[]`). Every pin can be an input, an output, or a power pin, so nothing is hard-wired. Before each test, `powerUp()` sets the correct IC pin to GND (LOW) and VCC (HIGH), then drives the inputs and reads the outputs.

14-pin ICs are mapped so IC pins 1–7 sit on ZIF 1–7 and IC pins 8–14 sit on ZIF 10–16 (`icToZif()`). The 14-pin IC's VCC (pin 14) is therefore ZIF 16, driven from A3.

## Combinational ICs

Each IC has a row in `combTests[]` (stored in flash with `PROGMEM`) that lists the input pins, output pins and function type. For every one of the 2^n input combinations the tester:

1. Drives the input pins,
2. Reads the output pins (with internal pull-ups, after 200 µs),
3. Compares them with the expected value from `evalComb()`.

The result is a bitmask of failing outputs, shown on the LCD as the output numbers that failed. The first failing input pattern is also displayed, with the expected and received values in hex.

Some ICs hold certain inputs at a fixed level (for example the 7485 cascade inputs) through the `fixPins` / `fixVals` fields.

## Sequential ICs

Flip-flops, latches, counters and shift registers are tested with a scripted sequence, for example for a JK flip-flop:

| Step | Test |
|------|------|
| 1 | Asynchronous clear gives Q = 0 |
| 2 | Asynchronous preset gives Q = 1 (if the IC has a preset pin) |
| 3 | J=0, K=1 and a clock pulse resets |
| 4 | J=1, K=0 and a clock pulse sets |
| 5, 6 | J=1, K=1 toggles on each clock pulse |
| 7 | J=0, K=0 holds |
| 8 | Clear again |

If a step fails, the number is shown on the LCD as `stN` so you can see where the chip misbehaved. Counters check the full count sequence, ripple/carry outputs, load, and enable inputs; shift registers check data movement over several clocks.

## User interface

The keypad is read through a PCF8574 using the `I2CKeyPad` library. The main loop collects up to 6 characters (starting with `74`), then `runIC()` looks the number up first in the combinational table and then in the sequential `switch`. Unknown numbers show `Unknown IC`.
