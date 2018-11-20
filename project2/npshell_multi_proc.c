#include "npshell_multi_proc.h"

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

int check_file_exist(char *filename) {
    struct stat sb;
    return (stat(filename, &sb) == 0);
}

void close_fds(int idx, int num_pipe[MAX_NUM_PIPE][2]) {
    int i;
    char user_pipe_file[MAX_MSG_LEN];
    /* close pipe fd */
    for(i = 0; i < MAX_NUM_PIPE; i++) {
        if(num_pipe[i][0] != -1)
            close(num_pipe[i][0]);
        if(num_pipe[i][1] != -1)
            close(num_pipe[i][1]);
    }
    /* remove user pipe file */
    for(i = 0; i < MAX_CLI_NUM; i++) {
        sprintf(user_pipe_file, "%s/%dto%d.txt", USER_PIPE_DIR, idx, i);
        if(check_file_exist(user_pipe_file))
            remove(user_pipe_file);
        sprintf(user_pipe_file, "%s/%dto%d.txt", USER_PIPE_DIR, i, idx);
        if(check_file_exist(user_pipe_file))
            remove(user_pipe_file);
    }
}

int npshell(int idx) {
    char line[MAX_LINE_LEN];
    char line_cpy[MAX_LINE_LEN];
    char msg[MAX_MSG_LEN];
    char *cmd[MAX_CMD_PIPE];
    int pipe_pos;
    pid_t pid;
    int num_pipe[MAX_NUM_PIPE][2];
    int num_pipe_exist;
    char user_pipe_file[MAX_MSG_LEN], del_file[MAX_MSG_LEN];
    int user_pipe_flag, file_to_del;
    int infd, outfd, closefd, override_infd;
    int err_redir;
    long N_pipe;
    char* endptr;
    int i, j, k;

    int maxfd;
    int nready;
    fd_set rset, allset;

    int *clientfd, *cliport;
    char *cliip[MAX_CLI_NUM], *username[MAX_CLI_NUM];

    signal(SIGCHLD, shell_sigchld_handler);

    for(i = 0; i < MAX_NUM_PIPE; i++) {
        num_pipe[i][0] = -1;
        num_pipe[i][1] = -1;
    }

    clientfd = (int*)shmat(clientfdkey, NULL, 0);
    cliport = (int*)shmat(cliportkey, NULL, 0);
    for(i = 0; i < MAX_CLI_NUM; i++) {
        cliip[i] = (char*)shmat(cliipkey[i], NULL, 0);
        username[i] = (char*)shmat(usernamekey[i], NULL, 0);
    }

    clearenv();
    setenv("PATH", "bin:.", 1);

    maxfd = (STDIN_FILENO > rcvfd) ? STDIN_FILENO : rcvfd;
    FD_ZERO(&allset);
    FD_SET(STDIN_FILENO, &allset);
    FD_SET(rcvfd, &allset);

    write(STDOUT_FILENO, "% ", 2);

    /* start of npshell */
    for(;;) {
        rset = allset;
        while((nready = Select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0);
        
        if(FD_ISSET(STDIN_FILENO, &rset)) {
            memset(line, 0, MAX_LINE_LEN);

            num_pipe_exist = 0;
            user_pipe_flag = 0;
            file_to_del = 0;
            override_infd = -1;
        
            if(read(STDIN_FILENO, line, MAX_LINE_LEN) != 0) {
                /* Ctrl+D for telnet */
                if((int)line[0] == 4) {
                    close_fds(idx, num_pipe);
                    write(STDOUT_FILENO, "\n", 1);
                    break;
                }
        
                if(strcmp(line, "\r\n") == 0) {
                    /* prompt */
                    write(STDOUT_FILENO, "% ", 2);
                    continue;
                }
        
                strcpy(line, strtok(line, "\r\n"));
                strcpy(line_cpy, line);
        
                parse_pipe(line, &cmd[0]);
        
                /* exit command */
                if(strcmp(line, "exit") == 0) {
                    close_fds(idx, num_pipe);
                    break;
                }
        
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
                                if(num_pipe[N_pipe - 1][0] == -1) {
                                    int pfd[2];
                                    pipe(pfd);
                                    num_pipe[N_pipe - 1][0] = pfd[0];
                                    num_pipe[N_pipe - 1][1] = pfd[1];
                                }
                                outfd = num_pipe[N_pipe - 1][1];
                                num_pipe_exist = 1;
                            }
                        }
                    }
                }
        
                /* close written pipe */
                closefd = num_pipe[0][1];
        
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

                    continue;
                }
                else if(strcmp(argv, "setenv") == 0) {
                    char *name = strtok(NULL, " ");
                    char *value = strtok(NULL, " ");
                    if(setenv(name, value, 1) < 0)
                        perror("setenv error");
        
                    close(infd);
                    close(closefd);

                    /* prompt */
                    write(STDOUT_FILENO, "% ", 2);

                    continue;
                }
        
                /* remote working ground (rwg) commands */
                if(strcmp(argv, "who") == 0) {
                    strcpy(msg, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
                    write(STDOUT_FILENO, msg, strlen(msg));

                    sem_wait(clientfdsem);
                    sem_wait(usernamesem);
                    for(i = 0; i < MAX_CLI_NUM; i++) {
                        if(clientfd[i] == -1)
                            continue;

                        char nickname[MAX_MSG_LEN] = "(no name)";
                        if(strlen(username[i]) != 0)
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
                    sem_post(usernamesem);
                    sem_post(clientfdsem);
        
                    close(infd);
                    close(closefd);

                    /* prompt */
                    write(STDOUT_FILENO, "% ", 2);
     
                    continue;
                }
                else if(strcmp(argv, "tell") == 0) {
                    char *userid_str = strtok(NULL, " ");

                    sem_wait(clientfdsem);
                    sem_wait(usernamesem);
                    if(clientfd[atoi(userid_str) - 1] == -1) {
                        sprintf(msg, "*** Error: user #%s does not exist yet. ***\n", userid_str);
                        write(clientfd[idx], msg, strlen(msg) * sizeof(char));
                    }
                    else {
                        if(strlen(username[idx]) != 0)
                            sprintf(msg, "%d *** %s told you ***: %s\n", 
                                         atoi(userid_str) - 1,
                                         username[idx], 
                                         line_cpy + strlen(argv) + strlen(userid_str) + 2);
                        else
                            sprintf(msg, "%d *** (no name) told you ***: %s\n", 
                                         atoi(userid_str) - 1, 
                                         line_cpy + strlen(argv) + strlen(userid_str) + 2);
                        write(tellfd, msg, strlen(msg) * sizeof(char));
                    }
                    sem_post(usernamesem);
                    sem_post(clientfdsem);
        
                    close(infd);
                    close(closefd);

                    /* prompt */
                    write(STDOUT_FILENO, "% ", 2);

                    continue;
                }
                else if(strcmp(argv, "yell") == 0) {
                    sem_wait(usernamesem);
                    if(strlen(username[idx]) != 0)
                        sprintf(msg, "*** %s yelled ***: %s\n", username[idx], 
                                                                line_cpy + strlen(argv) + 1);
                    else
                        sprintf(msg, "*** (no name) yelled ***: %s\n", line_cpy + strlen(argv) + 1);
                    sem_post(usernamesem);

                    write(clientfd[idx], msg, strlen(msg));

                    char bcstmsg[MAX_MSG_LEN];
                    sprintf(bcstmsg, "%d %s", idx, msg);
                    write(bcstfd, bcstmsg, strlen(bcstmsg));
        
                    close(infd);
                    close(closefd);

                    /* prompt */
                    write(STDOUT_FILENO, "% ", 2);

                    continue;
                }
                else if(strcmp(argv, "name") == 0) {
                    char *newname = strtok(NULL, " ");
                    sem_wait(usernamesem);
                    for(i = 0; i < MAX_CLI_NUM; i++) {
                        if(strlen(username[i]) != 0 && strcmp(newname, username[i]) == 0) {
                            sprintf(msg, "*** User '%s' already exists. ***\n", newname);
                            write(STDOUT_FILENO, msg, strlen(msg));
                            break;
                        }
                    }
                    sem_post(usernamesem);
        
                    if(i == MAX_CLI_NUM) {
                        sem_wait(usernamesem);
                        strcpy(username[idx], newname);
        
                        sprintf(msg, "*** User from %s/%d is named '%s'. ***\n", cliip[idx], 
                                                                                 cliport[idx], 
                                                                                 username[idx]);
                        sem_post(usernamesem);

                        write(clientfd[idx], msg, strlen(msg));

                        char bcstmsg[MAX_MSG_LEN];
                        sprintf(bcstmsg, "%d %s", idx, msg);
                        write(bcstfd, bcstmsg, strlen(bcstmsg));
                    }
        
                    close(infd);
                    close(closefd);

                    /* prompt */
                    write(STDOUT_FILENO, "% ", 2);

                    continue;
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
                                sem_wait(clientfdsem);
                                if(clientfd[N_pipe - 1] == -1) {
                                    sprintf(msg, "*** Error: user #%d does not exist yet. ***\n",
                                                 (int)N_pipe);
                                    write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));
        
                                    close(infd);
                                    close(closefd);

                                    user_pipe_flag = 1;

                                    sem_post(clientfdsem);
                                    break;
                                }
                                sem_post(clientfdsem);

                                sprintf(user_pipe_file, "%s/%dto%d.txt", USER_PIPE_DIR, 
                                                                         (int)N_pipe - 1, 
                                                                         idx);
                                if(!check_file_exist(user_pipe_file)) {
                                    sprintf(msg, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", 
                                                 (int)N_pipe, idx + 1);
                                    write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));
        
                                    close(infd);
                                    close(closefd);

                                    user_pipe_flag = 1;

                                    break;
                                }
                                else {
                                    /* override numbered-pipes */
                                    override_infd = infd;
                                    infd = open(user_pipe_file, O_RDONLY);
                                    strcpy(del_file, user_pipe_file);
                                    file_to_del = 1;
        
                                    char sndname[MAX_MSG_LEN] = "(no name)";
                                    char rcvname[MAX_MSG_LEN] = "(no name)";
                                    sem_wait(usernamesem);
                                    if(strlen(username[N_pipe - 1]) != 0)
                                        strcpy(sndname, username[N_pipe - 1]);
                                    if(strlen(username[idx]) != 0)
                                        strcpy(rcvname, username[idx]);
                                    sem_post(usernamesem);
                                    sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
                                                 rcvname, idx + 1, sndname, (int)N_pipe, line_cpy);
                                    
                                    write(clientfd[idx], msg, strlen(msg) * sizeof(char));

                                    char bcstmsg[MAX_MSG_LEN];
                                    sprintf(bcstmsg, "%d %s", idx, msg);
                                    write(bcstfd, bcstmsg, strlen(bcstmsg) * sizeof(char));

                                    break;
                                }
                            }
                        }
                    }
                    if(user_pipe_flag == 1) {
                        /* prompt */
                        write(STDOUT_FILENO, "% ", 2);
                        continue;
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
                                sem_wait(clientfdsem);
                                if(clientfd[N_pipe - 1] == -1) {
                                    sprintf(msg, "*** Error: user #%d does not exist yet. ***\n",
                                                 (int)N_pipe);
                                    write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));
        
                                    close(infd);
                                    close(closefd);

                                    user_pipe_flag = 1;

                                    sem_post(clientfdsem);
                                    break;
                                }
                                sem_post(clientfdsem);

                                char user_pipe_file[MAX_MSG_LEN];
                                sprintf(user_pipe_file, "%s/%dto%d.txt", USER_PIPE_DIR, 
                                                                         idx,
                                                                         (int)N_pipe - 1); 
                                if(check_file_exist(user_pipe_file)) {
                                    sprintf(msg, "*** Error: the pipe #%d->#%d already exists. ***\n", 
                                                 idx + 1, (int)N_pipe);
                                    write(STDOUT_FILENO, msg, strlen(msg) * sizeof(char));
        
                                    close(infd);
                                    close(closefd);

                                    user_pipe_flag = 1;

                                    break;
                                }
                                else {
                                    outfd = open(user_pipe_file, O_WRONLY | O_CREAT, 0600);
                                    num_pipe_exist = 1;
                                    
                                    char sndname[MAX_MSG_LEN] = "(no name)";
                                    char rcvname[MAX_MSG_LEN] = "(no name)";
                                    sem_wait(usernamesem);
                                    if(strlen(username[idx]) != 0)
                                        strcpy(sndname, username[idx]);
                                    if(strlen(username[N_pipe - 1]) != 0)
                                        strcpy(rcvname, username[N_pipe - 1]);
                                    sem_post(usernamesem);
                                    sprintf(msg, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n",
                                                 sndname, idx + 1, line_cpy, rcvname, (int)N_pipe);

                                    write(clientfd[idx], msg, strlen(msg) * sizeof(char));

                                    char bcstmsg[MAX_MSG_LEN];
                                    sprintf(bcstmsg, "%d %s", idx, msg);
                                    write(bcstfd, bcstmsg, strlen(bcstmsg) * sizeof(char));

                                    break;
                                } 
                            }
                        }
                    }
                    if(user_pipe_flag == 1) {
                        /* prompt */
                        write(STDOUT_FILENO, "% ", 2);
                        continue;
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

                    if(file_to_del) {
                        remove(del_file);
                    }

                    /* prompt */
                    write(STDOUT_FILENO, "% ", 2);
                }
            }
            else {
                close_fds(idx, num_pipe);
                write(STDOUT_FILENO, "\n", 1);
                break;
            }
        }

        if(FD_ISSET(rcvfd, &rset)) {
            /* broadcast message */
            if((i = read(rcvfd, msg, MAX_MSG_LEN)) != 0)
                write(STDOUT_FILENO, msg, i);
        }
    }

    return 0;
}
