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
#define USER_PIPE_DIR "./user_pipe"
#define CLI_FD_SEM "clientfdsem"
#define USRNM_SEM "usernamesem"

extern void err_sys(const char* x);
extern int Select(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout);

extern sem_t *clientfdsem, *usernamesem;
extern int clientfdkey, cliportkey;
extern int cliipkey[MAX_CLI_NUM], usernamekey[MAX_CLI_NUM];
extern int bcstfd, rcvfd, tellfd;
