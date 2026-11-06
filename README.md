# Arduino 74-Series IC Tester

A low-cost digital IC tester built around an **Arduino Uno**. Type the number of a 74-series TTL chip on a 4×4 keypad, drop the chip in the ZIF socket, and the tester checks it against its truth table (or functional test sequence) and shows **PASS / FAIL** on a 16×2 LCD.

![Prototype](images/prototype.jpg)

## Features

- Tests **40+ common 74-series ICs**: gates, decoders, multiplexers, encoders, comparator, adder, 7-segment drivers, flip-flops, latches, counters and shift registers
- Exhaustive truth-table testing for combinational ICs (every input combination is applied)
- Functional step-by-step tests for sequential ICs (clear, preset, set, reset, toggle, hold, count up/down, load, shift)
- On failure the LCD shows **which outputs failed** (and, for sequential ICs, the **failing step**); for combinational ICs it also shows the first failing input pattern with expected vs. got values
- Power pins (VCC/GND) are **assigned automatically in software**, so ICs with non-standard power pins (e.g. 7473, 7475, 7476, 7483, 7490, 7493) work too
- Table-driven design: adding a new combinational IC is one line in the `combTests[]` table
- Serial debug output at 9600 baud

## Hardware

| Part | Notes |
|------|-------|
| Arduino Uno | Main controller |
| 16-pin ZIF socket | Holds the IC under test |
| 16×2 LCD with I²C backpack | Address `0x27` |
| PCF8574 I/O expander | Address `0x38`, reads the keypad |
| 4×4 membrane keypad | Connected to PCF8574 P0–P7 |
| Perfboard, hook-up wire, header pins | For the ZIF shield |
| 9 V battery or USB | Power |

| Arduino | ZIF Socket | Arduino | ZIF Socket |
|---------|-----------|---------|-----------|
| D2 | pin 1 | D10 | pin 9 |
| D3 | pin 2 | D11 | pin 10 |
| D4 | pin 3 | D12 | pin 11 |
| D5 | pin 4 | D13 | pin 12 |
| D6 | pin 5 | A0 | pin 13 |
| D7 | pin 6 | A1 | pin 14 |
| D8 | pin 7 | A2 | pin 15 |
| D9 | pin 8 | A3 | pin 16 |

**I²C bus:** A4 = SDA, A5 = SCL, shared by the LCD and the PCF8574. Power both from 5 V and GND.

![Circuit diagram](images/circuit-diagram.png)

### Components

| Arduino Uno | 16×2 LCD | 4×4 Keypad | ZIF Socket |
|:-:|:-:|:-:|:-:|
| ![](images/arduino-uno.jpg) | ![](images/lcd-16x2.jpg) | ![](images/keypad-4x4.jpg) | ![](images/zif-socket.jpg) |

## Software setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Install these libraries via **Sketch → Include Library → Manage Libraries**:
   - `LiquidCrystal_I2C`
   - `I2CKeyPad` (by Rob Tillaart)
3. Open `ic_tester_74_series/ic_tester_74_series.ino`.
4. Select **Arduino Uno**, choose your port, and upload.

If the LCD stays blank, your backpack may use another address (often `0x3F`). Change `LCD_ADDR` at the top of the sketch.

## How to use

1. Power the board. The LCD shows `74-Series Tester / System Ready`.
2. Insert the IC in the ZIF socket and close the lever.
   - **16-pin ICs:** pin 1 at ZIF pin 1.
   - **14-pin ICs:** place them at the **end of the socket with pin 1 at ZIF pin 1**, so IC pins 8–14 land on ZIF pins 10–16 (the socket's last two slots on the lower side stay empty).
3. Type the IC number on the keypad. `74` is already filled in, so for a 7400 type `00`, for a 74138 type `138`.

   | Key | Action |
   |-----|--------|
   | `0`–`9` | Enter digit |
   | `*` | Clear entry (back to `74`) |
   | `#` | Start test |

4. Read the result on the LCD. `Status: PASS :)` means the chip behaved correctly for every test.
5. **Remove the IC only after the test finishes** (the pins are released automatically).

## Supported ICs

**Combinational**

| Type | ICs |
|------|-----|
| Gates | 7400, 7402, 7404, 7408, 7432, 7486 |
| 3-input gates | 7410, 7411, 7427 |
| 4-input gates | 7420, 7421 |
| Decoders | 7442, 74138, 74139, 74155 |
| Multiplexers | 74151, 74153, 74157, 74158 |
| Encoders | 74147, 74148 |
| Comparator | 7485 |
| Adder | 7483 |
| 7-segment drivers | 7447, 7448 |

**Sequential**

| Type | ICs |
|------|-----|
| D flip-flop | 7474 |
| JK flip-flops | 7473, 7476, 74107, 74109, 74112 |
| Latch | 7475 |
| Counters | 7490, 7493, 74161, 74163, 74192, 74193 |
| Shift registers | 7491, 7495 |

Details of the test procedures are in [docs/how-it-works.md](docs/how-it-works.md).

## Configuration

At the top of the sketch:

| Setting | Default | Meaning |
|---------|---------|---------|
| `DEBUG` | `true` | Print failure details on Serial (9600 baud) |
| `HW_POWER` | `false` | `false`: Arduino pins power the IC automatically. `true`: you power the IC from the 5 V rail with a switch (standard-pinout ICs only) |
| `LCD_ADDR` / `KEYPAD_ADDR` | `0x27` / `0x38` | I²C addresses |

## Adding a new IC

For a simple combinational IC, add one row to `combTests[]`:

```cpp
// num, name, size, kind, gateSize, nIn, nOut, inputs, outputs, [fixed pins], [vcc, gnd]
{7400, "7400 NAND", 14, K_NAND, 2, 8, 4, {1,2,4,5,9,10,12,13}, {3,6,8,11}},
```

For new logic functions, add a `kind` to the enum and a `case` in `evalComb()`. Sequential ICs need their own test function plus a `case` in `runIC()`.

## Limitations and safety

- Designed for **5 V TTL** 74-series chips only (74LS/74HC/74HCT variants usually work, but this is not guaranteed). Do not test CMOS chips that need other voltages.
- The Arduino pins supply power to the IC, so ICs that draw a lot of current (LED-driving outputs, shorted chips) can overload the pins. The ATmega328P allows about 40 mA per pin and about 200 mA in total.
- Insert and remove ICs with the tester idle, never mid-test.
- Seven-segment drivers are not tested for input codes 10–15, because these differ between manufacturers.
- A PASS shows the chip matches the tests here; it is not a guarantee against every possible fault (speed, timing, and electrical limits are not checked).

## License

Released under the [MIT License](LICENSE).
