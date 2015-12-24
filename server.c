#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/msg.h>

#include "board.h"
#include "text.h"
#include "common.h"

enum
{
    MAX_ARGUMENTS = 4
};

bool
is_space(char x)
{
    return
        x == ' ' ||
        x == '\0' ||
        x == '\r' ||
        x == '\n' ||
        x == '\t' ||
        x == '\v';
}

int
split_by_spaces(char *string, int max_subs, char **substrs)
{
    int len = strlen(string);
    char *cur_pos = string;
    char *cur_word = string;
    char *last_pos = string + len;
    int cur_sub = 0;
    while (cur_pos <= last_pos && cur_sub < max_subs) {
        if (is_space(*cur_pos)) {
            if (cur_pos != cur_word) {
                substrs[cur_sub++] = cur_word;
            }
            *cur_pos = '\0';
            cur_word = ++cur_pos;
        } else {
            cur_pos++;
        }
    }
    return cur_sub;
}

bool
is_number(char *string)
{
    int len = strlen(string);
    bool result = len > 0;
    for (int i = 0; i < len; i++) {
        result = result && string[i] >= '0' && string[i] <= '9';
    }
    return result;
}

int
main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "%s\n", argc < 4 ? ERROR_TOO_FEW_ARGS : ERROR_TOO_MUCH_ARGS);
        fprintf(stderr, "%s\n", CORRECT_USE_INFO);
        return 1;
    }

    long width = atol(argv[1]);
    long height = atol(argv[2]);
    long chunks_count = atol(argv[3]);

    if (width < 1 || height < 1) {
        fprintf(stderr, "%s\n", ERROR_DIMENSIONS);
        return 2;
    }

    Board *board = board_create(width, height, chunks_count);
    if (board == NULL) {
        fprintf(stderr, "%s\n", ERROR_WORKERS_COUNT);
        return 3;
    }

    bool terminate = false;
    char *args[MAX_ARGUMENTS];
    int args_count;
    char *answer;
    char *cur_scanline;
    unsigned block_size;
    FILE *file;

    unsigned long long end_generation = 0;

    message message;
    memset(&message, 0, sizeof(message));

    key_t key_in = ftok("life-server", 'a');
    key_t key_out = ftok("life-server", 'b');
    int msg_in_id = msgget(key_in, IPC_CREAT | 0666);
    int msg_out_id = msgget(key_out, IPC_CREAT | 0666);
    do {
        answer = (char *) ERROR_NO;

        if (end_generation == 0) {
            msgrcv(msg_in_id, &message, MSG_SIZE, 0, 0);
        } else {
            //we're calculationg now
            message.mtype = -1;
            msgrcv(msg_in_id, &message, MSG_SIZE, 0, IPC_NOWAIT);
            if (message.mtype == -1) {
                //no messages recieved
                board_next_turn(board);
                if (board->generation_num == end_generation) {
                    end_generation = 0;
                }
                continue;
            }
        }
        printf("%s\n%s\n", LOG_COMMAND_RECIEVED, message.mtext);

        args_count = split_by_spaces(message.mtext, MAX_ARGUMENTS, args);

        if (strcmp(args[0], "add") == 0) {
            if (args_count < 3) {
                answer = (char *) ERROR_TOO_FEW_ARGS;
            } else if (args_count > 3) {
                answer = (char *) ERROR_TOO_MUCH_ARGS;
            } else if (!is_number(args[1]) || !is_number(args[2])) {
                answer = (char *) ERROR_NUMERIC_ARG;
            } else {
                if(!board_add_cell(board, atoi(args[1]), atoi(args[2]))) {
                    answer = (char *) ERROR_COORDINATES;
                }
            }
        } else if (strcmp(args[0], "clear") == 0) {
            if (args_count > 1) {
                answer = (char *) ERROR_TOO_MUCH_ARGS;
            } else {
                board_clear(board);
            }
        } else if (strcmp(args[0], "start") == 0) {
            if (args_count > 2) {
                answer = (char *) ERROR_TOO_MUCH_ARGS;
            } else {
                if (args_count == 2) {
                    if (!is_number(args[1])) {
                        answer = (char *) ERROR_NUMERIC_ARG;
                    } else {
                        end_generation = atoi(args[1]);
                        if (end_generation <= board->generation_num) {
                            end_generation = 0;
                            answer = (char *) ERROR_WRONG_GEN;
                        }
                    }
                } else {
                    end_generation = -1;
                }
            }
        } else if (strcmp(args[0], "stop") == 0) {
            if (args_count > 1) {
                answer = (char *) ERROR_TOO_MUCH_ARGS;
            } else if (end_generation == 0) {
                answer = (char *) ERROR_NOT_STARTED;
            } else {
                end_generation = 0;
            }
        } else if (strcmp(args[0], "snapshot") == 0) {
            if (args_count > 1) {
                answer = (char *) ERROR_TOO_MUCH_ARGS;
            } else {
                message.mtype = MSG_CONTINUE;
                for (int j = 1; j <= board->height; j++) {
                    cur_scanline = board_get_scanline(board, j);
                    for (int i = 0; i < board->width; i += BUF_SIZE) {
                        block_size = board->width - i;
                        if (block_size > BUF_SIZE) {
                            block_size = BUF_SIZE;
                        }
                        memcpy(
                            message.mtext,
                            cur_scanline + i,
                            block_size);
                        message.mtext[block_size] = '\0';
                        msgsnd(msg_out_id, &message, MSG_SIZE, 0);
                    }
                    free(cur_scanline);

                    strcpy(message.mtext, "\n");
                    msgsnd(msg_out_id, &message, MSG_SIZE, 0);
                }
                fflush(stdout);
            }
        } else if (strcmp(args[0], "load") == 0) {
            if (args_count < 2) {
                answer = (char *) ERROR_TOO_FEW_ARGS;
            } else if (args_count > 2) {
                answer = (char *) ERROR_TOO_MUCH_ARGS;
            } else {
                file = fopen(args[1], "r");
                if (file == NULL) {
                    answer = (char *) ERROR_FILE_OPEN;
                } else {
                    if (!board_load_from_file(board, file)) {
                        answer = (char *) ERROR_FILE_FORMAT;
                    }
                    fclose(file);
                }
            }
        } else if (strcmp(args[0], "save") == 0) {
            if (args_count < 2) {
                answer = (char *) ERROR_TOO_FEW_ARGS;
            } else if (args_count > 2) {
                answer = (char *) ERROR_TOO_MUCH_ARGS;
            } else {
                file = fopen(args[1], "w");
                if (file == NULL) {
                    answer = (char *) ERROR_FILE_CREATE;
                } else {
                    board_save_to_file(board, file);
                    fclose(file);
                }
            }
        } else if (strcmp(args[0], "quit") == 0) {
            terminate = true;
        } else {
            answer = (char *) ERROR_UNKNOWN;
        }

        message.mtype = terminate ? MSG_EXIT : MSG_OK;
        strcpy(message.mtext, answer);
        msgsnd(msg_out_id, &message, MSG_SIZE, 0);
    } while (!terminate);

    msgctl(key_out, IPC_RMID, NULL);
    msgctl(key_in, IPC_RMID, NULL);

    board_destroy(board);
    return 0;
}
