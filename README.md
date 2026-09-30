# Game Boy Emulator

This project is a lightweight Game Boy emulator written in C with an SDL2 frontend for the window, keyboard input, and rendering. It focuses on the main hardware blocks needed to run a ROM: the CPU, memory controller, graphics pipeline, timer, and joypad input.

## What this emulator does

The emulator loads a Game Boy ROM, initializes the machine state, and then repeatedly advances the emulated CPU and supporting hardware until a frame is ready. Once a frame is complete, it draws the 160x144 framebuffer into an SDL window.

This is a learning-oriented emulator rather than a full-featured commercial-grade system. It covers the essentials for running many simple cartridges and is useful for understanding how Game Boy hardware works in practice.

## Architecture overview

This emulator is split into a few core modules that mirror the main Game Boy subsystems:

- `src/main.c`  
  Starts SDL, loads the ROM, initializes the machine state, runs the main emulation loop, and handles keyboard input and screen updates.

- `src/cpu.c`  
  Implements the LR35902 CPU. It fetches instructions from memory, decodes them, updates registers and flags, and reacts to interrupts and control flow.

- `src/memory.c`  
  Handles the memory map, cartridge ROM loading, banking, RAM, I/O ports, and joypad state. This is where MBC1, MBC3, and MBC5 behavior is handled.

- `src/gpu.c`  
  Emulates the PPU. It renders the background, window, and sprites into a frame buffer and marks when a full image is ready to display.

- `src/timer.c`  
  Simulates the Game Boy timer and DIV counter used to advance time-related hardware events.

- `src/gb.h`  
  Contains shared types, global state, and function declarations used across the project.

### Project layout

```text
gameboy/
├── Makefile
├── README.md
├── .gitignore
├── roms/
│   └── Cherry Rescue! v1.0.gb
└── src/
    ├── gb.h
    ├── main.c
    ├── cpu.c
    ├── memory.c
    ├── gpu.c
    └── timer.c
```

## How it works

The emulation loop in `main.c` follows a simple pattern:

1. Load the ROM and reset the emulator state.
2. Run the CPU instruction loop, which reads and writes memory as the program executes.
3. Advance the GPU and timers as cycles pass.
4. When a full frame is ready, copy the framebuffer into an SDL texture and display it.
5. Poll keyboard input and map it to the Game Boy joypad registers.

In other words, the emulator behaves like a small hardware simulation: the CPU talks to memory, the GPU renders pixels, the timer advances system time, and SDL provides the user-facing window and controls.

The code uses a cycle-driven approach so that CPU execution, PPU timing, and timers remain synchronized enough to produce a believable emulated frame.

## Important implementation details

- This project is designed as a learning-oriented emulator, not a complete high-compatibility Game Boy clone.
- Audio is intentionally not implemented.
- The display is a 160x144 framebuffer, matching the original Game Boy screen size.
- Cartridge support includes the common MBC1, MBC3, and MBC5 schemes.
- Joypad input is mapped from keyboard keys to Game Boy buttons.
- The emulator is intentionally simplified in some areas; not every hardware edge case is fully supported.
- ROMs should only be used if you have the legal right to do so.

## Requirements

- GCC or another compatible C compiler
- GNU Make
- SDL2 development libraries

On Windows, use the MSYS2 MinGW 64-bit environment with GCC, Make, and SDL2 installed. On Debian or Ubuntu, install the dependencies with:

```sh
sudo apt install build-essential make libsdl2-dev
```

## Build

From the project directory, run:

```sh
make
```

This creates `gb` (or `gb.exe` on Windows). To remove the generated executable:

```sh
make clean
```

## How to run it

### Linux / macOS

```sh
make
./gb "roms/Cherry Rescue! v1.0.gb"
```

### Windows (MSYS2 / MinGW)

```powershell
make
gb.exe "roms/Cherry Rescue! v1.0.gb"
```

If you are running the Windows build, keep `SDL2.dll` in the same folder as the executable.

You can also pass any compatible Game Boy ROM path instead of the sample ROM:

```sh
./gb "path/to/your/game.gb"
```

## Controls

| Game Boy input | Keyboard |
| --- | --- |
| D-pad | Arrow keys |
| A | Z |
| B | X |
| Select | Backspace or Right Shift |
| Start | Enter |
| Quit | Escape |

## Notes

This project is especially useful if you want to learn how emulators are structured: a ROM loader, a CPU core, memory mapping, an interrupt/timing system, and a display pipeline all working together. It is a solid example of a small hardware emulator built in C.

Use ROM files you are legally allowed to use. Check the redistribution rights for any ROM before publishing or sharing it.