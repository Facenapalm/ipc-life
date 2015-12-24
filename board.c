#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "core.h"
#include "board.h"

enum
{
    IPC_CREAT_RW = IPC_CREAT | 0666
};

static void board_chunks_create(Board *board);

static inline unsigned
div_round_up(unsigned dividend, unsigned divider)
{
    return (dividend + divider - 1) / divider;
}

//all of the parameters must be positive
static inline unsigned
calc_max_cell_size(
    unsigned width,
    unsigned height,
    unsigned chunks_hor,
    unsigned chunks_ver)
{
    if (chunks_hor > width || chunks_ver > height) {
        return 0;
    }

    //we can split N cells into n blocks of k cells, if:
    //N / n <= k < N / (n - 1)
    //we must find the maximal k, so, the result is (N - 1) / (n - 1)
    unsigned minx;
    unsigned miny;
    unsigned maxx;
    unsigned maxy;
    if (chunks_hor == 1) {
        minx = width;
        maxx = width > height ? width : height;
    } else {
        minx = div_round_up(width, chunks_hor);
        maxx = (width - 1) / (chunks_hor - 1);
    }
    if (chunks_ver == 1) {
        miny = height;
        maxy = width > height ? width : height;
    } else {
        miny = div_round_up(height, chunks_ver);
        maxy = (height - 1) / (chunks_ver - 1);
    }
    unsigned min = minx > miny ? minx : miny;
    unsigned max = maxx < maxy ? maxx : maxy;
    return min <= max ? max : 0;
}

static inline bool
border_ids_has_special(Border_ids *border_ids)
{
    return border_ids->top == -1 || border_ids->bottom == -1;
}

static inline Border_ids *
border_ids_create(
    unsigned chunk_size,
    unsigned last_width,
    unsigned last_height,
    bool frst_row,
    bool frst_col,
    bool last_row,
    bool last_col)
{
    Border_ids *result = calloc(1, sizeof(*result));

    unsigned cur_width = last_row ? last_width : chunk_size;
    unsigned cur_height = last_col ? last_height : chunk_size;

    if (!frst_row) {
        result->top = shmget(IPC_PRIVATE, cur_width * sizeof(Cell), IPC_CREAT_RW);
    } else {
        result->top = -1;
    }

    if (!frst_col) {
        result->left = shmget(IPC_PRIVATE, cur_height * sizeof(Cell), IPC_CREAT_RW);
    } else {
        result->left = -1;
    }

    if (!last_col) {
        result->right = shmget(IPC_PRIVATE, cur_height * sizeof(Cell), IPC_CREAT_RW);
    } else {
        result->right = -1;
    }

    if (!last_row) {
        result->bottom = shmget(IPC_PRIVATE, cur_width * sizeof(Cell), IPC_CREAT_RW);
    } else {
        result->bottom = -1;
    }

    if (border_ids_has_special(result)) {
        result->special = shmget(IPC_PRIVATE, cur_width * sizeof(Cell), IPC_CREAT_RW);
    } else {
        result->special = result->top != -1 ? result->top : result->bottom;
    }

    return result;
}

