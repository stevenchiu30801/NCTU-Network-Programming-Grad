#include "header.h"

#define MAX_LINE_LEN 15000
#define MAX_MSG_LEN 1024
#define MAX_CMD_LEN 256
#define MAX_ARG 20
#define MAX_CMD_PIPE 2500
#define MAX_NUM_PIPE 1000
#define MAX_FORK_NUM 200
#define MAX_CLI_NUM 30
// #define MAX_CLI_NUM FD_SETSIZE

extern void err_sys(const char* x);

extern int maxi;
extern int clientfd[FD_SETSIZE];
extern int cliport[FD_SETSIZE];
extern char *cliip[FD_SETSIZE];
extern char *path[FD_SETSIZE];
extern char *username[FD_SETSIZE];
