#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SCALE   10
#define WIDTH   64
#define HEIGHT  32

typedef struct {
    uint8_t  memory[4096];
    uint8_t  V[16];                   // registers V0..VF, VF is the flag register
    uint16_t I;                       // index register: holds a memory address
    uint16_t pc;                      // program counter
    uint8_t  display[WIDTH * HEIGHT]; // 0 = off, 1 = on
    uint8_t  delay_timer;
    uint8_t  sound_timer;
    uint8_t  keys[16];
    uint8_t  sp;
    uint16_t stack[16];               // return addresses are 16-bit, so the stack must be too
} Chip8;

size_t load_rom(Chip8 *c, const char *path) {
    FILE *rom = fopen(path, "rb");
    if (rom == NULL) {
        printf("Can't open ROM!\n");
        return 0;
    }
    size_t length = fread(&c->memory[0x200], 1, 4096 - 0x200, rom);  // program loads at 0x200
    fclose(rom);
    printf("ROM loaded: %zu bytes\n", length);
    return length;
}

void execute_opcode(Chip8 *c) {
    uint16_t basePc = c->pc;
    uint16_t opcode = (c->memory[c->pc] << 8) + c->memory[c->pc + 1];  // 2 bytes = 1 opcode
    uint16_t family = (opcode & 0xF000) >> 12;

    switch (family) {

        case 0x0:
            switch (opcode & 0x00FF) {
                case 0xE0:  // 00E0: clear screen
                    memset(c->display, 0, sizeof(c->display));
                    break;
                case 0xEE:  // 00EE: return from subroutine
                    if (c->sp > 0) {
                        c->sp--;
                        c->pc = c->stack[c->sp];
                    }
                    break;
                default:
                    printf("[0x__] unknown 0x0 opcode: 0x%04X (pc=0x%03X)\n", opcode, basePc);
                    break;
            }
            break;

        case 0x1:  // 1nnn: jump to nnn
            c->pc = opcode & 0x0FFF;
            break;

        case 0x2: {  // 2nnn: call subroutine at nnn
            c->stack[c->sp] = c->pc + 2;
            c->sp++;
            c->pc = opcode & 0x0FFF;
            break;
        }

        case 0x3: {  // 3xkk: skip next if V[x] == kk
            uint8_t x  = (opcode & 0x0F00) >> 8;
            uint8_t kk =  opcode & 0x00FF;
            if (c->V[x] == kk)
                c->pc += 4;
            break;
        }

        case 0x4: {  // 4xkk: skip next if V[x] != kk
            uint8_t x  = (opcode & 0x0F00) >> 8;
            uint8_t kk =  opcode & 0x00FF;
            if (c->V[x] != kk)
                c->pc += 4;
            break;
        }

        case 0x5: {  // 5xy0: skip next if V[x] == V[y]
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            if (c->V[x] == c->V[y])
                c->pc += 4;
            break;
        }

        case 0x6: {  // 6xkk: V[x] = kk
            uint8_t x  = (opcode & 0x0F00) >> 8;
            uint8_t kk =  opcode & 0x00FF;
            c->V[x] = kk;
            break;
        }

        case 0x7: {  // 7xkk: V[x] += kk
            uint8_t x  = (opcode & 0x0F00) >> 8;
            uint8_t kk =  opcode & 0x00FF;
            c->V[x] += kk;
            break;
        }

        case 0x8: {  // 8xy_: arithmetic/logic between V[x] and V[y]
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            switch (opcode & 0x000F) {
                case 0x0: c->V[x] =  c->V[y]; break;
                case 0x1: c->V[x] |= c->V[y]; break;
                case 0x2: c->V[x] &= c->V[y]; break;
                case 0x3: c->V[x] ^= c->V[y]; break;
                case 0x4: {
                    uint16_t sum = c->V[x] + c->V[y];
                    c->V[0xF] = (sum > 255) ? 1 : 0;          // carry
                    c->V[x] = sum;
                    break;
                }
                case 0x5:
                    c->V[0xF] = (c->V[x] >= c->V[y]) ? 1 : 0;  // no borrow
                    c->V[x] = c->V[x] - c->V[y];
                    break;
                case 0x6:                                      // 8xy6: shift right
                    c->V[0xF] = c->V[x] & 1;                    // VF = bit lost
                    c->V[x] >>= 1;
                    break;
                case 0x7:                                      // 8xy7: V[x] = V[y] - V[x]
                    c->V[0xF] = (c->V[y] >= c->V[x]) ? 1 : 0;
                    c->V[x] = c->V[y] - c->V[x];
                    break;
                case 0xE:                                      // 8xyE: shift left
                    c->V[0xF] = (c->V[x] >> 7) & 1;             // VF = bit lost
                    c->V[x] <<= 1;
                    break;
                default:
                    printf("[8xy_] unknown ALU op: 0x%04X (pc=0x%03X)\n", opcode, basePc);
                    break;
            }
            break;
        }

        case 0x9: {  // 9xy0: skip next if V[x] != V[y]
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            if (c->V[x] != c->V[y])
                c->pc += 4;
            break;
        }

        case 0xA:  // Annn: I = nnn
            c->I = opcode & 0x0FFF;
            break;

        case 0xB:  // Bnnn: jump to nnn + V0
            c->pc = (opcode & 0x0FFF) + c->V[0];
            break;

        case 0xC: {  // Cxkk: V[x] = random & kk
            uint8_t x  = (opcode & 0x0F00) >> 8;
            uint8_t kk =  opcode & 0x00FF;
            c->V[x] = (rand() % 256) & kk;
            break;
        }

        case 0xD: {  // Dxyn: draw n-row sprite from memory[I] at (V[x], V[y])
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            uint8_t n =  opcode & 0x000F;

            //printf("DRAW at (%d,%d) n=%d  I=0x%03X\n", c->V[x], c->V[y], n, c->I);

            uint8_t Vx = c->V[x];
            uint8_t Vy = c->V[y];
            c->V[0xF] = 0;

            for (int line = 0; line < n; line++) {
                uint8_t sprite_line = c->memory[c->I + line];  // one byte = one row of 8 pixels
                for (int col = 0; col < 8; col++) {
                    uint8_t bit = (sprite_line >> (7 - col)) & 1;
                    if (bit == 1) {
                        int px = (Vx + col)  % WIDTH;   // wrap around the edges
                        int py = (Vy + line) % HEIGHT;
                        int index = py * WIDTH + px;

                        if (c->display[index] == 1)
                            c->V[0xF] = 1;  // a lit pixel is turned off -> collision

                        c->display[index] ^= 1;  // XOR toggles the pixel
                    }
                }
            }
            break;
        }

        case 0xE: {  // Ex__: skip depending on key V[x]
            uint8_t x = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x9E:  // Ex9E: skip if key V[x] is pressed
                    if (c->keys[c->V[x]] == 1)
                        c->pc += 4;
                    break;
                case 0xA1:  // ExA1: skip if key V[x] is NOT pressed
                    if (c->keys[c->V[x]] == 0)
                        c->pc += 4;
                    break;
                default:
                    printf("[Ex__] unknown key op: 0x%04X (pc=0x%03X)\n", opcode, basePc);
                    break;
            }
            break;
        }

        case 0xF: {  // Fx__: misc
            uint8_t x = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x07: c->V[x] = c->delay_timer; break;   // Fx07: read delay timer
                case 0x0A: {                                   // Fx0A: wait for a key press
                    int pressed = -1;
                    for (int i = 0; i < 16; i++)
                        if (c->keys[i]) pressed = i;
                    if (pressed == -1)
                        c->pc -= 2;       // no key: stay on this instruction
                    else
                        c->V[x] = pressed;
                    break;
                }
                case 0x15: c->delay_timer = c->V[x]; break;   // Fx15: set delay timer
                case 0x18: c->sound_timer = c->V[x]; break;   // Fx18: set sound timer
                case 0x1E: c->I += c->V[x]; break;            // Fx1E: I += V[x]
                case 0x29: c->I = c->V[x] * 5; break;         // Fx29: I = address of digit sprite
                case 0x33:                                    // Fx33: store BCD of V[x]
                    c->memory[c->I]     =  c->V[x] / 100;
                    c->memory[c->I + 1] = (c->V[x] / 10) % 10;
                    c->memory[c->I + 2] =  c->V[x] % 10;
                    break;
                case 0x55:                                    // Fx55: store V0..Vx into memory
                    for (int i = 0; i <= x; i++)
                        c->memory[c->I + i] = c->V[i];
                    break;
                case 0x65:                                    // Fx65: load V0..Vx from memory
                    for (int i = 0; i <= x; i++)
                        c->V[i] = c->memory[c->I + i];
                    break;
                default:
                    printf("[Fx__] unknown misc op: 0x%04X (pc=0x%03X)\n", opcode, basePc);
                    break;
            }
            break;
        }

        default:
            printf("[????] unknown opcode family: 0x%04X (pc=0x%03X)\n", opcode, basePc);
            break;
    }

    // advance to the next opcode, unless a jump/skip already moved pc
    if (basePc == c->pc)
        c->pc += 2;
}