Board *
board_create(unsigned width, unsigned height, unsigned chunks_count)
{
    if (width == 0 || height == 0 || chunks_count == 0) {
        return NULL;
    }

    unsigned chunk_size = 0;
    unsigned chunks_hor;
    unsigned chunks_ver;

    unsigned cur_size;
    unsigned cur_chunks_hor;
    unsigned cur_chunks_ver;
    for (unsigned i = 1; i <= chunks_count; i++) {
        if (chunks_count % i == 0) {
            cur_chunks_hor = i;
            cur_chunks_ver = chunks_count / i;

            if (width % cur_chunks_hor == 0 && height % cur_chunks_ver == 0 &&
                (cur_size = width / cur_chunks_hor) == height / cur_chunks_ver) {
                chunk_size = cur_size;
                chunks_hor = cur_chunks_hor;
                chunks_ver = cur_chunks_ver;
                break;
            }

            cur_size = calc_max_cell_size(width, height, cur_chunks_hor, cur_chunks_ver);
            if (cur_size > chunk_size) {
                chunk_size = cur_size;
                chunks_hor = cur_chunks_hor;
                chunks_ver = cur_chunks_ver;
            }
        }
    }

    if (chunk_size == 0) {
        return NULL;
    }

    unsigned last_width = width - (chunks_hor - 1) * chunk_size;
    unsigned last_height = height - (chunks_ver - 1) * chunk_size;

    Board *result = calloc(1, sizeof(*result));

    result->width = width;
    result->height = height;

    result->chunks_count = chunks_count;
    result->chunks_hor_count = chunks_hor;
    result->chunks_ver_count = chunks_ver;

    result->chunk_size = chunk_size;

    result->last_width = last_width;
    result->last_height = last_height;

    result->generation_num = 1;

    result->chunks = calloc(chunks_ver, sizeof(*result->chunks));
    result->border_ids = calloc(chunks_ver, sizeof(*result->border_ids));
    result->special_pointers = calloc(chunks_ver, sizeof(*result->special_pointers));
    for (unsigned j = 0; j < chunks_ver; j++) {
        result->chunks[j] = calloc(chunks_hor, sizeof(**result->chunks));
        result->border_ids[j] = calloc(chunks_hor, sizeof(**result->border_ids));
        result->special_pointers[j] = calloc(chunks_hor, sizeof(**result->special_pointers));

        for (unsigned i = 0; i < chunks_hor; i++) {
            result->border_ids[j][i] = border_ids_create(
                chunk_size,
                last_width,
                last_height,
                j == 0,
                i == 0,
                j == chunks_ver - 1,
                i == chunks_hor - 1);
        }
    }

    result->sem_id = semget(IPC_PRIVATE, 2, IPC_CREAT_RW);
    result->shm_id = shmget(IPC_PRIVATE, sizeof(*result->cur_instruction), IPC_CREAT_RW);

    board_chunks_create(result);

    result->cur_instruction = shmat(result->shm_id, NULL, 0);
    for (unsigned j = 0; j < chunks_ver; j++) {
        for (unsigned i = 0; i < chunks_hor; i++) {
            result->special_pointers[j][i] = shmat(result->border_ids[j][i]->special, NULL, 0);
        }
    }

    return result;
}

static inline void *
safe_shmat(int id)
{
    if (id == -1) {
        return NULL;
    } else {
        return shmat(id, NULL, 0);
    }
}

static inline void
safe_shmdt(void *ptr)
{
    if (ptr != NULL) {
        shmdt(ptr);
    }
}

static void
free_borders(Borders *borders)
{
    safe_shmdt(borders->top_side);
    safe_shmdt(borders->left_side);
    safe_shmdt(borders->right_side);
    safe_shmdt(borders->bottom_side);

    free(borders);
}

