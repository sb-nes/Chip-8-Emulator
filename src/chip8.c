#include "chip8.h"
#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>

const char chip8_default_character_set[] = {
    0xf0, 0x90, 0x90, 0x90, 0xf0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xf0, 0x10, 0xf0, 0x80, 0xf0, //2
    0xf0, 0x10, 0xf0, 0x10, 0xf0, //3
    0x90, 0x90, 0xf0, 0x10, 0x10, //4
    0xf0, 0x80, 0xf0, 0x10, 0xf0, //5
    0xf0, 0x80, 0xf0, 0x90, 0xf0, //6
    0xf0, 0x10, 0x20, 0x40, 0x40, //7
    0xf0, 0x90, 0xf0, 0x90, 0xf0, //8
    0xf0, 0x90, 0xf0, 0x10, 0xf0, //9
    0xf0, 0x90, 0xf0, 0x90, 0x90, //a
    0xe0, 0x90, 0xe0, 0x90, 0xe0, //b
    0xf0, 0x80, 0x80, 0x80, 0xf0, //c
    0xe0, 0x90, 0x90, 0x90, 0xe0, //d
    0xf0, 0x80, 0xf0, 0x80, 0xf0, //e
    0xf0, 0x80, 0xf0, 0x80, 0x80, //f
};

void chip8_init(struct chip8* chip8){
    memset(chip8, 0, sizeof(struct chip8));
    memcpy(&chip8->memory.memory, chip8_default_character_set, sizeof(chip8_default_character_set));
}

void chip8_load(struct chip8* chip8, const char* buf, size_t size){
    assert(size+CHIP8_PROGRAM_LOAD_ADDRESS < CHIP8_MEMORY_SIZE);
    memcpy(&chip8->memory.memory[CHIP8_PROGRAM_LOAD_ADDRESS], buf, size);
    chip8->registers.PC = CHIP8_PROGRAM_LOAD_ADDRESS;
}

static void eight_xyn(struct chip8* chip8, unsigned short opcode){
    unsigned char x = (opcode >> 8) & 0x000f;
    unsigned char y = (opcode >> 4) & 0x000f;
    unsigned char final_four_bits = opcode & 0x000f;
    unsigned short temp = 0;

    switch(final_four_bits){
        case 0x00://8xy0 - LD Vx, Vy - Sets the value of Vx as Vy
            chip8->registers.V[x] = chip8->registers.V[y];
        break;

        case 0x01://8xy1 - OR Vx, Vy - Performs bitwise OR on Vx and Vy, stores in Vx
            chip8->registers.V[x] = chip8->registers.V[x] | chip8->registers.V[y];
        break;

        case 0x02://8xy2 - AND Vx, Vy - Performs bitwise AND on Vx and Vy, stores in Vx
            chip8->registers.V[x] = chip8->registers.V[x] & chip8->registers.V[y];
        break;

        case 0x03://8xy3 - XOR Vx, Vy - Performs bitwise XOR on Vx and Vy, stores in Vx
            chip8->registers.V[x] = chip8->registers.V[x] ^ chip8->registers.V[y];
        break;

        case 0x04://8xy4 - ADD Vx, Vy - Add Vx and Vy, stores in Vx and carry in VF
            temp = chip8->registers.V[x] + chip8->registers.V[y];
            chip8->registers.V[0x0f] = 0;
            if(temp>0xff){
                chip8->registers.V[0x0f] = 1;
            }

            chip8->registers.V[x] = temp;
        break;

        case 0x05://8xy5 - SUB Vx, Vy - Subtract Vx by Vy, stores in Vx and set VF as 'Not Borrow'
            chip8->registers.V[0x0f] = 0;
            if(chip8->registers.V[x] > chip8->registers.V[y]){
                chip8->registers.V[0x0f] = 1;
            }
            chip8->registers.V[x] = chip8->registers.V[x] - chip8->registers.V[y];
        break;

        case 0x06://8xy6 - SHR Vx {, Vy} - LSB
            chip8->registers.V[0x0f] = chip8->registers.V[x] & 0x01;
            chip8->registers.V[x] /= 2;
        break;

        case 0x07://8xy7 - SUBN Vx, Vy - Subtract Vy by Vx, stores in Vx and set VF as 'Not Borrow'
            chip8->registers.V[0x0f] = chip8->registers.V[y] > chip8->registers.V[x];
            chip8->registers.V[x] = chip8->registers.V[y] - chip8->registers.V[x];
        break;

        case 0x0E://8xyE - SHL Vx {, Vy} - MSB
            chip8->registers.V[0x0f] = chip8->registers.V[x] & 0b10000000;
            chip8->registers.V[x] *= 2;
        break;
    }
}

static char chip8_wait_for_key_press(struct chip8* chip8){
    SDL_Event event;
    while (SDL_WaitEvent(&event)){
        if(event.type != SDL_KEYDOWN)
            continue;
        char c = event.key.keysym.sym;
        char chip8_key = chip8_keyboard_map(&chip8->keyboard, c);
        if(chip8_key != 1){
            return chip8_key;
        }
    }
    return -1;
}

