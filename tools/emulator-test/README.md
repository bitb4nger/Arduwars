# Emulator test rig

`abrun` runs the game headless on the [Ardens](https://github.com/tiberiusbrown/Ardens)
emulator core: no window, buttons from a script, screenshots as PGM files,
serial output on stdout, and the lowest stack pointer seen (to catch the stack
running into the variables, the classic Arduboy crash). See `build.sh`.

```
abrun GAME.hex fxdata.bin [-m] -f FRAMES -o OUTDIR [-e EVERY] [-s f1,f2] [-i "f:BTNS,..."] [-S script.txt]
                           [-t] [-c cpu.txt] [-P prof.txt FROMFRAME]
  -m   FX-C / Mini wiring (flash chip select on PE2/HWB) for the -FXC build
  -i   button changes by frame: "40:B,42:,100:UR" (U D L R A B)
  -S   script: "+N BTNS" = N frames after the last line, "W TOKEN" = wait for
       TOKEN on serial (the AW_TESTHOOKS build prints HT<day> when your turn
       starts), "snap NAME" = screenshot
  -t   print the deepest stack and its call chain (resolve with avr-addr2line)
  -c   per-frame CPU load;  -P  instruction counts for prof.py (where time goes)
```

Test builds of the game (compile with `--build-property compiler.cpp.extra_flags=...`):

| flag | what it does |
|---|---|
| `-DAW_AUTOPLAY` | the computer plays both sides, logs every turn and the full state every 5 days over serial (`drawstate.py` turns that into map pictures) |
| `-DAW_TESTHOOKS` | prints `HT<day>` when a human turn starts, for scripts |
| `-DAW_SCENARIO` | starts the Skirmish map with a small fight set up next to your HQ (with `scenario-combat.txt`) |
