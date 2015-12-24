#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/msg.h>

#include "common.h"

int main(void)
{
    key_t key_out = ftok("life-server", 'a');
    key_t key_in = ftok("life-server", 'b');
    int msg_out_id = msgget(key_out, IPC_CREAT | 0666);
    int msg_in_id = msgget(key_in, IPC_CREAT | 0666);

    message message;
    memset(&message, 0, sizeof(message));
    do {
        message.mtype = MSG_OK;
        fgets((char *) &message.mtext, BUF_SIZE, stdin);
        msgsnd(msg_out_id, &message, MSG_SIZE, 0);

        do {
            msgrcv(msg_in_id, &message, MSG_SIZE, 0, 0);
            printf("%s", message.mtext);
        } while (message.mtype == MSG_CONTINUE);
        printf("\n");
    } while (message.mtype != MSG_EXIT);

    msgctl(key_out, IPC_RMID, NULL);
    msgctl(key_in, IPC_RMID, NULL);
}
