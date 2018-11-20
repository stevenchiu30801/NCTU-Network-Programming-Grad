#include "np_simple.h"

int main(int argc, char **argv) {
    int serv_port;
    int listenfd, connfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    const int optval = 1;
    int stdin_copy = dup(0);
    int stdout_copy = dup(1);
    int stderr_copy = dup(2);

    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_port = atoi(argv[1]);

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 0.0.0.0
    serv_addr.sin_port = htons(serv_port);

    Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    Bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    Listen(listenfd, LISTENQ);

    for(;;) {
        clilen = sizeof(cli_addr);
        connfd = Accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        dup2(connfd, STDIN_FILENO);
        dup2(connfd, STDOUT_FILENO);
        dup2(connfd, STDERR_FILENO);
        close(connfd);

        npshell();

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        dup2(stdin_copy, STDIN_FILENO);
        dup2(stdout_copy, STDOUT_FILENO);
        dup2(stderr_copy, STDERR_FILENO);
    }

    return 0;
}