static void F_x(struct chip8* chip8, unsigned short opcode){
    unsigned char x = (opcode >> 8) & 0x000f;
    switch (opcode & 0x00ff){
        case 0x07://LD Vx, DT - Fx07 - Set Vx to delay timer value
            chip8->registers.V[x] = chip8->registers.delay_timer;
        break;

        case 0x0A:{//LD Vx K - Fx0A
            char pressed_key = chip8_wait_for_key_press(chip8);
            chip8->registers.V[x] = pressed_key;
        }
        break;

        case 0x15://LD DT, Vx - Fx15 - Set delay timer to Vx value
            chip8->registers.delay_timer = chip8->registers.V[x];
        break;

        case 0x18://LD ST, Vx - Fx18 - set sound timer to Vx value
            chip8->registers.sound_timer = chip8->registers.V[x];
        break;

        case 0x1e://ADD I, Vx - Fx1E - adds
            chip8->registers.I += chip8->registers.V[x];
        break;

        case 0x29://LD F, Vx - Fx29 - Loads sprite of Vx to I
            chip8->registers.I = chip8->registers.V[x] * CHIP8_DEFAULT_SPRITE_HEIGHT;
        break;

        case 0x33:{//LD B, Vx - Fx33
            unsigned char hundreds = chip8->registers.V[x] / 100;
            unsigned char tens = chip8->registers.V[x] /10 % 10;
            unsigned char units = chip8->registers.V[x] % 10;
            chip8_memory_set(&chip8->memory, chip8->registers.I, hundreds);
            chip8_memory_set(&chip8->memory, chip8->registers.I+1, tens);
            chip8_memory_set(&chip8->memory, chip8->registers.I+2, units);
        }
        break;

        case 0x55:{//LD [I], Vx - Fx55 - Loads sprite of Vx to I
            for(int i = 0; i <= x; i++){
                chip8_memory_set(&chip8->memory, chip8->registers.I+i, chip8->registers.V[i]);
            }
        }
        break;

        case 0x65:{//LD Vx, [I] - Fx65 - Reads registers from V0 through Vx from memory starting at I
            for(int i = 0; i <= x; i++){
                chip8->registers.V[i] = chip8_memory_get(&chip8->memory, chip8->registers.I+i);
            }
        }
        break;
    }
}

static void chip8_exec_extended(struct chip8* chip8, unsigned short opcode){
    unsigned short nnn = opcode & 0x0fff;
    unsigned char x = (opcode >> 8) & 0x000f;
    unsigned char y = (opcode >> 4) & 0x000f;
    unsigned char kk = opcode & 0x00ff;
    unsigned char n = opcode & 0x000f;

    switch(opcode & 0xf000){
        case 0x1000://JP addr - 1nnn - Jump to location 'nnn'
            chip8->registers.PC = nnn;
        break;

        case 0x2000://CALL addr - 2nnn - Call subroutine at location 'nnn'
            chip8_stack_push(chip8, chip8->registers.PC);
            chip8->registers.PC = nnn;
        break;

        case 0x3000://SE Vx, byte - 3xkk - Skip next instruction if Vx = kk
            if(chip8->registers.V[x] == kk){
                chip8->registers.PC += 2;
            }
        break;

        case 0x4000://SNE Vx, byte - 4xkk - Skip next instruction if Vx != kk
            if(chip8->registers.V[x] != kk){
                chip8->registers.PC += 2;
            }
        break;

        case 0x5000://SE Vx, Vy - 5xy0 - Skip next instruction if Vx = Vy
            if(chip8->registers.V[x] == chip8->registers.V[y]){
                chip8->registers.PC += 2;
            }
        break;

        case 0x6000://LD Vx, byte - 6xkk - Loads instruction Vx = kk
            chip8->registers.V[x] = kk;
        break;

        case 0x7000://ADD Vx, byte - 7xkk - Adds value kk to Vx : Vx = Vx +kk
            chip8->registers.V[x] += kk;
        break;

        case 0x8000:
            eight_xyn(chip8, opcode);
        break;

        case 0x9000://SNE Vx, Vy - 9xy0 - Skip next instruction if Vx != Vy
            if(chip8->registers.V[x] != chip8->registers.V[y]){
                chip8->registers.PC += 2;
            }
        break;

        case 0xA000://LD I, addr - Annn - sets I register to nnn
            chip8->registers.I = nnn;
        break;

        case 0xB000://JUMP V0, addr - Bnnn - jump to location V0+nnn
            chip8->registers.PC = nnn + chip8->registers.V[0x00];
        break;

        case 0xC000://RND Vx, byte - Cxkk - generates a random number from 0 to 255, AND'ed with kk and stored in V[x]
            srand(clock());
            chip8->registers.V[x] = (rand() % 255) & kk;
        break;

        case 0xD000:{//DRW Vx Vy nibble - Dxyn - Draws Sprite
            const char* sprite = (const char*) &chip8->memory.memory[chip8->registers.I];
            chip8->registers.V[0x0f] = chip8_screen_draw_sprite(&chip8->screen, chip8->registers.V[x], chip8->registers.V[y], sprite, n);
        }
        break;

        //Keyboard Events
        case 0xE000:{
            switch(opcode & 0x00ff){
                case 0x9e://SKP Vx - Ex9e - skip next instruction if the key with value of Vx is Pressed
                    if(chip8_keyboard_is_down(&chip8->keyboard, chip8->registers.V[x])){
                        chip8->registers.PC += 2;
                    }
                break;

                case 0xa1://SKNP Vx - Exa1 - skip next instruction if the key with value of x is not Pressed
                    if(!chip8_keyboard_is_down(&chip8->keyboard, chip8->registers.V[x])){
                        chip8->registers.PC += 2;
                    }
                break;
            }
        }
        break;

        case 0xF000:
            F_x(chip8, opcode);
        break;
    }
}

void chip8_exec(struct chip8* chip8, unsigned short opcode){
    switch(opcode){
        case 0x00E0://CLS - Clear the Screen
            chip8_screen_clear(&chip8->screen);
        break;

        case 0x00EE://RET - Return from Subroutine
            chip8->registers.PC = chip8_stack_pop(chip8);
        break;

        default:
            chip8_exec_extended(chip8, opcode);
        break;
    }
}