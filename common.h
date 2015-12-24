#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

enum
{
	BUF_SIZE = 255,

	MSG_OK = 1,
	MSG_CONTINUE = 2,
	MSG_EXIT = 3
};

typedef struct message
{
	long mtype;
	char mtext[BUF_SIZE + 1]; //for '\0'
} message;

enum
{
	MSG_SIZE = sizeof(message) - sizeof(long)
};

#endif //COMMON_H_INCLUDED
