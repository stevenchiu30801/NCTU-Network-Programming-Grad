#include "header.h"

#define WELCOME_MSG "****************************************\n\
** Welcome to the information server. **\n\
****************************************\n"
#define LISTENQ 1024
#define MAX_MSG_LEN 1024
#define MAX_CLI_NUM 30
// #define MAX_CLI_NUM FD_SETSIZE
#define USER_PIPE_DIR "./user_pipe"
#define CLI_FD_SEM "clientfdsem"
#define USRNM_SEM "usernamesem"

extern void err_sys(const char *x);
extern void err_quit(const char *x);
extern int Setsockopt(int sockfd, int level, int optname,
                      const void *optval, socklen_t optlen);
extern int Socket(int domain, int type, int protocol);
extern int Bind(int socket, const struct sockaddr *addr, socklen_t addrlen);
extern int Listen(int sockfd, int backlog);
extern int Select(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout);
extern int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int npshell();
