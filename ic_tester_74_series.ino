#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <I2CKeyPad.h>
#include <avr/pgmspace.h>

#define LCD_ADDR    0x27
#define KEYPAD_ADDR 0x38

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
I2CKeyPad keypad(KEYPAD_ADDR);

char keyMap[] = "123A"
                "456B"
                "789C"
                "*0#D";

// ==========================================
// Types (must stay above all functions)
// ==========================================
enum { K_NAND, K_NOR, K_AND, K_OR, K_XOR, K_NOT,
       K_DEC138, K_DEC139, K_DEC42, K_DEC155,
       K_MUX157, K_MUX158, K_MUX153, K_MUX151,
       K_ENC147, K_ENC148, K_CMP85, K_ADD83, K_7SEGH, K_7SEGL };

struct CombTest {
  unsigned long num;
  char name[12];
  uint8_t size, kind, gsz, nIn, nOut;
  uint8_t inPins[12];     // IC pin numbers of inputs
  uint8_t outPins[10];    // IC pin numbers of outputs
  uint8_t nFix;           // inputs held at a fixed level
  uint8_t fixPins[3];
  uint8_t fixVals;        // bit k = level of fixPins[k]
  uint8_t vcc, gnd;       // 0 = standard
};

// ZIF pin (1..16) -> Arduino pin
const int zifPins[16] = {
  2, 3, 4, 5, 6, 7, 8, 9,
  10, 11, 12, 13, A0, A1, A2, A3
};

const bool DEBUG = true;      // Serial debug at 9600 baud

// false = Arduino pins supply VCC and GND. Any pin can be VCC/GND, chosen
//         automatically per IC (default VCC=ZIF16, GND=ZIF7 or ZIF8).
// true  = you power the IC from the 5V rail with a manual switch (standard
//         pinout ICs only).
const bool HW_POWER = false;

int icSize = 14;

// ==========================================
// Low-level pin helpers
// ==========================================
void setPin(int zifPin, int mode, int val = LOW) {
  int p = zifPins[zifPin - 1];
  pinMode(p, mode);
  if (mode == OUTPUT) digitalWrite(p, val);
}

int readPin(int zifPin) {
  int p = zifPins[zifPin - 1];
  pinMode(p, INPUT_PULLUP);
  delayMicroseconds(200);
  return digitalRead(p);
}

// 14-pin IC: pins 1-7 -> ZIF 1-7, pins 8-14 -> ZIF 10-16. 16-pin: one-to-one.
int icToZif(int icPin) {
  if (icSize == 14 && icPin >= 8) return icPin + 2;
  return icPin;
}

void setICPin(int icPin, int mode, int val = LOW) { setPin(icToZif(icPin), mode, val); }
int  readICPin(int icPin) { return readPin(icToZif(icPin)); }

void resetPins() {
  for (int i = 0; i < 16; i++) pinMode(zifPins[i], INPUT);
}

// vcc / gnd are IC pin numbers. 0 = standard (VCC = last pin, GND = half).
void powerUp(int pins, int vcc = 0, int gnd = 0) {
  icSize = pins;
  if (vcc == 0) vcc = pins;
  if (gnd == 0) gnd = pins / 2;
  resetPins();
  delay(20);
  if (!HW_POWER) {
    setICPin(gnd, OUTPUT, LOW);    // GND (automatic)
    setICPin(vcc, OUTPUT, HIGH);   // VCC (automatic)
  }
  delay(30);
}

