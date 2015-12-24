#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "core.h"

Frame *
frame_create(
    unsigned width,
    unsigned height,
    Borders *inner_borders,
    Borders *outer_borders)
{
    if (!width || !height) {
        return NULL;
    }

    Frame *result = calloc(1, sizeof(*result));

    result->width = width;
    result->height = height;

    result->full_width = width + 2;
    result->full_height = height + 2;

    result->data = calloc(result->full_height, sizeof(*result->data));
    for (unsigned i = 0; i < result->full_height; i++) {
        result->data[i] = calloc(result->full_width, sizeof(**result->data));
    }

    result->inner_borders = inner_borders;
    result->outer_borders = outer_borders;

    frame_update_inner_borders(result);

    return result;
}

static inline Cell
safe_deref(Cell *pointer)
{
    return pointer == NULL ? 0 : *pointer;
}

void
frame_update_outer_borders(Frame *frame)
{
    Borders *borders = frame->outer_borders;
    if (borders == NULL) {
        return;
    }

    unsigned frst_row = 0;
    unsigned frst_col = 0;
    unsigned last_row = frame->full_height - 1;
    unsigned last_col = frame->full_width - 1;

    frame->data[frst_row][frst_col] = safe_deref(borders->tl_angle);
    frame->data[frst_row][last_col] = safe_deref(borders->tr_angle);
    frame->data[last_row][frst_col] = safe_deref(borders->bl_angle);
    frame->data[last_row][last_col] = safe_deref(borders->br_angle);

    if (borders->top_side != NULL) {
        memcpy(
            frame->data[frst_row] + 1,
            borders->top_side,
            frame->width * sizeof(**frame->data));
    }

    if (borders->bottom_side != NULL) {
        memcpy(
            frame->data[last_row] + 1,
            borders->bottom_side,
            frame->width * sizeof(**frame->data));
    }

    if (borders->left_side != NULL) {
        for (unsigned i = 1; i <= frame->height; i++) {
            frame->data[i][frst_col] = borders->left_side[i - 1];
        }
    }

    if (borders->right_side != NULL) {
        for (unsigned i = 1; i <= frame->height; i++) {
            frame->data[i][last_col] = borders->right_side[i - 1];
        }
    }
}

void
frame_update_inner_borders(Frame *frame)
{
    Borders *borders = frame->inner_borders;
    if (borders == NULL) {
        return;
    }

    if (borders->top_side != NULL) {
        memcpy(
            borders->top_side,
            frame->data[1] + 1,
            frame->width * sizeof(**frame->data));
    }

    if (borders->bottom_side != NULL) {
        memcpy(
            borders->bottom_side,
            frame->data[frame->height] + 1,
            frame->width * sizeof(**frame->data));
    }

    if (borders->left_side != NULL) {
        for (unsigned i = 1; i <= frame->height; i++) {
            borders->left_side[i - 1] = frame->data[i][1];
        }
    }

    if (borders->right_side != NULL) {
        for (unsigned i = 1; i <= frame->height; i++) {
            borders->right_side[i - 1] = frame->data[i][frame->width];
        }
    }
}

bool
frame_set_cell(Frame *frame, unsigned x, unsigned y, Cell value)
{
    if (x < 1 || x > frame->width) {
        return false;
    }
    if (y < 1 || y > frame->height) {
        return false;
    }

    frame->data[y][x] = value;

    //update borders
    Borders *borders = frame->inner_borders;
    if (borders == NULL) {
        return true;
    }

    if (x == 1 && borders->left_side != NULL) {
        borders->left_side[y - 1] = value;
    }
    if (x == frame->width && borders->right_side != NULL) {
        borders->right_side[y - 1] = value;
    }

    if (y == 1 && borders->top_side != NULL) {
        borders->top_side[x - 1] = value;
    }
    if (y == frame->height && borders->bottom_side != NULL) {
        borders->bottom_side[x - 1] = value;
    }

    return true;
}

static inline Cell
process_cell(Cell old_value, int neighbours_count)
{
    switch (neighbours_count) {
    case 3:
        return true;
    case 2:
        return old_value;
    default:
        return false;
    }
}