static void
board_chunks_create(Board *board)
{
    for (unsigned j = 0; j < board->chunks_ver_count; j++) {
        for (unsigned i = 0; i < board->chunks_hor_count; i++) {
            if (!(board->chunks[j][i] = fork())) {
                unsigned chunk_num_x = i;
                unsigned chunk_num_y = j;

                bool frst_row = chunk_num_y == 0;
                bool frst_col = chunk_num_x == 0;
                bool last_col = chunk_num_x == board->chunks_hor_count - 1;
                bool last_row = chunk_num_y == board->chunks_ver_count - 1;

                unsigned width = last_col ? board->last_width : board->chunk_size;
                unsigned height = last_row ? board->last_height : board->chunk_size;

                Borders *inner = calloc(1, sizeof(*inner));
                Borders *outer = calloc(1, sizeof(*outer));

                Border_ids *cur_border_ids = board->border_ids[j][i];

                inner->top_side = safe_shmat(cur_border_ids->top);
                inner->left_side = safe_shmat(cur_border_ids->left);
                inner->right_side = safe_shmat(cur_border_ids->right);
                inner->bottom_side = safe_shmat(cur_border_ids->bottom);

                char *special_pointer;
                if (border_ids_has_special(cur_border_ids)) {
                    special_pointer = safe_shmat(cur_border_ids->special);
                } else {
                    special_pointer = (char *) (inner->top_side != NULL ? inner->top_side : inner->bottom_side);
                }

                if (!frst_row) {
                    outer->top_side = safe_shmat(board->border_ids[j - 1][i]->bottom);
                    if (!frst_col) {
                        outer->tl_angle = safe_shmat(board->border_ids[j - 1][i - 1]->bottom);
                        outer->tl_angle += board->chunk_size - 1;
                    } else {
                        outer->tl_angle = NULL;
                    }
                    if (!last_col) {
                        outer->tr_angle = safe_shmat(board->border_ids[j - 1][i + 1]->bottom);
                    } else {
                        outer->tr_angle = NULL;
                    }
                } else {
                    outer->top_side = NULL;
                    outer->tl_angle = NULL;
                    outer->tr_angle = NULL;
                }

                if (!frst_col) {
                    outer->left_side = safe_shmat(board->border_ids[j][i - 1]->right);
                } else {
                    outer->left_side = NULL;
                }

                if (!last_col) {
                    outer->right_side = safe_shmat(board->border_ids[j][i + 1]->left);
                } else {
                    outer->right_side = NULL;
                }

                if (!last_row) {
                    outer->bottom_side = safe_shmat(board->border_ids[j + 1][i]->top);
                    if (!frst_col) {
                        outer->bl_angle = safe_shmat(board->border_ids[j + 1][i - 1]->top);
                        outer->bl_angle += board->chunk_size - 1;
                    } else {
                        outer->bl_angle = NULL;
                    }
                    if (!last_col) {
                        outer->br_angle = safe_shmat(board->border_ids[j + 1][i + 1]->top);
                    } else {
                        outer->br_angle = NULL;
                    }
                } else {
                    outer->bottom_side = NULL;
                    outer->bl_angle = NULL;
                    outer->br_angle = NULL;
                }

                Frame *frame_one = frame_create(width, height, inner, outer);
                Frame *frame_two = frame_create(width, height, inner, outer);
                Frame *cur_frame;
                Chunk *chunk = chunk_create(2, frame_one, frame_two);

                Instruction *instruction = safe_shmat(board->shm_id);
                struct sembuf *operation = calloc(1, sizeof(*operation));
                char *scanline;
                bool terminate = false;
                do {
                    operation->sem_num = 0;
                    operation->sem_op = -1;
                    semop(board->sem_id, operation, 1);

                    if ((instruction->chunk_num_x == chunk_num_x ||
                        instruction->chunk_num_x == CHUNK_NUM_ANY) &&
                        (instruction->chunk_num_y == chunk_num_y ||
                        instruction->chunk_num_y == CHUNK_NUM_ANY)) {
                        cur_frame = chunk->cur_frame;
                        switch (instruction->id) {
                            case INSTRUCTION_DESTROY:
                                terminate = true;
                                break;
                            case INSTRUCTION_ADD_CELL:
                                frame_set_cell(cur_frame, instruction->param1, instruction->param2, CELL_ALIVE);
                                break;
                            case INSTRUCTION_WRITE_SCANLINE:
                                scanline = frame_render_line(cur_frame, instruction->param1);
                                memcpy(special_pointer, scanline, width);
                                free(scanline);
                                break;
                            case INSTRUCTION_READ_SCANLINE:
                                scanline = calloc(width + 1, sizeof(*scanline));
                                memcpy(scanline, special_pointer, width);
                                frame_load_line(cur_frame, scanline, instruction->param1);
                                free(scanline);
                                break;
                            case INSTRUCTION_UPDATE_INNER_BORDERS:
                                frame_update_inner_borders(cur_frame);
                                break;
                            case INSTRUCTION_UPDATE_OUTER_BORDERS:
                                frame_update_outer_borders(cur_frame);
                                break;
                            case INSTRUCTION_CALCULATE:
                                frame_calc(chunk_switch_next_frame(chunk), cur_frame);
                                break;
                            case INSTRUCTION_CLEAR:
                                chunk_clear(chunk);
                                break;
                            case INSTRUCTION_NOP:
                            default:
                                break;
                        }
                    }

                    operation->sem_num = 0;
                    operation->sem_op = 0;
                    semop(board->sem_id, operation, 1);

                    operation->sem_num = 1;
                    operation->sem_op = +1;
                    semop(board->sem_id, operation, 1);
                } while (!terminate);

                free(operation);
                shmdt(instruction);

                chunk_destroy(chunk);

                if (border_ids_has_special(cur_border_ids)) {
                    shmdt(special_pointer);
                }

                free_borders(outer);
                free_borders(inner);

                exit(0);
            }
        }
    }
}

