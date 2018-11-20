#include "header.h"

void err_sys(const char *x) {
    perror(x);
    exit(1);
}

void err_quit(const char *x) {
    perror(x);
    exit(1);
}

int Setsockopt(int sockfd, int level, int optname, 
               const void *optval, socklen_t optlen) {
    int n;

    if((n = setsockopt(sockfd, level, optname, optval, optlen)) < 0)
        err_sys("Setsockopt Error");

    return n;
}

int Socket(int domain, int type, int protocol) {
    int sockfd;

    if((sockfd = socket(domain, type, protocol)) < 0)
            err_sys("Socket Error");

    return sockfd;
}

int Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int n;

    if((n = bind(sockfd, addr, addrlen)) < 0)
        err_sys("Bind Error");

    return n;
}

int Listen(int sockfd, int backlog) {
    int n;

    if((n = listen(sockfd, backlog)) < 0)
        err_sys("Listen Error");

    return n;
}

int Select(int nfds, fd_set *readfds, fd_set *writefds, 
           fd_set *exceptfds, struct timeval *timeout) {
    int fdcnt;

    if((fdcnt = select(nfds, readfds, writefds, exceptfds, timeout)) < 0) {
        if(errno != EINTR)
            err_sys("Select Error");
    }

    return fdcnt;
}

int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int connfd;

    if((connfd = accept(sockfd, addr, addrlen)) < 0) {
        err_sys("Accept Error");
    }

    return connfd;
}
