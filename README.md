# PARAMECIUM

A homebrew game for the [Vectrex](https://en.wikipedia.org/wiki/Vectrex) vector
console, written in C for the [VectreC / CMOC](https://github.com/rogerboesch/vectreC)
toolchain.

You are a single-celled organism clinging to a **nucleus**. Orbit it, then leap
from nucleus to nucleus, tethering each one you land on. Link them all to clear
the slide and advance — but don't fly off the edge, and watch out for the things
living in the dish with you.

## Gameplay

- **Orbit & jump.** Your organism continuously orbits its current nucleus.
  Hold **Button 1** to launch a jump straight out along your current heading;
  land on another nucleus to draw a tether between them.
- **Tether them all.** Connect every nucleus on the board to finish the level.
  You cannot reuse an existing tether.
- **Aborts.** Release before you're committed to pull back to your orbit. You
  get **two** aborted attempts; a third failed attempt bursts you.
- **Fall off the slide.** Leaving the screen in any way is instant death.
- **50 levels** of escalating difficulty: more nuclei, faster orbits and jumps,
  tighter size ranges, drifting nuclei, and new hazards. Boards past the first
  few are generated procedurally.

## Hazards (introduced by level)

| Level | Hazard | Behavior |
|------:|--------|----------|
| 2 | **Eye** | Opens on a random nucleus, pupil scanning for you; landing on it while open bursts you. |
| 2 | **Drift** | Nuclei begin drifting slowly around the dish. |
| 3 | **Bacterium** | A kidney-bean swimmer; jump into it and you deflect off in a new direction. |
| 4 | **Amoeba** | A slow, morphing background blob; jump too close and it electrocutes you with a jolt of lightning. |

## Controls

| Input | Action |
|-------|--------|
| Button 1 (title) | Start the game |
| Button 1 (hold) | Launch / hold a jump |
| Button 1 (release early) | Abort back to orbit (limited) |
| Button 1 (retry screen) | Try again |

The **Try Again** screen times out after ~15 seconds and returns to the title.

## Building

Requires the VectreC toolchain (CMOC + lwtools + Vectrex stdlib). Paths are set
in [`config.env`](config.env) and can be overridden with environment variables:

- `VECTREC` — path to the VectreC toolchain directory
- `VECX_APP` — path to the [Vecx](https://github.com/jhawthorn/vecx) emulator app (for `run.sh`)

```bash
./compile.sh        # build build/sphere-link.bin
./run.sh            # build, then launch the ROM in Vecx
./watch.sh          # rebuild on source changes
```

The output ROM (`build/sphere-link.bin`) runs on real Vectrex hardware via a
flash cart, or in any Vectrex emulator.

## Layout

```
src/main.c     game source (single translation unit)
Makefile       build rules
compile.sh     one-shot build
run.sh         build + launch in Vecx
watch.sh       rebuild-on-change loop
config.env     toolchain / emulator paths
```

## License

© 2026 Brian Jones.
