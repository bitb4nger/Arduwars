#!/bin/sh
# Builds both hex files and the .arduboy packages into build/.
# Needs arduino-cli with an AVR core plus the Arduboy2 and ArduboyFX libraries,
# and Python 3 with Pillow + MrBlinky's Arduboy-Python-Utilities (fxdata-build.py).
#   FQBN    board to compile for (default arduboy-homemade:avr:arduboy)
#   FXDATA  path to fxdata-build.py
set -e
cd "$(dirname "$0")/.."
FQBN=${FQBN:-arduboy-homemade:avr:arduboy}
OUT=build
mkdir -p $OUT
if [ -n "$FXDATA" ]; then (cd ArduwarsFX/fxdata && python3 "$FXDATA" fxdata.txt); fi
arduino-cli compile -b "$FQBN" $EXTRA --build-path /tmp/arduwars-fx  ArduwarsFX
cp /tmp/arduwars-fx/ArduwarsFX.ino.hex $OUT/ArduwarsFX-FX.hex
arduino-cli compile -b "$FQBN" $EXTRA --build-property "compiler.cpp.extra_flags=-DCART_CS_HWB" --build-path /tmp/arduwars-fxc ArduwarsFX
cp /tmp/arduwars-fxc/ArduwarsFX.ino.hex $OUT/ArduwarsFX-FXC.hex
cp ArduwarsFX/fxdata/fxdata.bin ArduwarsFX/fxdata/fxdata-data.bin ArduwarsFX/fxdata/fxdata-save.bin $OUT/
python3 tools/make_arduboy.py $OUT