// ==========================================
// Table-driven combinational tests
// ==========================================
const CombTest combTests[] PROGMEM = {
  // num, name, size, kind, gateSize, nIn, nOut, inputs, outputs, [fix..], [vcc,gnd]
  {7400,  "7400 NAND",  14, K_NAND, 2, 8, 4, {1,2,4,5,9,10,12,13}, {3,6,8,11}},
  {7402,  "7402 NOR",   14, K_NOR,  2, 8, 4, {2,3,5,6,8,9,11,12},  {1,4,10,13}},
  {7404,  "7404 INV",   14, K_NOT,  1, 6, 6, {1,3,5,9,11,13},      {2,4,6,8,10,12}},
  {7408,  "7408 AND",   14, K_AND,  2, 8, 4, {1,2,4,5,9,10,12,13}, {3,6,8,11}},
  {7432,  "7432 OR",    14, K_OR,   2, 8, 4, {1,2,4,5,9,10,12,13}, {3,6,8,11}},
  {7486,  "7486 XOR",   14, K_XOR,  2, 8, 4, {1,2,4,5,9,10,12,13}, {3,6,8,11}},
  {7410,  "7410 NAND3", 14, K_NAND, 3, 9, 3, {1,2,13,3,4,5,9,10,11}, {12,6,8}},
  {7411,  "7411 AND3",  14, K_AND,  3, 9, 3, {1,2,13,3,4,5,9,10,11}, {12,6,8}},
  {7427,  "7427 NOR3",  14, K_NOR,  3, 9, 3, {1,2,13,3,4,5,9,10,11}, {12,6,8}},
  {7420,  "7420 NAND4", 14, K_NAND, 4, 8, 2, {1,2,4,5,9,10,12,13}, {6,8}},
  {7421,  "7421 AND4",  14, K_AND,  4, 8, 2, {1,2,4,5,9,10,12,13}, {6,8}},
  {74138, "74138 DEC",  16, K_DEC138, 0, 6, 8, {1,2,3,4,5,6},      {15,14,13,12,11,10,9,7}},
  {74139, "74139 DEC",  16, K_DEC139, 0, 6, 8, {1,2,3,15,14,13},   {4,5,6,7,12,11,10,9}},
  {7442,  "7442 DEC",   16, K_DEC42,  0, 4, 10, {12,13,14,15},     {1,2,3,4,5,6,7,9,10,11}},
  {74155, "74155 DEC",  16, K_DEC155, 0, 6, 8, {13,3,1,2,14,15},   {7,6,5,4,9,10,11,12}},
  {74157, "74157 MUX",  16, K_MUX157, 0, 10, 4, {1,15,2,3,5,6,11,10,14,13}, {4,7,9,12}},
  {74158, "74158 MUX",  16, K_MUX158, 0, 10, 4, {1,15,2,3,5,6,11,10,14,13}, {4,7,9,12}},
  {74153, "74153 MUX",  16, K_MUX153, 0, 12, 2, {14,2,1,15,6,5,4,3,10,11,12,13}, {7,9}},
  {74151, "74151 MUX",  16, K_MUX151, 0, 12, 2, {11,10,9,7,4,3,2,1,15,14,13,12}, {5,6}},
  {74147, "74147 ENC",  16, K_ENC147, 0, 9, 4, {10,11,12,1,2,3,4,5,13}, {9,7,6,14}},
  {74148, "74148 ENC",  16, K_ENC148, 0, 9, 5, {10,11,12,13,1,2,3,4,5}, {9,7,6,14,15}},
  // 7485: cascade inputs fixed (A<B=0, A=B=1, A>B=0)
  {7485,  "7485 CMP",   16, K_CMP85, 0, 8, 3, {10,12,13,15,9,11,14,1}, {5,6,7}, 3, {2,3,4}, 2},
  // 7483: VCC pin 5, GND pin 12
  {7483,  "7483 ADD",   16, K_ADD83, 0, 9, 5, {10,8,3,1,11,7,4,16,13}, {9,6,2,15,14}, 0, {0}, 0, 5, 12},
  // 7447/7448: BI/RBO (pin 4) left unconnected
  {7447,  "7447 7SEG",  16, K_7SEGL, 0, 6, 7, {7,1,2,6,3,5}, {13,12,11,10,9,15,14}},
  {7448,  "7448 7SEG",  16, K_7SEGH, 0, 6, 7, {7,1,2,6,3,5}, {13,12,11,10,9,15,14}},
};

uint16_t care;   // output bits that matter for the current pattern

