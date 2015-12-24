#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

#include <stdbool.h>

typedef bool Cell;

enum
{
    CELL_EMPTY = false,
    CELL_ALIVE = true
};

typedef struct Borders
{
    Cell *top_side;
    Cell *left_side;
    Cell *right_side;
    Cell *bottom_side;

    //the following pointers used only for outer border
    //(because in the inner border sides includes angles)
    Cell *tl_angle; //top left
    Cell *tr_angle; //top right
    Cell *bl_angle; //botton left
    Cell *br_angle; //bottom right
} Borders;

typedef struct Frame
{
    //without outer borders
    unsigned width;
    unsigned height;

    //with outer borders
    unsigned full_width;
    unsigned full_height;

    Cell **data;

    Borders *inner_borders; //doesn't used in calculations in this frame, but must be updated
    Borders *outer_borders;
} Frame;

typedef struct Chunk
{
    Frame **frames;
    Frame *cur_frame;

    unsigned frames_count;
    unsigned cur_frame_num;

    unsigned width;
    unsigned height;

    unsigned undo_depth;
} Chunk;

//main functions
Frame *frame_create(unsigned, unsigned, Borders *, Borders *);
void frame_destroy(Frame *); //calls automatically in chunk_destroy

Chunk *chunk_create(unsigned, ...);
void chunk_destroy(Chunk *);

Frame *chunk_switch_next_frame(Chunk *);

//low-level functions (unsafe)
void frame_update_outer_borders(Frame *);
void frame_update_inner_borders(Frame *);
bool frame_calc(Frame *, Frame *); //(will not update borders)

//high-level functions (will update borders automatically and check parameters for errors)
//all of this fuctions will return false or NULL in case of fail (unless otherwise specified)
void frame_clear(Frame *);
char *frame_render_line(Frame *, unsigned);
bool frame_load_line(Frame *, char *, unsigned);

bool frame_set_cell(Frame *, unsigned, unsigned, Cell);
unsigned frame_cells_count(Frame *); //number of cells who are still alive

bool chunk_do_turn(Chunk *); //returns false if field is stable
bool chunk_undo_turn(Chunk *);
void chunk_clear(Chunk *);

//all functions will not work correctly with unitialized Frane * or Chunk * pointers

#endif //CORE_H_INCLUDED
