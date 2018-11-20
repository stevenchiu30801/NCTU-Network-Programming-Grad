#include "header.h"

#define LISTENQ 1024

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
extern void npshell();