uint16_t evalComb(const CombTest &c, uint16_t x) {
  uint16_t o = 0;
  care = 0xFFFF;
  switch (c.kind) {
    case K_NAND: case K_NOR: case K_AND: case K_OR: case K_XOR: case K_NOT: {
      uint8_t mask = (1 << c.gsz) - 1;
      for (uint8_t g = 0; g < c.nOut; g++) {
        uint8_t b = (x >> (g * c.gsz)) & mask;
        bool r;
        switch (c.kind) {
          case K_NAND: r = (b != mask); break;
          case K_AND:  r = (b == mask); break;
          case K_NOR:  r = (b == 0);    break;
          case K_OR:   r = (b != 0);    break;
          case K_XOR:  r = __builtin_parity(b); break;
          default:     r = (b == 0);    break;   // NOT
        }
        if (r) o |= (1 << g);
      }
    } break;

    case K_DEC138: {   // A,B,C,G2A,G2B,G1
      o = 0xFF;
      bool en = ((x >> 5) & 1) && !((x >> 3) & 1) && !((x >> 4) & 1);
      if (en) o &= ~(1 << (x & 7));
    } break;

    case K_DEC139: {   // 1G,1A,1B,2G,2A,2B
      o = 0xFF;
      if (!(x & 1))        o &= ~(1 << ((x >> 1) & 3));
      if (!((x >> 3) & 1)) o &= ~(1 << (4 + ((x >> 4) & 3)));
    } break;

    case K_DEC42: {    // A,B,C,D -> outputs 0..9 active low
      o = 0x3FF;
      uint8_t v = x & 15;
      if (v <= 9) o &= ~(1 << v);
    } break;

    case K_DEC155: {   // A,B,1C,/1G,/2C,/2G ; outputs 1Y0-3, 2Y0-3 active low
      o = 0xFF;
      uint8_t sel = x & 3;
      if (((x >> 2) & 1) && !((x >> 3) & 1))  o &= ~(1 << sel);
      if (!((x >> 4) & 1) && !((x >> 5) & 1)) o &= ~(1 << (4 + sel));
    } break;

    case K_MUX157: case K_MUX158: {   // S,G, 1A,1B,2A,2B,3A,3B,4A,4B
      bool S = x & 1, G = (x >> 1) & 1;
      for (uint8_t g = 0; g < 4; g++) {
        bool a = (x >> (2 + 2 * g)) & 1;
        bool b = (x >> (3 + 2 * g)) & 1;
        bool y = G ? 0 : (S ? b : a);
        if (y) o |= (1 << g);
      }
      if (c.kind == K_MUX158) o ^= 0x0F;   // inverting version
    } break;

    case K_MUX153: {   // A,B,1G,2G, 1C0-3, 2C0-3
      uint8_t sel = x & 3;
      for (uint8_t ch = 0; ch < 2; ch++) {
        bool en = !((x >> (2 + ch)) & 1);
        bool d = (x >> (4 + 4 * ch + sel)) & 1;
        if (en && d) o |= (1 << ch);
      }
    } break;

    case K_MUX151: {   // A,B,C,G, D0-D7 ; outputs Y, W
      uint8_t sel = x & 7;
      bool G = (x >> 3) & 1;
      bool d = (x >> (4 + sel)) & 1;
      bool y = !G && d;
      o = (y ? 1 : 0) | (y ? 0 : 2);
    } break;

    case K_ENC147: {   // bits0-8 = inputs 1..9 (active low), outputs A,B,C,D active low
      uint8_t v = 0;
      for (int8_t n = 9; n >= 1; n--) if (!((x >> (n - 1)) & 1)) { v = n; break; }
      o = (~v) & 15;
    } break;

    case K_ENC148: {   // bits0-7 = in0..7 (low), bit8 = /EI ; outputs A0,A1,A2,GS,EO
      bool ei = (x >> 8) & 1;
      if (ei) {
        o = 0x07 | 0x08 | 0x10;
      } else {
        int8_t hi = -1;
        for (int8_t n = 7; n >= 0; n--) if (!((x >> n) & 1)) { hi = n; break; }
        if (hi < 0) o = 0x07 | 0x08;          // none active: GS=1, EO=0
        else        o = ((~hi) & 7) | 0x10;   // GS=0, EO=1
      }
    } break;

    case K_CMP85: {    // A0-A3, B0-B3 ; outputs A>B, A=B, A<B
      uint8_t a = x & 15, b = (x >> 4) & 15;
      o = (a > b ? 1 : 0) | (a == b ? 2 : 0) | (a < b ? 4 : 0);
    } break;

    case K_ADD83: {    // A1-A4, B1-B4, C0 ; outputs S1-S4, C4
      uint8_t a = x & 15, b = (x >> 4) & 15, c0 = (x >> 8) & 1;
      o = a + b + c0;
    } break;

    case K_7SEGH: case K_7SEGL: {   // A,B,C,D,/LT,/RBI ; outputs a..g
      static const uint8_t seg[10] PROGMEM = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
      uint8_t v = x & 15;
      bool lt = (x >> 4) & 1, rbi = (x >> 5) & 1;
      uint8_t s;
      if (!lt)                    s = 0x7F;                    // lamp test
      else if (v == 0 && !rbi)    s = 0;                       // ripple blanking
      else if (v <= 9)            s = pgm_read_byte(&seg[v]);
      else                      { s = 0; care = 0; }           // 10-15 differ by maker
      o = (c.kind == K_7SEGL) ? (~s & 0x7F) : s;
    } break;
  }
  return o;
}

uint16_t badIn, badExp, badGot;