void draw_display(SDL_Renderer *renderer, uint8_t *display) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        if (display[i] == 1) {
            SDL_Rect tile;
            tile.x = (i % WIDTH) * SCALE;
            tile.y = (i / WIDTH) * SCALE;
            tile.w = SCALE;
            tile.h = SCALE;
            SDL_RenderFillRect(renderer, &tile);
        }
    }
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow(
        "CHIP-8",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH * SCALE, HEIGHT * SCALE,
        0
    );
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

    uint8_t fontset[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
        0x20, 0x60, 0x20, 0x20, 0x70,  // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
        0xF0, 0x80, 0xF0, 0x80, 0x80   // F
    };

    Chip8 c = {0};
    c.pc = 0x200;
    memcpy(c.memory, fontset, 80);   // font lives at address 0x00 (Fx29 uses V[x]*5)

    char gamename[100];
    char ext[] = ".ch8";

    printf("GAME NAME:\n");
    scanf("%99s", gamename);

    int len = strlen(gamename);

    for (int i = 0; ext[i] != '\0'; i++) {
        gamename[len + i] = ext[i];
    }

    gamename[len + strlen(ext)] = '\0';

    printf("LOAD %s\n", gamename);

    if (load_rom(&c, gamename) == 0)
        return 1;

    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                int pressed = (event.type == SDL_KEYDOWN) ? 1 : 0;
                switch (event.key.keysym.sym) {
                    case SDLK_1: c.keys[0x1] = pressed; break;
                    case SDLK_2: c.keys[0x2] = pressed; break;
                    case SDLK_3: c.keys[0x3] = pressed; break;
                    case SDLK_4: c.keys[0xC] = pressed; break;
                    case SDLK_q: c.keys[0x4] = pressed; break;
                    case SDLK_w: c.keys[0x5] = pressed; break;
                    case SDLK_e: c.keys[0x6] = pressed; break;
                    case SDLK_r: c.keys[0xD] = pressed; break;
                    case SDLK_a: c.keys[0x7] = pressed; break;
                    case SDLK_s: c.keys[0x8] = pressed; break;
                    case SDLK_d: c.keys[0x9] = pressed; break;
                    case SDLK_f: c.keys[0xE] = pressed; break;
                    case SDLK_z: c.keys[0xA] = pressed; break;
                    case SDLK_x: c.keys[0x0] = pressed; break;
                    case SDLK_c: c.keys[0xB] = pressed; break;
                    case SDLK_v: c.keys[0xF] = pressed; break;
                }
            }
            if (event.type == SDL_QUIT)
                running = 0;
        }

        if (c.delay_timer > 0) c.delay_timer--;
        if (c.sound_timer > 0) c.sound_timer--;

        for (int i = 0; i < 10; i++)   // several opcodes per frame for a sane speed
            execute_opcode(&c);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        draw_display(renderer, c.display);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);  // 60 fps
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
