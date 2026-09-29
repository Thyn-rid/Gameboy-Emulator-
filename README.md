# Game Boy Emulator

A small Game Boy emulator written in C, with an SDL2 window for displaying the screen and reading keyboard input. It emulates the CPU, memory, graphics, and timer; audio is not implemented. Cartridge banking support includes MBC1, MBC3, and MBC5.

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

## Run

Pass the path to a Game Boy ROM file:

```sh
./gb "roms/Cherry Rescue! v1.0.gb"
```

On Windows, run `gb.exe` instead of `./gb`. The included `SDL2.dll` should remain beside the executable when running on Windows.

Use ROM files you are legally allowed to use. Check the redistribution rights for any ROM before publishing or sharing it.

## Controls

| Game Boy input | Keyboard |
| --- | --- |
| D-pad | Arrow keys |
| A | Z |
| B | X |
| Select | Backspace or Right Shift |
| Start | Enter |
| Quit | Escape |