int runComb(const CombTest &c) {
  powerUp(c.size, c.vcc, c.gnd);
  for (uint8_t j = 0; j < c.nOut; j++) setICPin(c.outPins[j], INPUT);
  for (uint8_t k = 0; k < c.nFix; k++) setICPin(c.fixPins[k], OUTPUT, (c.fixVals >> k) & 1);
  delay(10);

  uint16_t failed = 0;
  bool first = true;
  uint32_t total = 1UL << c.nIn;

  for (uint32_t x = 0; x < total; x++) {
    for (uint8_t i = 0; i < c.nIn; i++) setICPin(c.inPins[i], OUTPUT, (x >> i) & 1);
    uint16_t expv = evalComb(c, x), got = 0;
    for (uint8_t j = 0; j < c.nOut; j++)
      if (readICPin(c.outPins[j])) got |= (1 << j);
    uint16_t diff = (expv ^ got) & care;
    if (diff) {
      failed |= diff;
      if (first) { badIn = x; badExp = expv; badGot = got; first = false; }
    }
  }
  if (DEBUG && failed) {
    Serial.print("Fail in=0x"); Serial.print(badIn, HEX);
    Serial.print(" exp=0x");    Serial.print(badExp, HEX);
    Serial.print(" got=0x");    Serial.println(badGot, HEX);
  }
  resetPins();
  return failed;
}

// ==========================================
// Sequential tests
// ==========================================
uint8_t failStep = 0;

void ctl(int pin, int v) { setICPin(pin, OUTPUT, v); }

bool chkQ(int q, int qn, int eq, int eqn, uint8_t step) {
  bool ok = (readICPin(q) == eq) && (readICPin(qn) == eqn);
  if (!ok && failStep == 0) failStep = step;
  return ok;
}

// 7474 dual D flip-flop (rising edge)
int test7474() {
  const uint8_t ff[2][6] = {{1,2,3,4,5,6}, {13,12,11,10,9,8}};  // clr,d,clk,pre,q,qn
  int failed = 0; failStep = 0;
  powerUp(14);
  for (int i = 0; i < 2; i++) {
    uint8_t clr = ff[i][0], d = ff[i][1], clk = ff[i][2], pre = ff[i][3], q = ff[i][4], qn = ff[i][5];
    setICPin(q, INPUT); setICPin(qn, INPUT);
    ctl(clr, HIGH); ctl(pre, HIGH); ctl(d, LOW); ctl(clk, LOW); delay(2);
    bool ok = true;

    ctl(pre, LOW); delay(2); ok &= chkQ(q, qn, 1, 0, 1); ctl(pre, HIGH);
    ctl(clr, LOW); delay(2); ok &= chkQ(q, qn, 0, 1, 2); ctl(clr, HIGH);

    ctl(d, HIGH); ctl(clk, LOW); delay(2); ctl(clk, HIGH); delay(2);
    ok &= chkQ(q, qn, 1, 0, 3);
    ctl(d, LOW); delay(2);
    ok &= chkQ(q, qn, 1, 0, 4);
    ctl(clk, LOW); delay(2); ctl(clk, HIGH); delay(2);
    ok &= chkQ(q, qn, 0, 1, 5);
    ctl(d, HIGH); ctl(clk, LOW); delay(2); ctl(clk, HIGH); delay(2);
    ok &= chkQ(q, qn, 1, 0, 6);

    if (!ok) failed |= (1 << i);
  }
  resetPins();
  return failed;
}

// Generic dual JK flip-flop tester.
// ff[i] = {j, k, clk, clr, pre, q, qn}  (pre = 0 -> no preset pin)
// rising = clock edge type, kInv = K input is active low (74109)
int testJK(const uint8_t ff[][7], uint8_t nff, uint8_t size, uint8_t vcc, uint8_t gnd,
           bool rising, bool kInv) {
  int failed = 0; failStep = 0;
  powerUp(size, vcc, gnd);
  for (uint8_t i = 0; i < nff; i++) {
    uint8_t j = ff[i][0], k = ff[i][1], clk = ff[i][2], clr = ff[i][3];
    uint8_t pre = ff[i][4], q = ff[i][5], qn = ff[i][6];
    bool idle = rising ? LOW : HIGH;
    setICPin(q, INPUT); setICPin(qn, INPUT);
    ctl(clr, HIGH); if (pre) ctl(pre, HIGH);
    ctl(j, LOW); ctl(k, kInv ? HIGH : LOW); ctl(clk, idle); delay(2);
    bool ok = true;

    auto pulse = [&]() { ctl(clk, !idle); delay(2); ctl(clk, idle); delay(2); };
    auto setJK = [&](bool jv, bool kv) { ctl(j, jv); ctl(k, kInv ? !kv : kv); delay(1); };

    ctl(clr, LOW); delay(2); ok &= chkQ(q, qn, 0, 1, 1); ctl(clr, HIGH);
    if (pre) { ctl(pre, LOW); delay(2); ok &= chkQ(q, qn, 1, 0, 2); ctl(pre, HIGH); }
    setJK(0, 1); pulse(); ok &= chkQ(q, qn, 0, 1, 3);   // reset
    setJK(1, 0); pulse(); ok &= chkQ(q, qn, 1, 0, 4);   // set
    setJK(1, 1); pulse(); ok &= chkQ(q, qn, 0, 1, 5);   // toggle
                 pulse(); ok &= chkQ(q, qn, 1, 0, 6);   // toggle
    setJK(0, 0); pulse(); ok &= chkQ(q, qn, 1, 0, 7);   // hold
    ctl(clr, LOW); delay(2); ok &= chkQ(q, qn, 0, 1, 8); ctl(clr, HIGH);

    if (!ok) failed |= (1 << i);
  }
  resetPins();
  return failed;
}

