#include "np_single_proc.h"

int maxi;
int clientfd[MAX_CLI_NUM];
int cliport[MAX_CLI_NUM];
char *cliip[MAX_CLI_NUM], *username[MAX_CLI_NUM], *path[MAX_CLI_NUM];

static int stdin_copy;
static int stdout_copy;
static int stderr_copy;

void replace_std_fds(int fd) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
}

void reopen_std_fds() {
    dup2(stdin_copy, STDIN_FILENO);
    dup2(stdout_copy, STDOUT_FILENO);
    dup2(stderr_copy, STDERR_FILENO);
}

int main(int argc, char **argv) {
    int serv_port;
    int listenfd, connfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    const int optval = 1;

    int maxfd;
    int nready;
    fd_set rset, allset;

    int i, j, n;
    char msg[MAX_MSG_LEN];

    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    stdin_copy = dup(STDIN_FILENO);
    stdout_copy = dup(STDOUT_FILENO);
    stderr_copy = dup(STDERR_FILENO);

    serv_port = atoi(argv[1]);

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 0.0.0.0
    serv_addr.sin_port = htons(serv_port);

    Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    Bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    Listen(listenfd, LISTENQ);

    maxfd = listenfd;
    maxi = -1;
    
    for(i = 0; i < MAX_CLI_NUM; i++)
        clientfd[i] = -1;

    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    for(;;) {
        rset = allset;
        while((nready = Select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0);

        if(FD_ISSET(listenfd, &rset)) {
            clilen = sizeof(cli_addr);
            connfd = Accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

            for(i = 0; i < MAX_CLI_NUM; i++){
                if(clientfd[i] == -1) {
                    clientfd[i] = connfd;
                    break;
                }
            }
            if(i == MAX_CLI_NUM)
                err_quit("Excessive connection from clients");

            if(clientfd[i] > maxfd)
                maxfd = clientfd[i];
            if(i > maxi)
                maxi = i;
            FD_SET(clientfd[i], &allset);

            write(clientfd[i], WELCOME_MSG, strlen(WELCOME_MSG) * sizeof(char));

            cliip[i] = malloc(INET_ADDRSTRLEN * sizeof(char));
            inet_ntop(AF_INET, &cli_addr.sin_addr, cliip[i], INET_ADDRSTRLEN);
            /* strcpy(cliip[i], "CGILAB"); */
            cliport[i] = ntohs(cli_addr.sin_port);
            /* cliport[i] = 511; */
            path[i] = malloc(MAX_MSG_LEN * sizeof(char));
            strcpy(path[i], "bin:.");

            for(j = 0; j <= maxi; j++){
                if(clientfd[j] != -1){
                    sprintf(msg, "*** User '(no name)' entered from %s/%d. ***\n", 
                            cliip[i], cliport[i]);
                    write(clientfd[j], msg, strlen(msg) * sizeof(char));
                }
            }

            /* initial prompt */
            write(clientfd[i], "% ", 2 * sizeof(char));

            if(--nready == 0)
                continue;
        }

        for(i = 0; i <= maxi; i++) {
            if(clientfd[i] < 0)
                continue;

            if(FD_ISSET(clientfd[i], &rset)) {
                replace_std_fds(clientfd[i]);
                n = npshell(i);
                reopen_std_fds();

                if(n == 1) {
                    /* connection closed by client */
                    for(j = 0; j <= maxi; j++) {
                        if(clientfd[j] != -1) {
                            if(username[i] != NULL)
                                sprintf(msg, "*** User '%s' left. ***\n", username[i]);
                            else
                                strcpy(msg, "*** User '(no name)' left. ***\n");
                            write(clientfd[j], msg, strlen(msg) * sizeof(char));
                        }
                    }

                    if(clientfd[i] == maxfd) {
                        int submaxfd = -1;
                        for(j = 0; j <= maxi; j++) {
                            if(clientfd[j] > submaxfd && i != j)
                                submaxfd = clientfd[j];
                        }
                        if(submaxfd != -1)
                            maxfd = submaxfd;
                    }

                    close(clientfd[i]);
                    FD_CLR(clientfd[i], &allset);
                    clientfd[i] = -1;

                    if(i == maxi) {
                        for(j = maxi - 1; j >= 0; j--){
                            if(clientfd[j] != -1) {
                                break;
                            }
                        }
                        maxi = j;
                    }
                    
                    free(cliip[i]);
                    free(path[i]);
                    if(username[i] != NULL) {
                        free(username[i]);
                        username[i] = NULL;
                    }
               }

                if(--nready == 0)
                    break;
            }
        }
    }

    return 0;
}
