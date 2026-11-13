# MASHUP for Digitakt (mk1)

Performance repeats and effects for the **Elektron Digitakt mk1, OS 1.54**, as an
[elekloader](https://github.com/irpina/elekloader) mod (id `mashup-dt`; it was called STUTTER up to 0.5). The Digitakt keeps the last
few seconds of its master mix in a buffer at all times. Switch MASHUP mode on and the 16 trig keys play
that audio back: repeats, scrambles, half time, pitch steps, rolls and spin-backs, all in time. The
sequencer runs on underneath, so the pattern comes back exactly where it would have been.

By Robbie. GPL-2.0-or-later.

**This is the Digitakt version.** It does not run on a Digitone.

> Unofficial and unsupported. Elektron and Digitakt are trademarks of Elektron Music Machines. Nothing
> here is affiliated with or endorsed by them.

## What you need

- A Digitakt mk1 on **OS 1.54**, and the stock OS file for it.
- elekloader with **core 4.1**.

## Install

Add `mashup-dt-0.6.1-os1.54.elemod` in the elekloader app with core 4.1 and your other mods, build, and
send the firmware to the Digitakt. Or send the ready-made build, which is your usual set of mods with
MASHUP added.

## MASHUP mode

**FUNC + SAMPLING** switches the mode on and off. The screen's top line says `MASHUP ON` or
`MASHUP OFF`, the SAMPLING key lights purple while it is on, and each trig key lights in its colour.

- **Mode off:** every key is stock. Trig keys play their tracks, as they always did.
- **Mode on**, on the normal screen: the 16 trig keys are MASHUP's, and the sequencer's red step still
  moves across them while a pattern plays. Holding FUNC, mute mode (FUNC + BANK), pattern and bank
  selection (hold PTN, or tap BANK), grid recording and keyboard mode all work as stock: their keys and
  lights come back, and MASHUP's return when you leave them.
- The mode starts off when the Digitakt is switched on.
- FUNC + SAMPLING no longer starts a quick recording. Recording from the RECORDER page works as before.

## The keys

Everything starts on the next 16th, and the live mix comes back on the next 16th after you let go.
With the sequencer stopped, both happen at once. A held key lights white, and each press names what
the held keys will play on the screen's top line, such as `1/8 REV PITCH UP` or `SCRAMBLE 1/16`.

**Bottom row: repeats**

| Key | Colour | Hold it for |
|---|---|---|
| 9 | pale orange | a repeat of the last **½ bar** |
| 10 | pale orange | the last **¼** |
| 11 | orange | the last **⅛** |
| 12 | orange | the last **1/16** |
| 13 | strong orange | the last **1/32** |
| 14 | strong orange | the last **⅛ triplet** |
| 15 | purple | **REV**: with any of 9–14, the repeat plays backwards. On its own, in time with the pattern: each beat plays the beat before it backwards (with HALF, over two beats) |
| 16 | pink | **HALF**: with any of 9–14, the repeat plays at half speed (an octave down). On its own, **HALF-TIME**, in time with the pattern: each half bar plays its first beat over the whole half bar, an octave down, then starts again on the next |

Slide between keys for a build-up: hold 10, add 11, then 12. Each new length repeats a shorter part of
the same slice, and letting go of the newest key goes back to the one still held.

**Top row: effects**

| Key | Colour | Hold it for |
|---|---|---|
| 1 | light blue | **SCRAMBLE**: a random 16th from the last bar at every 16th |
| 2 | blue | **SCRAMBLE ⅛**: the same with 8ths, chunkier |
| 3 | bright blue | **SCRAMBLE + REV**: random 16ths, about a third of them backwards |
| 4 | teal | **SCRAMBLE MIX**: random 16ths and 8ths from the last bar, about a third of them backwards |
| 5 | dark green | **PITCH ▼**: with a length key (9–14) or ROLL, each pass plays a semitone lower than the last, down to two octaves |
| 6 | green | **PITCH ▲**: each pass a semitone higher, up to two octaves |
| 7 | yellow | **ROLL**: an automatic build-up, a beat each of ¼, ⅛ and 1/16, then 1/32 until you let go |
| 8 | grey | **BACKSPIN**: a DJ-style spin-back, backwards at three times speed slowing to a stop in about a beat. Let go on the downbeat to drop back in |

If several are on, the first of these wins: BACKSPIN, SCRAMBLE, the repeats, then REV and HALF on their
own. PITCH, REV and HALF change the repeat that's playing.

**Latching.** While you hold an effect, press SAMPLING: it latches as it is, without a break, and keeps
playing when you let go (the key stays white, and the top line says `LATCH ...`). Or hold SAMPLING and
tap a trig key. Tap a latched key again to unlatch it, or tap SAMPLING on its own to unlatch everything. Latched and held keys combine: latch ⅛, then play PITCH or REV over it by hand.
In MASHUP mode SAMPLING is the latch key, so turn the mode off to open the RECORDER.

**Combinations.** The more keys, the more happens:

| Keys | |
|---|---|
| two length keys (9–14) | the repeat alternates between the two lengths, pass by pass |
| PITCH ▼ + PITCH ▲ (5 + 6) | **WARBLE**: each pass alternately up and down by the step |
| PITCH with a length key or ROLL | each pass a step higher or lower (knob E sets the step) |
| SCRAMBLE (1–4) + a length key | scrambles slices of that length, 1/32 to ½ bar |
| BACKSPIN + HALF (8 + 16) | **SLOW SPIN**: from 1.5 times speed, over two beats |
| BACKSPIN + REV (8 + 15) | **TAPE START**: the mix winds up from a stop to speed in a beat |
| REV or HALF with a length key | that repeat backwards, or at half speed |

## The knobs

While you hold any MASHUP key (or hold SAMPLING, for latched effects), seven of the knobs shape what plays.
The top line shows each value as you turn; **push a knob to reset it**. The settings stay until you
reset them or switch the mode off, so they carry from one effect to the next. With no MASHUP key held,
the knobs are the page's, as always.

| Knob | | |
|---|---|---|
| A | **BEND** | pitch, smoothly, ±24 semitones in quarter tones: repeats, scrambles, half time and spins |
| B | **FILTER** | a DJ filter on the effect: left low-pass, right high-pass |
| C | — | the page's, as usual |
| D | **CRUSH** | fewer bits and a lower sample rate on the effect |
| E | **PITCH STEP** | how far PITCH ▲ / ▼ and WARBLE move each pass, 1–12 semitones |
| F | **GATE** | how much of each pass plays, 100 % down to 10 %: chopped stutters |
| G | **REV CHANCE** | how often a pass or slice plays backwards, 0–100 % |
| H | **MIX** | the effect against the live mix |

Every start, stop and slice is crossfaded over 64 samples, so nothing clicks. The repeats include delay
and reverb, and they go through GESTALT, so its knobs still work on them.

## What it gives up

- **In MASHUP mode, the trig keys don't play their tracks** on the normal screen. The sequencer plays on.
- **The buffer is the recorder's.** MASHUP uses 1 MB of the RAM the RECORDER keeps its takes in, so
  your 64 MB of sample memory is untouched. While the recorder is armed or recording, MASHUP steps
  aside and the keys do nothing. Afterwards it starts again and overwrites the take on the RECORDER
  page, so **save a take before you stutter** if you want to keep it.
- **Incoming audio** is part of the mix if the internal mixer sends it there, so it is repeated too.
- While the mode shows, the trig keys don't show which tracks are sounding (the step still does).
- **SAMPLING alone** is the latch key in the mode: the RECORDER opens with the mode off.

## How it works

`src/stutter.c` runs in three places:

- **The render**, on core 4.1's master output (`ev_render_master`, order 60, just before GESTALT's 70).
  Every block it writes the mix into a 1 MB ring of 16-bit stereo (5.4 s), and while an effect is held
  it plays the ring instead. It finds the 16ths from the render's timeline, which moves 2 × tempo each
  block (5,400,000 units a 16th).
