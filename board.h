#ifndef BOARD_H_INCLUDED
#define BOARD_H_INCLUDED

#include <stdio.h>
#include <sys/types.h>

typedef enum Instruction_code
{
    INSTRUCTION_NOP,

    INSTRUCTION_DESTROY,

    INSTRUCTION_ADD_CELL,
    INSTRUCTION_WRITE_SCANLINE,
    INSTRUCTION_READ_SCANLINE,

    INSTRUCTION_UPDATE_INNER_BORDERS,
    INSTRUCTION_UPDATE_OUTER_BORDERS,
    INSTRUCTION_CALCULATE,
    INSTRUCTION_CLEAR
} Instruction_code;

enum
{
    CHUNK_NUM_ANY = 0xFFFFFFFF
};

typedef struct Instruction
{
    Instruction_code id;
    unsigned chunk_num_x;
    unsigned chunk_num_y;
    unsigned param1;
    unsigned param2;
} Instruction;

typedef struct Border_ids
{
    int top;
    int left;
    int right;
    int bottom;

    //shared memory for communication with father
    //it can be top, bottom, or, if field consists of 1 chunk, special area
    int special;
} Border_ids;

typedef struct Board
{
    pid_t **chunks;
    Border_ids ***border_ids;

    char ***special_pointers;

    unsigned width;
    unsigned height;

    unsigned chunks_count;
    unsigned chunks_hor_count;
    unsigned chunks_ver_count;

    unsigned chunk_size;
    unsigned last_width;
    unsigned last_height;

    unsigned long long generation_num;

    int sem_id;
    int shm_id;

    Instruction *cur_instruction;
} Board;

Board *board_create(unsigned, unsigned, unsigned);
void board_destroy(Board *);

bool board_add_cell(Board *, unsigned, unsigned);
void board_next_turn(Board *);
void board_clear(Board *);

char *board_get_scanline(Board *, unsigned);
bool board_set_scanline(Board *, unsigned, char *);

bool board_load_from_file(Board *, FILE *);
bool board_save_to_file(Board *, FILE *);

#endif //BOARD_H_INCLUDED
