# ASCII Cube

Small real‑time 3D "ray‑marched" cube rendered as Unicode art in your terminal.  
Includes rain, mountains, a sun/light source, motion path, FPS/volume HUD and a tiny chiptune-style music loop.

## Demo

<video width="640" controls>
  <source src="https://github.com/Lallapallooza/c_ascii_render/raw/main/resources/demo.webm" type="video/webm">
  Your browser does not support the video tag.
</video>

## Build

Requirements:
- Linux
- `gcc` (or Clang) with C11
- `make`
- `aplay` (ALSA) for audio (optional)

```bash
make         # debug build to build/bin/ascii_cube
make release # optimized build
make clean   # remove build artifacts
```

## Run

```bash
./build/bin/ascii_cube [OPTIONS]
```

Useful options:
- `--size FLOAT`      cube half‑extent (default: `1.0`)
- `--speed FLOAT`     rotation speed multiplier (default: `1.0`)
- `--light-x FLOAT`   light X position (default: `-3.0`)
- `--light-y FLOAT`   light Y position (default: `4.5`)
- `--light-z FLOAT`   light Z position (default: `4.0`)
- `--max-steps INT`   raymarch steps (default: `100`)

## Controls

- `W/S` – rotate up / down  
- `A/D` – rotate left / right  
- `M`   – toggle orbiting motion path  
- `Scroll` or `+` / `-` – change music volume  
- `Q`   – quit
