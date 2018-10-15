#include "npshell.h"

void sigchld_handler(int signo) {
    waitpid(-1, NULL, 0);
    return;
}

void parse_pipe(char* line, char** pipe_cmd) {
    int i;

    pipe_cmd[0] = strtok(line, "|");
    i = 1;
    while((pipe_cmd[i] = strtok(NULL, "|")) != NULL) {
        i++;
    }
}

void exec_cmd(char* line) {
    char *argv[MAX_ARG];
    int argcnt = 0;

    /* parse command */
    argv[argcnt] = strtok(line, " ");
    argcnt++;

    while((argv[argcnt] = strtok(NULL, " ")) != NULL) {
        argcnt++;
    }

    /* execute command */
    if(execvp(argv[0], argv) < 0) {
        if(errno == 2) {
            /* unrecognized command */
            char err_msg[MAX_CMD_LEN];
            sprintf(err_msg, "Unknown command: [%s].\n", argv[0]);
            write(STDERR_FILENO, err_msg, strlen(err_msg));
            exit(1);
        }
        else
            err_sys("exec error");
    }
}

void child_process(char** cmd, int pos, int infd, int outfd, int err_redir) {
    if(cmd[pos + 1] == NULL) {
        /* last command */
        if(infd > 0) {
            close(STDIN_FILENO);
            dup2(infd, STDIN_FILENO);
            close(infd);
        }
        if(outfd > 0) {
            close(STDOUT_FILENO);
            dup2(outfd, STDOUT_FILENO);
            if(err_redir > 0) {
                close(STDERR_FILENO);
                dup2(outfd, STDERR_FILENO);
            }
            close(outfd);
        }

        exec_cmd(cmd[pos]);
    }
    else {
        int pfd[2];
        pid_t pid;
        /* int status; */

        pipe(pfd);
        pid = fork();
        
        if(pid < 0)
            err_sys("fork error");
        else if(pid == 0){
            /* child process */
            if(infd > 0) {
                close(STDIN_FILENO);
                dup2(infd, STDIN_FILENO);
                close(infd);
            }

            close(STDOUT_FILENO);
            dup2(pfd[1], STDOUT_FILENO);
            close(pfd[0]);
            close(pfd[1]);

            exec_cmd(cmd[pos]);
        }
        else {
            /* parent process */
            close(STDIN_FILENO);
            dup2(pfd[0], STDIN_FILENO);
            close(pfd[0]);
            close(pfd[1]);

            signal(SIGCHLD, sigchld_handler);

            /* waitpid(pid, &status, 0); */

            /* exec error, break out pipe */
            /* then commands after pipe would not be executed */
            /* if(status == 256) { */
            /*     exit(1); */
            /* } */

            child_process(cmd, pos + 1, -1, outfd, err_redir);
        }
    }
}

