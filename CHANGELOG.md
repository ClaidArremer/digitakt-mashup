# Changelog: MASHUP (Digitakt mk1)

MASHUP was called STUTTER up to 0.5.

## 0.6.1 (2026-11-13)

- Fix: after FUNC + SAMPLING, the trig keys could stay dark and do nothing, as if FUNC were still held.
  MASHUP now ignores FUNC's key repeats, and takes FUNC as up whenever another key goes down without
  the firmware's FUNC flag.

## 0.6 (2026-11-13)

- Renamed **MASHUP** (id `mashup-dt`); the pop-up says `MASHUP ON` / `OFF`.
- **HALF-TIME** (16 alone) is in time: each half bar plays its first beat over the whole half bar, then
  starts again. **REV** alone is in time: each beat plays the beat before it backwards (with HALF, over
  two beats).
- **Latch while holding**: SAMPLING pressed while an effect is held latches it without a break.
- Holding FUNC and mute mode (FUNC + BANK) give the trig keys and their lights back to the firmware, as
  pattern and bank selection already did.
- Knob C (DECAY) is gone: C is the page's again.

## 0.5 (2026-11-13)

- **The knobs**: while a STUTTER key (or SAMPLING) is held, A BEND, B FILTER, C DECAY, D CRUSH, E PITCH
  STEP, F GATE, G REV CHANCE, H MIX. A push resets a knob; each value shows on the top line.
- **Combinations**: two length keys alternate; PITCH ▼ + ▲ is WARBLE; SCRAMBLE + a length key scrambles
  slices of that length; BACKSPIN + HALF is SLOW SPIN; BACKSPIN + REV is TAPE START.
- PITCH's resampled repeats wrap within their slice, faded at each seam, instead of falling silent.

## 0.4 (2026-11-13)

- **Latching**: SAMPLING + a trig key latches its effect; that key again, or SAMPLING alone, unlatches.
- Trig key 4 is **SCRAMBLE MIX** (16ths and 8ths at random, a third backwards, teal). HALF-TIME moves
  to trig key 16 on its own (pink), where HALF with a length key is still a half-speed repeat. The
  tape stop is gone.
- Pattern and bank selection (PTN held, BANK latched) work in the mode, with the stock lights.
- The sequencer's red step moves across STUTTER's colours while a pattern plays.

## 0.3 (2026-11-13)

- PITCH ▼ is now trig key 5 and PITCH ▲ trig key 6 (dark green and green).
- Each press names what the held keys play on the top line, such as `1/8 REV PITCH UP`.

## 0.2 (2026-11-13)

- **STUTTER mode**: FUNC + SAMPLING switches it on and off, with a pop-up (`STUTTER ON` / `OFF`).
  Mode off, every key is stock, so trig keys 9–16 play the MIDI tracks again. FUNC + SAMPLING no
  longer starts a quick recording.
- In the mode, the top row's effects: 1 SCRAMBLE, 2 SCRAMBLE ⅛, 3 SCRAMBLE + REV, 4 HALF-TIME,
  5 PITCH ▲, 6 PITCH ▼, 7 ROLL, 8 BACKSPIN.
- The lights: SAMPLING purple in the mode, each trig key in its colour, a held key white. Grid
  recording and keyboard mode keep their stock lights. The track-activity lights rest while STUTTER's
  colours show.
- Configuration changes and SCRAMBLE's slices land on 16ths kept from the render's timeline, or from
  the tempo while the sequencer is stopped.

## 0.1 (2026-11-13)

- First version. An always-on 5.4 s buffer of the master mix, before GESTALT, in the recorder's RAM.
- Trig keys 9–14 on the normal screen repeat the last ½ bar, ¼, ⅛, 1/16, 1/32 or ⅛ triplet, starting
  and stopping on the next 16th. 15 REV (backwards; alone, a reverse swoop), 16 HALF (half speed;
  alone, a tape stop).
- Steps aside while the recorder is armed or recording. Leaves the keys alone in grid recording and in
  keyboard mode.
- OS 1.54, core 4.1. Tested in digiemu with the full build (DIGISEQ, GLIDE 0.4, GESTALT 0.2, DH-101,
  Digi Mono, digihealth, LOST LVLS splash, Direct Jump).