static void
board_send_instruction(Board *board)
{
    struct sembuf *operation = calloc(1, sizeof(*operation));
    operation->sem_num = 0;
    operation->sem_op = board->chunks_count;
    semop(board->sem_id, operation, 1);
    operation->sem_num = 1;
    operation->sem_op = -board->chunks_count;
    semop(board->sem_id, operation, 1);
    free(operation);
}

static inline unsigned
get_chunk_num(unsigned coord, unsigned chunk_size, unsigned chunk_count)
{
    unsigned result = (coord - 1) / chunk_size;
    if (result >= chunk_count) {
        result = chunk_count - 1;
    }
    return result;
}

static inline unsigned
get_chunk_coord(unsigned coord, unsigned chunk_size, unsigned chunk_count)
{
    return coord - chunk_size * get_chunk_num(coord, chunk_size, chunk_count);
}

bool
board_add_cell(Board *board, unsigned x, unsigned y)
{
    if (x < 1 || x > board->width || y < 1|| y > board->height) {
        return false;
    }

    Instruction *instruction = board->cur_instruction;
    instruction->id = INSTRUCTION_ADD_CELL;
    instruction->chunk_num_x = get_chunk_num(x, board->chunk_size, board->chunks_hor_count);
    instruction->chunk_num_y = get_chunk_num(y, board->chunk_size, board->chunks_ver_count);
    instruction->param1 = get_chunk_coord(x, board->chunk_size, board->chunks_hor_count);
    instruction->param2 = get_chunk_coord(y, board->chunk_size, board->chunks_ver_count);
    board_send_instruction(board);

    return true;
}

void
board_next_turn(Board *board)
{
    Instruction *instruction = board->cur_instruction;
    instruction->chunk_num_x = CHUNK_NUM_ANY;
    instruction->chunk_num_y = CHUNK_NUM_ANY;
    instruction->id = INSTRUCTION_UPDATE_INNER_BORDERS;
    board_send_instruction(board);
    instruction->id = INSTRUCTION_UPDATE_OUTER_BORDERS;
    board_send_instruction(board);
    instruction->id = INSTRUCTION_CALCULATE;
    board_send_instruction(board);

    board->generation_num += 1;
}

void
board_clear(Board *board)
{
    Instruction *instruction = board->cur_instruction;
    instruction->id = INSTRUCTION_CLEAR;
    instruction->chunk_num_x = CHUNK_NUM_ANY;
    instruction->chunk_num_y = CHUNK_NUM_ANY;
    board_send_instruction(board);

    board->generation_num = 1;
}

char *
board_get_scanline(Board *board, unsigned y)
{
    if (y < 1 || y > board->height) {
        return NULL;
    }
    Instruction *instruction = board->cur_instruction;
    instruction->id = INSTRUCTION_WRITE_SCANLINE;
    instruction->chunk_num_x = CHUNK_NUM_ANY;
    instruction->chunk_num_y = get_chunk_num(y, board->chunk_size, board->chunks_ver_count);
    instruction->param1 = get_chunk_coord(y, board->chunk_size, board->chunks_ver_count);
    board_send_instruction(board);

    char *result = calloc(board->width + 1, sizeof(*result));
    unsigned last_chunk_num = board->chunks_hor_count - 1;
    for (unsigned i = 0; i < board->chunks_hor_count; i++) {
        memcpy(
            result + i * board->chunk_size,
            board->special_pointers[instruction->chunk_num_y][i],
            (i == last_chunk_num ? board->last_width : board->chunk_size) * sizeof(char));
    }

    return result;
}