bool
frame_calc(Frame *frame, Frame *prev_frame)
{
    bool stable = true;

    Cell *prev_line = prev_frame->data[0];
    Cell *cur_line = prev_frame->data[1];
    Cell *next_line, *output_line;

    int neighbours_count;
    for (unsigned j = 1; j <= frame->height; j++) {
        next_line = prev_frame->data[j + 1];
        output_line = frame->data[j];

        for (unsigned i = 1; i <= frame->width; i++) {
            neighbours_count = -cur_line[i];
            for (unsigned k = i - 1; k <= i + 1; k++) {
                neighbours_count += prev_line[k];
                neighbours_count += cur_line[k];
                neighbours_count += next_line[k];
            }
            output_line[i] = process_cell(cur_line[i], neighbours_count);

            stable = stable && output_line[i] == cur_line[i];
        }

        prev_line = cur_line;
        cur_line = next_line;
    }

    return !stable;
}

unsigned
frame_cells_count(Frame *frame)
{
    unsigned result = 0;
    for (unsigned j = 1; j <= frame->height; j++) {
        for (unsigned i = 1; i <= frame->width; i++) {
            result += frame->data[j][i];
        }
    }
    return result;
}

void
frame_clear(Frame *frame)
{
    for (unsigned i = 1; i <= frame->height; i++) {
        memset(frame->data[i] + 1, 0, frame->width * sizeof(*frame->data[i]));
    }
    frame_update_inner_borders(frame);
}

char *
frame_render_line(Frame *frame, unsigned y)
{
    if (y < 1 || y > frame->height) {
        return NULL;
    }

    static char map[2] = {'.', '*'};

    char *result = calloc(frame->width + 1, sizeof(*result));
    for (unsigned i = 1; i <= frame->width; i++) {
        result[i - 1] = map[frame->data[y][i]];
    }
    return result;
}

bool
frame_load_line(Frame *frame, char *line, unsigned y)
{
    if (y < 1 || y > frame->height) {
        return false;
    }
    if (line == NULL || strlen(line) != frame->width) {
        return false;
    }

    for (unsigned i = 1; i <= frame->width; i++) {
        frame->data[y][i] = line[i - 1] == '*';
    }

    frame_update_inner_borders(frame);
    return true;
}

void
frame_destroy(Frame *frame)
{
    for (unsigned i = 0; i < frame->full_height; i++) {
        free(frame->data[i]);
    }
    free(frame->data);

    free(frame);
}



static inline void
chunk_update_cur_frame(Chunk *chunk)
{
    chunk->cur_frame = chunk->frames[chunk->cur_frame_num];
}

Chunk *
chunk_create(unsigned n, ...)
{
    if (n < 2) {
        return NULL;
    }

    Chunk *result = calloc(1, sizeof(*result));
    result->frames = calloc(n, sizeof(*result->frames));
    result->frames_count = n;

    Frame *cur_frame;

    va_list arguments;
    va_start(arguments, n);
    for (unsigned i = 0; i < n; i++) {
        cur_frame = va_arg(arguments, Frame *);
        result->frames[i] = cur_frame;
        if (i == 0) {
            result->width = cur_frame->width;
            result->height = cur_frame->height;
        } else {
            if (result->width != cur_frame->width || result->height != cur_frame->height) {
                free(result->frames);
                free(result);
                return NULL;
            }
        }
    }
    va_end(arguments);

    chunk_update_cur_frame(result);
    return result;
}

Frame *
chunk_switch_next_frame(Chunk *chunk)
{
    chunk->cur_frame_num++;
    if (chunk->cur_frame_num == chunk->frames_count) {
        chunk->cur_frame_num = 0;
    }
    if (chunk->undo_depth < chunk->frames_count - 1) {
        chunk->undo_depth++;
    }

    chunk_update_cur_frame(chunk);
    return chunk->cur_frame;
}

bool
chunk_do_turn(Chunk *chunk)
{
    bool result;

    Frame *prev_frame = chunk->cur_frame;
    Frame *cur_frame = chunk_switch_next_frame(chunk);

    frame_update_outer_borders(prev_frame);
    result = frame_calc(cur_frame, prev_frame);
    frame_update_inner_borders(cur_frame);

    chunk_update_cur_frame(chunk);
    return result;
}

bool
chunk_undo_turn(Chunk *chunk)
{
    if (chunk->undo_depth == 0) {
        return false;
    }

    if (chunk->cur_frame_num != 0) {
        chunk->cur_frame_num--;
    } else {
        chunk->cur_frame_num = chunk->frames_count - 1;
    }
    chunk->undo_depth--;

    chunk_update_cur_frame(chunk);
    return true;
}

void
chunk_clear(Chunk *chunk)
{
    frame_clear(chunk_switch_next_frame(chunk));
}

void
chunk_destroy(Chunk *chunk)
{
    for (unsigned i = 0; i < chunk->frames_count; i++) {
        frame_destroy(chunk->frames[i]);
    }
    free(chunk->frames);
    free(chunk);
}