int main(void) {
    char line[MAX_LINE_LEN];
    char line_cpy[MAX_LINE_LEN];
    int cnt;
    char *cmd[MAX_PIPE];
    int pipe_pos;
    pid_t pid;
    int num_pipe[MAX_NUM_PIPE][2];
    int infd, outfd, err_redir;
    int closefd;
    long N_pipe;
    char* endptr;
    int i;

    setenv("PATH", "bin:.", 1);

    for(i = 0; i < MAX_NUM_PIPE; i++) {
        num_pipe[i][0] = -1;
        num_pipe[i][1] = -1;
    }

    /* start of npshell */
    while(1) {    
        /* prompt */
        printf("%% ");
        fflush(stdout);

        memset(line, 0, MAX_LINE_LEN);

        errno = 0;
        if((cnt = read(STDIN_FILENO, line, MAX_LINE_LEN)) > 1) {
            line[--cnt] = '\0';

            strcpy(line_cpy, line);

            parse_pipe(line, &cmd[0]);

            /* check due numbered-pipes */
            infd = num_pipe[0][0];

            outfd = -1;
            err_redir = -1;

             /* shift num_pipe */
            for(i = 1; i < MAX_NUM_PIPE; i++) {
                num_pipe[i - 1][0] = num_pipe[i][0];
                num_pipe[i - 1][1] = num_pipe[i][1];
            }
            num_pipe[MAX_NUM_PIPE - 1][0] = -1;
            num_pipe[MAX_NUM_PIPE - 1][1] = -1;

            /* check numbered-pipes */
            /* only appear at the end of line */
            pipe_pos = 0;
            while(cmd[pipe_pos] != NULL)
                pipe_pos++;
            if(cmd[pipe_pos - 1][0] != ' ') {
                /* there should be no space btw pipe and numbers */
                N_pipe = strtol(cmd[pipe_pos - 1], &endptr, 0);
                if(*endptr == '\0') {
                    cmd[pipe_pos - 1] = NULL;
                    if(num_pipe[N_pipe - 1][0] == -1) {
                        int pfd[2];
                        pipe(pfd);
                        num_pipe[N_pipe - 1][0] = pfd[0];
                        num_pipe[N_pipe - 1][1] = pfd[1];
                    }
                    outfd = num_pipe[N_pipe - 1][1];
                }
            }

            if(cmd[pipe_pos - 1] != NULL) {
                for(i = 0; cmd[pipe_pos - 1][i] != '\0'; i++) {
                    if(cmd[pipe_pos - 1][i] == '!') {
                        N_pipe = strtol(cmd[pipe_pos - 1] + (i + 1), &endptr, 0);
                        if(*endptr == '\0') {
                            cmd[pipe_pos - 1][i] = '\0';
                            err_redir = 1;
                            if(num_pipe[N_pipe - 1][0] == -1) {
                                int pfd[2];
                                pipe(pfd);
                                num_pipe[N_pipe - 1][0] = pfd[0];
                                num_pipe[N_pipe - 1][1] = pfd[1];
                            }
                            outfd = num_pipe[N_pipe - 1][1];
                        }
                    }
                }
            }

            /* close written pipe */
            closefd = num_pipe[0][1];

            /* exit command */
            if(strcmp(line, "exit") == 0)
                break;

            /* built-in commands */
            char *argv = strtok(line_cpy, " ");
            if(strcmp(argv, "printenv") == 0) {
                argv = strtok(NULL, " ");
                if(argv == NULL) {
                    extern char **environ;
                    for(i = 0; environ[i] != NULL; i++)
                        printf("%s\n", environ[i]);
                }
                else {
                    char* env;
                    while(argv != NULL) {
                        env = getenv(argv);
                        if(env != NULL)
                            printf("%s\n", env);
                        argv = strtok(NULL, " ");
                    }
                }

                close(infd);
                close(closefd);

                continue;
            }
            else if(strcmp(argv, "setenv") == 0) {
                char *name = strtok(NULL, " ");
                char *value = strtok(NULL, " ");
                if(setenv(name, value, 1) < 0)
                    perror("setenv error");

                close(infd);
                close(closefd);

                continue;
            }

            /* other commands */
            pid = fork();

            if(pid < 0)
                err_sys("fork error");
            else if(pid == 0) {
                /* child process */
                char *outfile;

                /* parse_pipe(line, &cmd[0]); */

                /* check output redirection */
                pipe_pos = 0;
                while(cmd[pipe_pos] != NULL) {
                    for(i = 0; cmd[pipe_pos][i] != '\0'; i++) {
                        if(cmd[pipe_pos][i] == '>')
                            break;
                    }

                    if(cmd[pipe_pos][i] == '>') {
                        cmd[pipe_pos + 1] = NULL;

                        cmd[pipe_pos][i] = '\0';
                        outfile = strtok((char*)cmd[pipe_pos] + (i + 1), " ");

                        outfd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if(outfd < 0)
                            err_sys("open error");

                        break;
                    }

                    pipe_pos++;
                }

                child_process(cmd, 0, infd, outfd, err_redir);
            }
            else {
                /* parent process */
                waitpid(pid, NULL, 0);
                close(infd);
                close(closefd);
            }
        }
        else if(cnt == 1)
            continue;
        else{
            if(errno == 0) {
                /* detect EOF */
                printf("\n");
                break;
            }
            else
                err_sys("read error");
        }
    }

    return 0;
}