// 7475 4-bit latch (2 enables, VCC pin 5, GND pin 12)
int test7475() {
  const uint8_t L[4][3] = {{2,16,1}, {3,14,15}, {6,10,11}, {7,9,8}};  // d,q,qn
  const uint8_t G12 = 13, G34 = 4;
  const uint8_t pats[4] = {0x0A, 0x05, 0x0F, 0x00};
  int failed = 0; failStep = 0;
  powerUp(16, 5, 12);
  for (int i = 0; i < 4; i++) { setICPin(L[i][1], INPUT); setICPin(L[i][2], INPUT); }
  ctl(G12, HIGH); ctl(G34, HIGH);

  for (int p = 0; p < 4; p++) {                       // transparent: Q follows D
    for (int i = 0; i < 4; i++) ctl(L[i][0], (pats[p] >> i) & 1);
    delay(2);
    for (int i = 0; i < 4; i++) {
      int bit = (pats[p] >> i) & 1;
      if (!chkQ(L[i][1], L[i][2], bit, !bit, 1)) failed |= (1 << i);
    }
  }
  for (int i = 0; i < 4; i++) ctl(L[i][0], (0x0A >> i) & 1);
  delay(2);
  ctl(G12, LOW); ctl(G34, LOW); delay(2);             // latch pattern 1010
  for (int i = 0; i < 4; i++) ctl(L[i][0], !((0x0A >> i) & 1));
  delay(2);
  for (int i = 0; i < 4; i++) {                       // must hold
    int bit = (0x0A >> i) & 1;
    if (!chkQ(L[i][1], L[i][2], bit, !bit, 2)) failed |= (1 << i);
  }
  ctl(G12, HIGH); ctl(G34, HIGH); delay(2);           // follow again
  for (int i = 0; i < 4; i++) {
    int bit = !((0x0A >> i) & 1);
    if (!chkQ(L[i][1], L[i][2], bit, !bit, 3)) failed |= (1 << i);
  }
  resetPins();
  return failed;
}

// 7490 decade / 7493 binary ripple counters (falling-edge clocks)
int testRipple(bool is7490) {
  const uint8_t CKB = 1, R1 = 2, R2 = 3, R91 = 6, R92 = 7, CKA = 14;
  const uint8_t QA = 12, QB = 9, QC = 8, QD = 11;
  failStep = 0;
  powerUp(14, 5, 10);
  setICPin(QA, INPUT); setICPin(QB, INPUT); setICPin(QC, INPUT); setICPin(QD, INPUT);
  ctl(CKA, HIGH); ctl(CKB, HIGH); ctl(R1, LOW); ctl(R2, LOW);
  if (is7490) { ctl(R91, LOW); ctl(R92, LOW); }
  delay(2);

  bool ok = true;
  auto bad = [&](uint8_t step) { ok = false; if (!failStep) failStep = step; };
  auto pulse = [&](uint8_t pin) { ctl(pin, LOW); delay(2); ctl(pin, HIGH); delay(2); };

  ctl(R1, HIGH); ctl(R2, HIGH); delay(2);                       // reset
  if (readICPin(QA) || readICPin(QB) || readICPin(QC) || readICPin(QD)) bad(1);
  ctl(R1, LOW); ctl(R2, LOW);

  pulse(CKA); if (readICPin(QA) != 1) bad(2);                   // divide by 2
  pulse(CKA); if (readICPin(QA) != 0) bad(2);

  int n = is7490 ? 5 : 8;                                       // divide by 5 / 8
  for (int k = 1; k <= n; k++) {
    pulse(CKB);
    int v = readICPin(QB) | (readICPin(QC) << 1) | (readICPin(QD) << 2);
    if (v != (k % n)) bad(3);
  }
  if (is7490) {                                                 // set to 9
    ctl(R91, HIGH); ctl(R92, HIGH); delay(2);
    if (!(readICPin(QA) == 1 && readICPin(QB) == 0 && readICPin(QC) == 0 && readICPin(QD) == 1)) bad(4);
    ctl(R91, LOW); ctl(R92, LOW);
  }
  resetPins();
  return ok ? 0 : 1;
}

