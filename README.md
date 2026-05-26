# CHIP-8 Emulator

A CHIP-8 emulator written in C, using SDL2 for graphics and input.

This is my first emulator and my first big project in C. I built it to learn how emulation
works: decoding opcodes, managing memory, registers, the stack, timers, and drawing sprites.

## Features

- All 35 standard CHIP-8 opcodes
- 64x32 monochrome display, scaled 10x
- Sprite drawing with XOR collision detection and screen wrap-around
- Delay and sound timers at 60 Hz
- Built-in hexadecimal font set loaded at address 0x000
- Programs loaded at address 0x200

## Requirements

- A C compiler (gcc, clang, ...)
- SDL2 development libraries

Debian/Ubuntu: `sudo apt install libsdl2-dev`
macOS (Homebrew): `brew install sdl2`
Windows (MSYS2): `pacman -S mingw-w64-x86_64-SDL2`

## Building

```sh
gcc main.c -o main -lmingw32 -lSDL2main -lSDL2
```

## Usage

1. Put a CHIP-8 ROM (a `.ch8` file) in the same folder as the executable.
2. Run `./main`.
3. At the `GAME NAME:` prompt, type the ROM name without the `.ch8` extension
   (for `pong.ch8`, type `pong`).

## Controls

The 16-key CHIP-8 keypad is mapped to the left side of the keyboard, following the usual layout:

```
CHIP-8 keypad        Keyboard
1 2 3 C              1 2 3 4
4 5 6 D      ->      Q W E R
7 8 9 E              A S D F
A 0 B F              Z X C V
```

Close the window to quit.

## Acknowledgements

Thanks to Thomas P. Greene's CHIP-8 technical reference, my main guide for the instruction
set and the inner workings of the machine:

http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
