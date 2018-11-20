#include "npshell_single_proc.h"

int shell_fork_num = 0;
int process_fork_num = 0;

void shell_sigchld_handler(int signo) {
    waitpid(-1, NULL, 0);
    shell_fork_num--;
    return;
}

void process_sigchld_handler(int signo) {
    waitpid(-1, NULL, 0);
    process_fork_num--;
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
            char msg[MAX_MSG_LEN];
            sprintf(msg, "Unknown command: [%s].\n", argv[0]);
            write(STDERR_FILENO, msg, strlen(msg) * sizeof(char));
            /* char err_msg[MAX_CMD_LEN]; */
            /* sprintf(err_msg, "Unknown command: [%s].\n", argv[0]); */
            /* write(STDERR_FILENO, err_msg, strlen(err_msg)); */
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

            /* waitpid(pid, &status, 0); */

            /* exec error, break out pipe */
            /* then commands after pipe would not be executed */
            /* if(status == 256) { */
            /*     exit(1); */
            /* } */

            while(process_fork_num > MAX_FORK_NUM);

            process_fork_num++;
            child_process(cmd, pos + 1, -1, outfd, err_redir);
        }
    }
}

void close_fds(int idx, int num_pipe[MAX_CLI_NUM][MAX_NUM_PIPE][2], 
               int user_pipe[MAX_CLI_NUM][MAX_CLI_NUM][2]) {
    int i;
    /* close pipe fd */
    for(i = 0; i < MAX_NUM_PIPE; i++) {
        if(num_pipe[idx][i][0] != -1) {
            close(num_pipe[idx][i][0]);
            num_pipe[idx][i][0] = -1;
        }
        if(num_pipe[idx][i][1] != -1) {
            close(num_pipe[idx][i][1]);
            num_pipe[idx][i][1] = -1;
        }
    }
    for(i = 0; i < MAX_CLI_NUM; i++) {
        if(user_pipe[i][idx][0] != -1) {
            close(user_pipe[i][idx][0]);
            close(user_pipe[i][idx][1]);
            user_pipe[i][idx][0] = -1;
            user_pipe[i][idx][1] = -1;
        }
        if(user_pipe[idx][i][0] != -1) {
            close(user_pipe[idx][i][0]);
            close(user_pipe[idx][i][1]);
            user_pipe[idx][i][0] = -1;
            user_pipe[idx][i][1] = -1;
        }
    }
}