// 74161 (async clear) / 74163 (sync clear) 4-bit counters
int readCount() {
  return readICPin(14) | (readICPin(13) << 1) | (readICPin(12) << 2) | (readICPin(11) << 3);
}

bool chkCnt(int expect, uint8_t step, bool checkRco) {
  bool ok = (readCount() == expect);
  if (checkRco && (readICPin(15) != (expect == 15))) ok = false;
  if (!ok && failStep == 0) failStep = step;
  return ok;
}

int testCounter(bool sync) {
  const uint8_t CLR = 1, CLK = 2, pA = 3, pB = 4, pC = 5, pD = 6, ENP = 7, LOAD = 9, ENT = 10;
  failStep = 0;
  powerUp(16);
  for (uint8_t p = 11; p <= 15; p++) setICPin(p, INPUT);
  ctl(CLR, HIGH); ctl(CLK, LOW); ctl(pA, LOW); ctl(pB, LOW); ctl(pC, LOW); ctl(pD, LOW);
  ctl(ENP, HIGH); ctl(ENT, HIGH); ctl(LOAD, HIGH); delay(2);

  auto pulse = [&]() { ctl(CLK, HIGH); delay(2); ctl(CLK, LOW); delay(2); };
  bool ok = true;

  ctl(CLR, LOW); delay(2);
  if (sync) pulse();
  ok &= chkCnt(0, 1, true);
  ctl(CLR, HIGH);

  for (int n = 1; n <= 16; n++) { pulse(); ok &= chkCnt(n & 15, 2, true); }

  ctl(pA, HIGH); ctl(pD, HIGH);
  ctl(LOAD, LOW); pulse(); ctl(LOAD, HIGH);
  ok &= chkCnt(9, 3, true);
  pulse(); ok &= chkCnt(10, 4, true);

  ctl(ENP, LOW); pulse(); ok &= chkCnt(10, 5, false); ctl(ENP, HIGH);
  ctl(ENT, LOW); pulse(); ok &= chkCnt(10, 6, false); ctl(ENT, HIGH);

  resetPins();
  return ok ? 0 : 1;
}

// 74192 (decade) / 74193 (binary) up/down counters
int testUpDown(bool dec) {
  const uint8_t pB = 1, QB = 2, QA = 3, DN = 4, UP = 5, QC = 6, QD = 7;
  const uint8_t pD = 9, pC = 10, LD = 11, CO = 12, BO = 13, CLR = 14, pA = 15;
  const int mod = dec ? 10 : 16;
  const int L = dec ? 7 : 10;
  failStep = 0;
  powerUp(16);
  setICPin(QA, INPUT); setICPin(QB, INPUT); setICPin(QC, INPUT); setICPin(QD, INPUT);
  setICPin(CO, INPUT); setICPin(BO, INPUT);
  ctl(UP, HIGH); ctl(DN, HIGH); ctl(LD, HIGH); ctl(CLR, LOW);
  ctl(pA, LOW); ctl(pB, LOW); ctl(pC, LOW); ctl(pD, LOW); delay(2);

  bool ok = true;
  auto rd = [&]() -> int {
    return readICPin(QA) | (readICPin(QB) << 1) | (readICPin(QC) << 2) | (readICPin(QD) << 3);
  };
  auto chk = [&](int e, uint8_t step) {
    if (rd() != e) { ok = false; if (!failStep) failStep = step; }
  };

  ctl(CLR, HIGH); delay(2); chk(0, 1); ctl(CLR, LOW); delay(2);
  ctl(pA, L & 1); ctl(pB, (L >> 1) & 1); ctl(pC, (L >> 2) & 1); ctl(pD, (L >> 3) & 1);
  ctl(LD, LOW); delay(2); ctl(LD, HIGH); delay(2);
  chk(L, 2);

  int cnt = L;
  for (int i = 0; i < 6; i++) {                       // count up
    ctl(UP, LOW); delay(2);
    if (cnt == mod - 1 && readICPin(CO) != 0) { ok = false; if (!failStep) failStep = 3; }
    ctl(UP, HIGH); delay(2);
    cnt = (cnt + 1) % mod;
    chk(cnt, 4);
  }
  for (int i = 0; i < 12; i++) {                      // count down
    ctl(DN, LOW); delay(2);
    if (cnt == 0 && readICPin(BO) != 0) { ok = false; if (!failStep) failStep = 5; }
    ctl(DN, HIGH); delay(2);
    cnt = (cnt + mod - 1) % mod;
    chk(cnt, 6);
  }
  resetPins();
  return ok ? 0 : 1;
}

