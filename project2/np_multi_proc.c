#include "np_multi_proc.h"

sem_t *clientfdsem, *usernamesem;
int clientfdkey, cliportkey;
int cliipkey[MAX_CLI_NUM], usernamekey[MAX_CLI_NUM];
int bcstfd, rcvfd, tellfd;

static int exit_prog = 0;
static int stdin_copy;
static int stdout_copy;
static int stderr_copy;
static int exitclientkey;

void sigint_handler(int signo) {
    exit_prog = 1;
    return;
}

void sigchld_handler(int signo) {
    waitpid(-1, NULL, 0);
    int *exitclient = (int*)shmat(exitclientkey, NULL, 0);
    close(*exitclient);
    shmdt(exitclient);
    return;
}

void replace_std_fds(int fd) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
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

    int maxi;
    int *clientfd, *cliport;
    char *cliip[MAX_CLI_NUM], *username[MAX_CLI_NUM];
    int *exitclient;

    pid_t clipid;
    int pfd[2], broadcastpipe[2], tellpipe;

    int i, j, n;
    char line[MAX_MSG_LEN];
    char msg[MAX_MSG_LEN];

    struct sigaction action;

    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    action.sa_handler = sigint_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);
    signal(SIGCHLD, sigchld_handler);

    stdin_copy = dup(STDIN_FILENO);
    stdout_copy = dup(STDOUT_FILENO);
    stderr_copy = dup(STDERR_FILENO);

    /* initialize semaphores */
    clientfdsem = sem_open(CLI_FD_SEM, O_CREAT, 0600, 1);
    usernamesem = sem_open(USRNM_SEM, O_CREAT, 0600, 1);

    /* initialize shared memory */
    clientfdkey = shmget(IPC_PRIVATE, MAX_CLI_NUM * sizeof(int), IPC_CREAT | 0600);
    cliportkey = shmget(IPC_PRIVATE, MAX_CLI_NUM * sizeof(int), IPC_CREAT | 0600);
    for(i = 0; i < MAX_CLI_NUM; i++) {
        cliipkey[i] = shmget(IPC_PRIVATE, MAX_MSG_LEN * sizeof(char), IPC_CREAT | 0600);
        usernamekey[i] = shmget(IPC_PRIVATE, MAX_MSG_LEN * sizeof(char), IPC_CREAT | 0600);
    }
    exitclientkey = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);

    clientfd = (int*)shmat(clientfdkey, NULL, 0);
    cliport = (int*)shmat(cliportkey, NULL, 0);
    for(i = 0; i < MAX_CLI_NUM; i++) {
        cliip[i] = (char*)shmat(cliipkey[i], NULL, 0);
        username[i] = (char*)shmat(usernamekey[i], NULL, 0);
        memset(username[i], 0, MAX_MSG_LEN);
    }
    exitclient = (int*)shmat(exitclientkey, NULL, 0);

    for(i = 0; i < MAX_CLI_NUM; i++)
        clientfd[i] = -1;

    pipe(pfd);
    broadcastpipe[0] = pfd[0];
    bcstfd = pfd[1];
    pipe(pfd);
    broadcastpipe[1] = pfd[1];
    rcvfd = pfd[0];
    pipe(pfd);
    tellpipe = pfd[0];
    tellfd = pfd[1];

    /* check if user pipe directory exists */
    struct stat sb;
    if(stat(USER_PIPE_DIR, &sb) || !S_ISDIR(sb.st_mode)) {
        mkdir(USER_PIPE_DIR, 0700);
    }

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

    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    FD_SET(broadcastpipe[0], &allset);
    FD_SET(tellpipe, &allset);

    for(;;) {
        rset = allset;
        while((nready = Select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0) {
            if(exit_prog)
                break;
        }
        if(exit_prog)
            break;

        if(FD_ISSET(listenfd, &rset)) {
            clilen = sizeof(cli_addr);
            connfd = Accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

            sem_wait(clientfdsem);
            for(i = 0; i < MAX_CLI_NUM; i++){
                if(clientfd[i] == -1) {
                    clientfd[i] = connfd;
                    break;
                }
            }
            if(i == MAX_CLI_NUM) {
                write(STDOUT_FILENO, "Excessiclientfdve connection from clients\n", 34);
                continue;
            }

            if(clientfd[i] > maxfd)
                maxfd = clientfd[i];
            if(i > maxi)
                maxi = i;
            write(clientfd[i], WELCOME_MSG, strlen(WELCOME_MSG) * sizeof(char));
            sem_post(clientfdsem);

            inet_ntop(AF_INET, &cli_addr.sin_addr, cliip[i], INET_ADDRSTRLEN);
            /* strcpy(cliip[i], "CGILAB"); */
            cliport[i] = ntohs(cli_addr.sin_port);
            /* cliport[i] = 511; */

            sem_wait(clientfdsem);
            for(j = 0; j <= maxi; j++){
                if(clientfd[j] != -1){
                    sprintf(msg, "*** User '(no name)' entered from %s/%d. ***\n", 
                            cliip[i], cliport[i]);
                    write(clientfd[j], msg, strlen(msg) * sizeof(char));
                }
            }
            sem_post(clientfdsem);

            clipid = fork();

            if(clipid < 0)
                err_sys("fork error");
            else if(clipid == 0) {
                /* child process */
                replace_std_fds(clientfd[i]);

                /* close other clients' fds */
                sem_wait(clientfdsem);
                for(j = 0; j <= maxi; j++) {
                    if(i != j)
                        close(clientfd[j]);
                }
                sem_post(clientfdsem);

                close(broadcastpipe[0]);
                close(broadcastpipe[1]);
                close(tellpipe);

                npshell(i);

                /* connection closed by client */
                sem_wait(usernamesem);
                if(strlen(username[i]) != 0)
                    sprintf(msg, "*** User '%s' left. ***\n", username[i]);
                else
                    strcpy(msg, "*** User '(no name)' left. ***\n");
                memset(username[i], 0, MAX_MSG_LEN);
                sem_post(usernamesem);
                write(clientfd[i], msg, strlen(msg) * sizeof(char));

                char bcstmsg[MAX_MSG_LEN];
                sprintf(bcstmsg, "%d %s", i, msg);
                write(bcstfd, bcstmsg, strlen(bcstmsg) * sizeof(char));

                sem_wait(clientfdsem);
                *exitclient = clientfd[i];
                close(clientfd[i]);
                clientfd[i] = -1;
                sem_post(clientfdsem);

                shmdt(clientfd);
                shmdt(cliport);
                for(i = 0; i < MAX_CLI_NUM; i++) {
                    shmdt(cliip[i]);
                    shmdt(username[i]);
                }
                shmdt(exitclient);

                exit(0);
            }
            else {
                /* parent process */
                if(--nready == 0)
                    continue;
            }
        }
        if(FD_ISSET(broadcastpipe[0], &rset)) {
            /* broadcast message */
            if((n = read(broadcastpipe[0], line, MAX_MSG_LEN)) != 0) {
                char *userid_str = strtok(line, " ");
                sem_wait(clientfdsem);
                for(i = 0; i <= maxi; i++) {
                    if(clientfd[i] != -1 && i != atoi(userid_str))
                        write(clientfd[i], 
                              line + strlen(userid_str) + 1, 
                              n - strlen(userid_str) - 1);
                }
                sem_post(clientfdsem);
            }

            if(--nready == 0)
                continue;
        }
        if(FD_ISSET(tellpipe, &rset)) {
            /* tell message */
            if((n = read(tellpipe, line, MAX_MSG_LEN)) != 0) {
                char *userid_str = strtok(line, " ");
                write(clientfd[atoi(userid_str)], 
                      line + strlen(userid_str) + 1, 
                      n - strlen(userid_str) - 1);
            }
        }
    }

    /* clear shared memory and close semaphore */
    shmdt(clientfd);
    shmdt(cliport);
    for(i = 0; i < MAX_CLI_NUM; i++) {
        shmdt(cliip[i]);
        shmdt(username[i]);
    }
    shmdt(exitclient);
    shmctl(clientfdkey, IPC_RMID, 0);
    shmctl(cliportkey, IPC_RMID, 0);
    for(i = 0; i < MAX_CLI_NUM; i++) {
        shmctl(cliipkey[i], IPC_RMID, 0);
        shmctl(usernamekey[i], IPC_RMID, 0);
    }
    shmctl(exitclientkey, IPC_RMID, 0);

    sem_unlink(CLI_FD_SEM);
    sem_unlink(USRNM_SEM);
    sem_close(clientfdsem);
    sem_close(usernamesem);

    write(STDOUT_FILENO, "\n", 1);

    return 0;
}
