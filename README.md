# PARAMECIUM

A homebrew game for the [Vectrex](https://en.wikipedia.org/wiki/Vectrex) vector
console, written in C for the [VectreC / CMOC](https://github.com/rogerboesch/vectreC)
toolchain.

You are a single-celled organism clinging to a **nucleus**. Orbit it, then leap
from nucleus to nucleus, tethering each one you land on. Link them all to clear
the slide and advance — but don't fly off the edge twice, and watch out for the
things living in the dish with you.

**Latest release:** [beta .07](https://github.com/bjonesdeft/vectrex-paramecium/releases/tag/beta-0.07)
(`paramecium.bin`)

## Gameplay

- **Orbit & jump.** Your organism continuously orbits its current nucleus.
  Hold **Button 1** to launch a jump straight out along your current heading;
  land on another nucleus to draw a tether between them. Collision is the rule:
  if your path hits a nucleus, you land.
- **Tether them all.** Visit every nucleus on the board to finish the level.
  Reusing a previous link is allowed.
- **Aborts.** Release before the commit distance and you spring back to the
  launch nucleus. Early release is never lethal.
- **Edge bounce.** Miss a nucleus and hit the screen edge once: you reflect and
  get a second chance to land. Hit an edge again on that same jump and you fall
  off the slide.
- **One hazard at a time.** Each level features a single enemy type so the
  vector budget stays playable on real hardware.
- **50 levels** of escalating difficulty: more nuclei, faster orbits and jumps,
  tighter size ranges, drifting nuclei that bounce off each other, and rotating
  hazards. Boards past the first few are generated procedurally.

## Hazards (introduced by level)

| Level | Hazard | Behavior |
|------:|--------|----------|
| 2 | **Eye** | Opens on a random nucleus, pupil scanning; landing on it while open bursts you. Nuclei also begin drifting. |
| 3 | **Bacterium** | A swimming microbe; collide mid-jump and you burst. |
| 4+ | **Amoeba** | A slow morphing blob; jump too close and it electrocutes you. |

## Controls

| Input | Action |
|-------|--------|
| Button 1 (title) | Start the game |
| Button 1 (hold) | Launch / hold a jump |
| Button 1 (release early) | Abort back to orbit |
| Button 1 (retry screen) | Try again |

The **Try Again** screen times out after ~15 seconds and returns to the title.

## Building

Requires the VectreC toolchain (CMOC + lwtools + Vectrex stdlib). Paths are set
in [`config.env`](config.env) and can be overridden with environment variables:

- `VECTREC` — path to the VectreC toolchain directory
- `VECX_APP` — path to the [Vecx](https://github.com/jhawthorn/vecx) emulator app (for `run.sh`)

```bash
./compile.sh        # build build/sphere-link.bin
./run.sh            # build, then launch a fresh ROM copy in Vecx
./watch.sh          # rebuild on source changes
```

The output ROM (`build/sphere-link.bin`) runs on real Vectrex hardware via a
flash cart, or in any Vectrex emulator. Prebuilt binaries are attached to
[GitHub Releases](https://github.com/bjonesdeft/vectrex-paramecium/releases).

## Layout

```
src/main.c     game source (single translation unit)
Makefile       build rules
compile.sh     one-shot build
run.sh         build + launch in Vecx (unique ROM path each run)
watch.sh       rebuild-on-change loop
config.env     toolchain / emulator paths
```

## License

© 2026 Brian Jones. Boot ROM copyright mark: PROK.