// 7491 8-bit serial shift register
int test7491() {
  const uint8_t CK = 8, DA = 9, DB = 10, Q = 13, QN = 12;
  failStep = 0;
  powerUp(14);
  setICPin(Q, INPUT); setICPin(QN, INPUT);
  ctl(DA, LOW); ctl(DB, LOW); ctl(CK, LOW); delay(2);
  bool ok = true;
  for (int t = 0; t < 24; t++) {
    int bit = (0xB5C3 >> (t % 16)) & 1;
    ctl(DA, bit); ctl(DB, bit); delay(1);
    ctl(CK, HIGH); delay(2); ctl(CK, LOW); delay(2);
    if (t >= 7) {
      int e = (0xB5C3 >> ((t - 7) % 16)) & 1;
      if (!chkQ(Q, QN, e, !e, 1)) ok = false;
    }
  }
  resetPins();
  return ok ? 0 : 1;
}

// 7495 4-bit parallel-access shift register
int test7495() {
  const uint8_t SI = 1, pA = 2, pB = 3, pC = 4, pD = 5, MODE = 6, CK2 = 8, CK1 = 9;
  const uint8_t QD = 10, QC = 11, QB = 12, QA = 13;
  failStep = 0;
  powerUp(14);
  setICPin(QA, INPUT); setICPin(QB, INPUT); setICPin(QC, INPUT); setICPin(QD, INPUT);
  ctl(SI, LOW); ctl(pA, LOW); ctl(pB, LOW); ctl(pC, LOW); ctl(pD, LOW);
  ctl(MODE, LOW); ctl(CK1, HIGH); ctl(CK2, HIGH); delay(2);

  bool ok = true;
  auto rd = [&]() -> int {
    return readICPin(QA) | (readICPin(QB) << 1) | (readICPin(QC) << 2) | (readICPin(QD) << 3);
  };
  auto chk = [&](int e, uint8_t step) {
    if (rd() != e) { ok = false; if (!failStep) failStep = step; }
  };
  auto pulse = [&](uint8_t pin) { ctl(pin, LOW); delay(2); ctl(pin, HIGH); delay(2); };

  ctl(MODE, HIGH); ctl(pA, HIGH); ctl(pC, HIGH); delay(2);   // load A=1,B=0,C=1,D=0
  pulse(CK2); chk(0x05, 1);

  ctl(MODE, LOW); delay(2);
  int v = 0x05;
  const uint8_t si[4] = {1, 0, 0, 1};
  for (int i = 0; i < 4; i++) {                              // shift right
    ctl(SI, si[i]); delay(1);
    pulse(CK1);
    v = ((v << 1) | si[i]) & 15;
    chk(v, 2);
  }
  ctl(MODE, HIGH); ctl(pA, LOW); ctl(pB, HIGH); ctl(pC, LOW); ctl(pD, HIGH); delay(2);
  pulse(CK2); chk(0x0A, 3);

  resetPins();
  return ok ? 0 : 1;
}

// ==========================================
// UI
// ==========================================
void showResult(String name, int mask, bool comb) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(name);
  lcd.setCursor(0, 1);
  if (mask == 0) {
    lcd.print("Status: PASS :)");
  } else {
    String s = comb ? "Out:" : "Fail:";
    for (int i = 0; i < 10; i++)
      if (mask & (1 << i)) s += (char)(i < 9 ? '1' + i : 'A');
    if (!comb) { s += " st"; s += failStep; }
    lcd.print(s);
  }
  delay(4000);

  if (mask && comb) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("In:");  lcd.print(badIn, HEX);
    lcd.setCursor(0, 1);
    lcd.print("E:");   lcd.print(badExp, HEX);
    lcd.print(" G:");  lcd.print(badGot, HEX);
    delay(3000);
  }
}

void showTesting(String name) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(name);
  lcd.setCursor(0, 1);
  lcd.print("Testing...");
  delay(250);
}

void msgNeedArduinoPower() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set HW_POWER to");
  lcd.setCursor(0, 1);
  lcd.print("false for this IC");
  delay(2500);
}