int npshell(int idx) {
    static int flag = 0;
    char line[MAX_LINE_LEN];
    char line_cpy[MAX_LINE_LEN];
    char msg[MAX_MSG_LEN];
    char *cmd[MAX_CMD_PIPE];
    int pipe_pos;
    pid_t pid;
    int num_pipe_exist;
    static int num_pipe[MAX_CLI_NUM][MAX_NUM_PIPE][2];
    static int user_pipe[MAX_CLI_NUM][MAX_CLI_NUM][2];
    int infd, outfd, closefd, override_infd;
    int err_redir;
    long N_pipe;
    char* endptr;
    int i, j, k;

    if(flag == 0) {
        signal(SIGCHLD, shell_sigchld_handler);

        for(i = 0; i < MAX_CLI_NUM; i++) {
            for(j = 0; j < MAX_NUM_PIPE; j++) {
                num_pipe[i][j][0] = -1;
                num_pipe[i][j][1] = -1;
            }
        }

        for(i = 0; i < MAX_CLI_NUM; i++) {
            for(j = 0; j < MAX_CLI_NUM; j++) {
                user_pipe[i][j][0] = -1;
                user_pipe[i][j][1] = -1;
            }
        }

        flag = 1;
    }

    clearenv();
    setenv("PATH", path[idx], 1);

    memset(line, 0, MAX_LINE_LEN);

    num_pipe_exist = 0;
    override_infd = -1;

    if(read(STDIN_FILENO, line, MAX_LINE_LEN) != 0) {
        /* Ctrl+D for telnet */
        if((int)line[0] == 4) {
            close_fds(idx, num_pipe, user_pipe);
            write(STDOUT_FILENO, "\n", 1);
            return 1;
        }

        if(strcmp(line, "\r\n") == 0) {
            /* prompt */
            write(STDOUT_FILENO, "% ", 2);
            return 0;
        }

        strcpy(line, strtok(line, "\r\n"));
        strcpy(line_cpy, line);

        parse_pipe(line, &cmd[0]);

        /* exit command */
        if(strcmp(line, "exit") == 0) {
            close_fds(idx, num_pipe, user_pipe);
            return 1;
        }

        /* check due numbered-pipes */
        infd = num_pipe[idx][0][0];

        outfd = -1;
        err_redir = -1;

        /* shift num_pipe */
        for(i = 1; i < MAX_NUM_PIPE; i++) {
            num_pipe[idx][i - 1][0] = num_pipe[idx][i][0];
            num_pipe[idx][i - 1][1] = num_pipe[idx][i][1];
        }
        num_pipe[idx][MAX_NUM_PIPE - 1][0] = -1;
        num_pipe[idx][MAX_NUM_PIPE - 1][1] = -1;

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
                if(num_pipe[idx][N_pipe - 1][0] == -1) {
                    int pfd[2];
                    pipe(pfd);
                    num_pipe[idx][N_pipe - 1][0] = pfd[0];
                    num_pipe[idx][N_pipe - 1][1] = pfd[1];
                }
                outfd = num_pipe[idx][N_pipe - 1][1];
                num_pipe_exist = 1;
            }
        }

        /* check STDERR-redirected numbered-pipes */
        if(cmd[pipe_pos - 1] != NULL) {
            for(i = 0; cmd[pipe_pos - 1][i] != '\0'; i++) {
                if(cmd[pipe_pos - 1][i] == '!') {
                    N_pipe = strtol(cmd[pipe_pos - 1] + (i + 1), &endptr, 0);
                    if(*endptr == '\0') {
                        /* only appear at the end of line */
                        /* there is no other characters except numbers after '!' */
                        cmd[pipe_pos - 1][i] = '\0';
                        err_redir = 1;
                        if(num_pipe[idx][N_pipe - 1][0] == -1) {
                            int pfd[2];
                            pipe(pfd);
                            num_pipe[idx][N_pipe - 1][0] = pfd[0];
                            num_pipe[idx][N_pipe - 1][1] = pfd[1];
                        }
                        outfd = num_pipe[idx][N_pipe - 1][1];
                        num_pipe_exist = 1;
                    }
                }
            }
        }

        /* close written pipe */
        closefd = num_pipe[idx][0][1];

        /* built-in commands */
        char *argv = strtok(line_cpy, " ");
        if(strcmp(argv, "printenv") == 0) {
            argv = strtok(NULL, " ");
            if(argv == NULL) {
                extern char **environ;
                for(i = 0; environ[i] != NULL; i++) {
                    char msg[MAX_CMD_LEN];
                    sprintf(msg, "%s\n", environ[i]);
                    write(STDOUT_FILENO, msg, strlen(msg));
                }
            }
            else {
                char* env;
                while(argv != NULL) {
                    env = getenv(argv);
                    if(env != NULL) {
                        char msg[MAX_CMD_LEN];
                        sprintf(msg, "%s\n", env);
                        write(STDOUT_FILENO, msg, strlen(msg));
                    }
                    argv = strtok(NULL, " ");
                }
            }

            close(infd);
            close(closefd);

            /* prompt */
            write(STDOUT_FILENO, "% ", 2);

            return 0;
        }
        else if(strcmp(argv, "setenv") == 0) {
            char *name = strtok(NULL, " ");
            char *value = strtok(NULL, " ");
            if(setenv(name, value, 1) < 0)
                perror("setenv error");
            if(strcmp(name, "PATH") == 0)
                strcpy(path[idx], value);

            close(infd);
            close(closefd);

            /* prompt */
            write(STDOUT_FILENO, "% ", 2);

            return 0;
        }

        /* remote working ground (rwg) commands */
        if(strcmp(argv, "who") == 0) {
            strcpy(msg, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
            write(STDOUT_FILENO, msg, strlen(msg));

            for(i = 0; i <= maxi; i++) {
                if(clientfd[i] == -1)
                    continue;

                char nickname[MAX_MSG_LEN] = "(no name)";
                if(username[i] != NULL)
                    strcpy(nickname, username[i]);

                if(i == idx) {
                    sprintf(msg, "%d\t%s\t%s/%d\t%s\n", i + 1, nickname, cliip[i], 
                                                        cliport[i], "<-me");
                    write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));
                }
                else {
                    sprintf(msg, "%d\t%s\t%s/%d\n", i + 1, nickname, cliip[i], 
                                                    cliport[i]);
                    write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));
                }
            }

            close(infd);
            close(closefd);

            /* prompt */
            write(STDOUT_FILENO, "% ", 2);

            return 0;
        }
        else if(strcmp(argv, "tell") == 0) {
            char *userid_str = strtok(NULL, " ");

            if(clientfd[atoi(userid_str) - 1] == -1) {
                sprintf(msg, "*** Error: user #%s does not exist yet. ***\n", userid_str);
                write(clientfd[idx], msg, strlen(msg) * sizeof(char));
            }
            else {
                if(username[idx] != NULL)
                    sprintf(msg, "*** %s told you ***: %s\n", username[idx], 
                                 line_cpy + strlen(argv) + strlen(userid_str) + 2);
                else
                    sprintf(msg, "*** (no name) told you ***: %s\n", 
                                 line_cpy + strlen(argv) + strlen(userid_str) + 2);
                write(clientfd[atoi(userid_str) - 1], msg, strlen(msg) * sizeof(char));
            }

            close(infd);
            close(closefd);

            /* prompt */
            write(STDOUT_FILENO, "% ", 2);

            return 0;
        }
        else if(strcmp(argv, "yell") == 0) {
            if(username[idx] != NULL)
                sprintf(msg, "*** %s yelled ***: %s\n", username[idx], 
                                                        line_cpy + strlen(argv) + 1);
            else
                sprintf(msg, "*** (no name) yelled ***: %s\n", line_cpy + strlen(argv) + 1);

            for(i = 0; i <= maxi; i++)
                write(clientfd[i], msg, strlen(msg));

            close(infd);
            close(closefd);

            /* prompt */
            write(STDOUT_FILENO, "% ", 2);

            return 0;
        }
        else if(strcmp(argv, "name") == 0) {
            char *newname = strtok(NULL, " ");
            for(i = 0; i <= maxi; i++) {
                if(username[i] != NULL && strcmp(newname, username[i]) == 0) {
                    sprintf(msg, "*** User '%s' already exists. ***\n", newname);
                    write(STDOUT_FILENO, msg, strlen(msg));
                    break;
                }
            }

            if(i == maxi + 1) {
                username[idx] = malloc(MAX_MSG_LEN * sizeof(char));
                strcpy(username[idx], newname);

                sprintf(msg, "*** User from %s/%d is named '%s'. ***\n", cliip[idx], 
                                                                         cliport[idx], 
                                                                         username[idx]);
                for(i = 0; i <= maxi; i++)
                    write(clientfd[i], msg, strlen(msg));
            }

            close(infd);
            close(closefd);

            /* prompt */
            write(STDOUT_FILENO, "% ", 2);

            return 0;
        }

        if(cmd[pipe_pos - 1] != NULL) {
            /* get string length of cmd[pipe_pos - 1] in advance */
            /* in case of pipe_pos = 1 and cmd is modified by user pipe `<` */
            k = strlen(cmd[pipe_pos - 1]);
        }

        /* check user pipe */
        line_cpy[strlen(argv)] = ' ';
        if(cmd[0] != NULL) {
            j = strlen(cmd[0]);
            for(i = 0; i < j; i++) {
                if(cmd[0][i] == '<') {
                    N_pipe = strtol(cmd[0] + (i + 1), &endptr, 0);
                    if(endptr == cmd[0] + (i + 1))
                        break;
                    if(*endptr == '\0' || endptr[0] == ' ') {
                        cmd[0][i] = '\0';
                        if(clientfd[N_pipe - 1] == -1) {
                            sprintf(msg, "*** Error: user #%d does not exist yet. ***\n",
                                         (int)N_pipe);
                            write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));

                            close(infd);
                            close(closefd);

                            /* prompt */
                            write(STDOUT_FILENO, "% ", 2);

                            return 0;
                        }
                        else if(user_pipe[N_pipe - 1][idx][0] == -1) {
                            sprintf(msg, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", 
                                         (int)N_pipe, idx + 1);
                            write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));

                            close(infd);
                            close(closefd);

                            /* prompt */
                            write(STDOUT_FILENO, "% ", 2);

                            return 0;
                        }
                        else {
                            /* override numbered-pipes */
                            close(user_pipe[N_pipe - 1][idx][1]);
                            user_pipe[N_pipe - 1][idx][1] = -1;
                            override_infd = infd;
                            infd = user_pipe[N_pipe - 1][idx][0];
                            user_pipe[N_pipe - 1][idx][0] = -1;

                            char sndname[MAX_MSG_LEN] = "(no name)";
                            char rcvname[MAX_MSG_LEN] = "(no name)";
                            if(username[N_pipe - 1] != NULL)
                                strcpy(sndname, username[N_pipe - 1]);
                            if(username[idx] != NULL)
                                strcpy(rcvname, username[idx]);
                            sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
                                         rcvname, idx + 1, sndname, (int)N_pipe, line_cpy);
                            for(i = 0; i <= maxi; i++) {
                                if(clientfd[i] != -1)
                                    write(clientfd[i], msg, strlen(msg) * sizeof(char));
                            }

                            break;
                        }
                    }
                }
            }
        }

        if(cmd[pipe_pos - 1] != NULL) {
            for(i = 0; i < k; i++) {
                if(cmd[pipe_pos - 1][i] == '>') {
                    N_pipe = strtol(cmd[pipe_pos - 1] + (i + 1), &endptr, 0);
                    if(endptr == cmd[pipe_pos - 1] + (i + 1))
                        break;
                    if(*endptr == '\0' || endptr[0] == ' ') {
                        cmd[pipe_pos - 1][i] = '\0';
                        if(clientfd[N_pipe - 1] == -1) {
                            sprintf(msg, "*** Error: user #%d does not exist yet. ***\n",
                                         (int)N_pipe);
                            write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));

                            close(infd);
                            close(closefd);

                            /* prompt */
                            write(STDOUT_FILENO, "% ", 2);

                            return 0;
                        }
                        else if(user_pipe[idx][N_pipe - 1][1] != -1) {
                            sprintf(msg, "*** Error: the pipe #%d->#%d already exists. ***\n", 
                                         idx + 1, (int)N_pipe);
                            write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));

                            close(infd);
                            close(closefd);

                            /* prompt */
                            write(STDOUT_FILENO, "% ", 2);

                            return 0;
                        }
                        else {
                            int pfd[2];
                            pipe(pfd);
                            user_pipe[idx][N_pipe - 1][0] = pfd[0];
                            user_pipe[idx][N_pipe - 1][1] = pfd[1];

                            outfd = user_pipe[idx][N_pipe - 1][1];
                            num_pipe_exist = 1;
                            
                            char sndname[MAX_MSG_LEN] = "(no name)";
                            char rcvname[MAX_MSG_LEN] = "(no name)";
                            if(username[idx] != NULL)
                                strcpy(sndname, username[idx]);
                            if(username[N_pipe - 1] != NULL)
                                strcpy(rcvname, username[N_pipe - 1]);
                            sprintf(msg, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n",
                                         sndname, idx + 1, line_cpy, rcvname, (int)N_pipe);
                            for(i = 0; i <= maxi; i++) {
                                if(clientfd[i] != -1)
                                    write(clientfd[i], msg, strlen(msg) * sizeof(char));
                            }

                            break;
                        } 
                    }
                }
            }
        }

        /* other commands */
        while(shell_fork_num > MAX_FORK_NUM);

        shell_fork_num++;
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

            signal(SIGCHLD, process_sigchld_handler);

            child_process(cmd, 0, infd, outfd, err_redir);
        }
        else {
            /* parent process */
            if(num_pipe_exist)
                waitpid(pid, NULL, WNOHANG);
            else
                waitpid(pid, NULL, 0);
            
            close(infd);
            close(override_infd);
            close(closefd);

            /* prompt */
            write(STDOUT_FILENO, "% ", 2);
        }
    }
    else {
        close_fds(idx, num_pipe, user_pipe);
        write(STDOUT_FILENO, "\n", 1);
        return 1;
    }
    return 0;
}