- **The keys.** `src/ledhook.s` sees every key the main view handles (FUNC + SAMPLING is the mode's),
  and `src/keyhook.s` takes the trig keys on the normal screen's trig key path while the mode is on. An
  `ev_key` handler also catches their release wherever it lands, so no key stays held.
- **The lights.** Each UI frame the firmware clears the key LEDs, the screens set theirs through the LED
  manager, and a last pass turns off the rest. MASHUP sits at the manager's set and gives the trig keys
  and SAMPLING its colours through all of it, and skips the track-activity lights while its colours
  show. Pink borrows palette entry 13 while the mode is on, and gives it back after.

The OS 1.54 addresses, all found and checked in digiemu (the Digitakt mk1 emulator):

| | |
|---|---|
| 0x4237ef90 | the recorder's 16-bit buffer, 1,584,000 frames (33 s). MASHUP uses 0x42380000–0x42480000 |
| 0x4199f114 | the recorder's state: 0 idle, 1 armed, 2 recording |
| 0x80001f4c | the render's timeline |
| 0x4020df10 | the tempo, BPM × 120 |
| 0x40029876 | the main view's key handler (view, event) |
| 0x40029a50 | its trig key path on the normal screen (press and release) |
| 0x40028e1a | the firmware's keyboard-mode test (view) |
| 0x4009359c | its grid-recording test (view + 152) |
| 0x40093ab0 | the top line's message (view + 152, format, ...) |
| view + 256 | the pattern selection object: its byte 204 is set while PTN is held or bank selection is latched |
| 0x400083a0 | the LED manager's set (manager, LED, mode, colour); LED = panel LED + 1, trig keys 1–16, SAMPLING 36 |
| 0x40033b6c | the normal screen's track-activity lights |
| 0x400e677e, 0x4020dcb0 | a palette entry's set (entry, r, g, b), and the palette as sent |

CPU, counted in the emulator, in instructions per 32-sample block (the Digitakt renders 1,500 a
second): about 400 while it is only recording, 6,000–7,500 while an effect plays (with the knobs' filter,
crush and mix), up to about 13,000 when a SCRAMBLE slice changes. For comparison, GESTALT with both
halves on is about 4,900. RAM: 15 KB of code and data, leaving about 9 KB in the usual build.

## Building

```
python -m elekloader.sdk.build . --stock Digitakt_OS1.54.syx
```

## Licence

GPL-2.0-or-later.
