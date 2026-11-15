// Arduwars FX - an Advance Wars style strategy game for the Arduboy FX / FX-C.
// Original game by Botond Kis (BSD 3-clause), FX port and completion 2026.
//
// Needs the Arduboy2, ArduboyFX and Tinyfont libraries, and the FX data
// in fxdata/ (fxdata.bin) on the FX chip.

#include "AWGame.h"

AWGame game;

// No USB stack in the release build (it was ~1.4 KB of flash). To upload from a
// PC, hold DOWN while switching on, or upload from the FX cart menu. The test
// builds keep USB for their serial output.
#if !defined(AW_AUTOPLAY) && !defined(AW_TESTHOOKS)
ARDUBOY_NO_USB
#endif

void setup() {
}

void loop() {
  game.run(); // never returns
}