bool
board_set_scanline(Board *board, unsigned y, char *scanline)
{
    if (y < 1 || y > board->height) {
        return false;
    }
    if (strlen(scanline) != board->width) {
        return false;
    }

    unsigned chunk_num = get_chunk_num(y, board->chunk_size, board->chunks_ver_count);
    unsigned last_chunk_num = board->chunks_hor_count - 1;
    for (unsigned i = 0; i < board->chunks_hor_count; i++) {
        memcpy(
            board->special_pointers[chunk_num][i],
            scanline + i * board->chunk_size,
            (i == last_chunk_num ? board->last_width : board->chunk_size) * sizeof(char));
    }

    Instruction *instruction = board->cur_instruction;
    instruction->id = INSTRUCTION_READ_SCANLINE;
    instruction->chunk_num_x = CHUNK_NUM_ANY;
    instruction->chunk_num_y = chunk_num;
    instruction->param1 = get_chunk_coord(y, board->chunk_size, board->chunks_ver_count);
    board_send_instruction(board);
    return true;
}

bool
board_load_from_file(Board *board, FILE *input)
{
    if (input == NULL) {
        return false;
    }
    unsigned str_len = board->width + 2;
    char *cur_scanline = calloc(str_len, sizeof(char));
    for (unsigned i = 1; i <= board->height; i++) {
        if (fgets(cur_scanline, str_len, input) == NULL ||
            (cur_scanline[board->width] = '\0',
            !board_set_scanline(board, i, cur_scanline))) {
            board_clear(board);
            free(cur_scanline);
            return false;
        }
    }

    free(cur_scanline);

    board->generation_num = 1;
    return true;
}

bool
board_save_to_file(Board *board, FILE *output)
{
    if (output == NULL) {
        return false;
    }

    char *cur_scanline;
    for (unsigned i = 1; i <= board->height; i++) {
        cur_scanline = board_get_scanline(board, i);
        fprintf(output, "%s\n", cur_scanline);
        free(cur_scanline);
    }

    fflush(output);
    return true;
}

static void
board_chunks_destroy(Board *board)
{
    Instruction *instruction = board->cur_instruction;
    instruction->id = INSTRUCTION_DESTROY;
    instruction->chunk_num_x = CHUNK_NUM_ANY;
    instruction->chunk_num_y = CHUNK_NUM_ANY;
    board_send_instruction(board);

    for (unsigned j = 0; j < board->chunks_ver_count; j++) {
        for (unsigned i = 0; i < board->chunks_hor_count; i++) {
            waitpid(board->chunks[j][i], NULL, 0);
        }
    }
}

static inline void
safe_shm_free(int shm_id)
{
    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
    }
}

void
board_destroy(Board *board)
{
    board_chunks_destroy(board);

    shmdt(board->cur_instruction);

    shmctl(board->shm_id, IPC_RMID, NULL);
    semctl(board->sem_id, 0, 0, NULL);

    Border_ids *cur_ids;
    for (unsigned j = 0; j < board->chunks_ver_count; j++) {
        for (unsigned i = 0; i < board->chunks_hor_count; i++) {
            shmdt(board->special_pointers[j][i]);

            cur_ids = board->border_ids[j][i];
            safe_shm_free(cur_ids->top);
            safe_shm_free(cur_ids->left);
            safe_shm_free(cur_ids->right);
            safe_shm_free(cur_ids->bottom);

            if (border_ids_has_special(cur_ids)) {
                shmctl(cur_ids->special, IPC_RMID, NULL);
            }
            free(cur_ids);
        }
        free(board->special_pointers[j]);
        free(board->border_ids[j]);
        free(board->chunks[j]);
    }
    free(board->special_pointers);
    free(board->border_ids);
    free(board->chunks);

    free(board);
}