bool runIC(unsigned long num) {
  for (uint8_t i = 0; i < sizeof(combTests) / sizeof(combTests[0]); i++) {
    CombTest c;
    memcpy_P(&c, &combTests[i], sizeof(CombTest));
    if (c.num == num) {
      uint8_t v = c.vcc ? c.vcc : c.size;
      uint8_t g = c.gnd ? c.gnd : c.size / 2;
      if (HW_POWER && (v != c.size || g != c.size / 2)) { msgNeedArduinoPower(); return true; }
      showTesting(c.name);
      int m = runComb(c);
      showResult(c.name, m, true);
      return true;
    }
  }

  // Sequential ICs: {j, k, clk, clr, pre, q, qn}
  switch (num) {
    case 7474:
      showTesting("7474 D-FF"); showResult("7474 D-FF", test7474(), false); return true;

    case 74107: {
      const uint8_t ff[2][7] = {{1,4,12,13,0,3,2}, {8,11,9,10,0,5,6}};
      showTesting("74107 JK-FF");
      showResult("74107 JK-FF", testJK(ff, 2, 14, 0, 0, false, false), false); return true;
    }
    case 74112: {
      const uint8_t ff[2][7] = {{3,2,1,15,4,5,6}, {11,12,13,14,10,9,7}};
      showTesting("74112 JK-FF");
      showResult("74112 JK-FF", testJK(ff, 2, 16, 0, 0, false, false), false); return true;
    }
    case 74109: {
      const uint8_t ff[2][7] = {{2,3,4,1,5,6,7}, {14,13,12,15,11,10,9}};
      showTesting("74109 JK-FF");
      showResult("74109 JK-FF", testJK(ff, 2, 16, 0, 0, true, true), false); return true;
    }
    case 7473: {
      if (HW_POWER) { msgNeedArduinoPower(); return true; }
      const uint8_t ff[2][7] = {{14,3,1,2,0,12,13}, {7,10,5,6,0,9,8}};
      showTesting("7473 JK-FF");
      showResult("7473 JK-FF", testJK(ff, 2, 14, 4, 11, false, false), false); return true;
    }
    case 7476: {
      if (HW_POWER) { msgNeedArduinoPower(); return true; }
      const uint8_t ff[2][7] = {{4,16,1,3,2,15,14}, {9,12,6,8,7,11,10}};
      showTesting("7476 JK-FF");
      showResult("7476 JK-FF", testJK(ff, 2, 16, 5, 13, false, false), false); return true;
    }
    case 7475:
      if (HW_POWER) { msgNeedArduinoPower(); return true; }
      showTesting("7475 LATCH"); showResult("7475 LATCH", test7475(), false); return true;
    case 7490:
      if (HW_POWER) { msgNeedArduinoPower(); return true; }
      showTesting("7490 DEC-CNT"); showResult("7490 DEC-CNT", testRipple(true), false); return true;
    case 7493:
      if (HW_POWER) { msgNeedArduinoPower(); return true; }
      showTesting("7493 BIN-CNT"); showResult("7493 BIN-CNT", testRipple(false), false); return true;
    case 74161:
      showTesting("74161 CNT"); showResult("74161 CNT", testCounter(false), false); return true;
    case 74163:
      showTesting("74163 CNT"); showResult("74163 CNT", testCounter(true), false); return true;
    case 74192:
      showTesting("74192 U/D"); showResult("74192 U/D", testUpDown(true), false); return true;
    case 74193:
      showTesting("74193 U/D"); showResult("74193 U/D", testUpDown(false), false); return true;
    case 7491:
      showTesting("7491 SHIFT"); showResult("7491 SHIFT", test7491(), false); return true;
    case 7495:
      showTesting("7495 SHIFT"); showResult("7495 SHIFT", test7495(), false); return true;
  }
  return false;
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  resetPins();

  lcd.init();
  lcd.backlight();

  keypad.begin();
  keypad.loadKeyMap(keyMap);

  lcd.setCursor(0, 0);
  lcd.print("74-Series Tester");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  delay(2000);
}

void loop() {
  String inputBuffer = "74";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter IC Number:");
  lcd.setCursor(0, 1);
  lcd.print(inputBuffer);

  bool waitingForKeyRelease = false;

  while (true) {
    char key = keypad.getChar();
    if (key == '\0' || key == 'N') waitingForKeyRelease = false;

    if (!waitingForKeyRelease && key != '\0' && key != 'N' && key != 'F') {
      waitingForKeyRelease = true;

      if (key == '#') {
        break;
      } else if (key == '*') {
        inputBuffer = "74";
      } else if (key >= '0' && key <= '9' && inputBuffer.length() < 6) {
        inputBuffer += key;
      }
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(inputBuffer);
      delay(150);
    }
  }

  if (!runIC(inputBuffer.toInt())) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Unknown IC:");
    lcd.setCursor(0, 1);
    lcd.print(inputBuffer);
    delay(2000);
  }
}