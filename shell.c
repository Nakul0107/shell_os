#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>


#define MAX_ARGS 64
#define DELIM " \t\r\n"


char *read_line(void){

    char *line= NULL;
    size_t buffer_size=0;

    getline(&line,&buffer_size,stdin);

    return line;

}
char **parse_line(char *line)
{
    int buf_size= MAX_ARGS;
    int position=0;
    char **tokens= malloc(buf_size* sizeof(char *));
    char *token;

    token= strtok(line,DELIM);

    while(token!=NULL)
    {
        tokens[position]= token;
        position++;
        token= strtok(NULL,DELIM);
    }

    tokens[position]=NULL;

    return tokens;

}

int has_pipe(char *line)
{
    return strchr(line,'|')!=NULL;
}


void launch_pipe(char *line) {
    char *cmd1_str = strtok(line, "|");
    char *cmd2_str = strtok(NULL, "|");

    char **cmd1 = parse_line(cmd1_str);
    char **cmd2 = parse_line(cmd2_str);

    // printf("Command 1 first token: %s\n", cmd1[0]);
    // printf("Command 2 first token: %s\n", cmd2[0]);

    int pipefd[2];
    pipe(pipefd);

    pid_t pid1 = fork();

    if (pid1 == 0) {
        // ---- CHILD 1 yaha (ls banega) ----
        dup2(pipefd[1], STDOUT_FILENO);

        close(pipefd[0]);
        close(pipefd[1]);

        execvp(cmd1[0], cmd1);
        perror("shell");
        exit(1);
    }

    pid_t pid2 = fork();

    if (pid2 == 0) {
        // ---- CHILD 2 yaha (wc banega) ----
        dup2(pipefd[0], STDIN_FILENO);

        close(pipefd[0]);
        close(pipefd[1]);

        execvp(cmd2[0], cmd2);
        perror("shell");
        exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

void launch_pipe_multi(char *line) {
    char *commands_str[10];
    int num_cmds = 0;

    char *token = strtok(line, "|");
    while (token != NULL && num_cmds < 10) {
        commands_str[num_cmds] = token;
        num_cmds++;
        token = strtok(NULL, "|");
    }

    char **commands[10];
    for (int i = 0; i < num_cmds; i++) {
        commands[i] = parse_line(commands_str[i]);
    }

    int pipes[9][2]; // N commands ke liye max N-1 pipes chahiye (max 9 for 10 cmds)
    for (int i = 0; i < num_cmds - 1; i++) {
        pipe(pipes[i]);
    }

    pid_t pids[10];

    for (int i = 0; i < num_cmds; i++) {
        pids[i] = fork();

        if (pids[i] == 0) {
            // ---- CHILD i ----
            signal(SIGINT, SIG_DFL);

            // agar pehla command nahi hai, stdin pichhle pipe ke read-end se lo
            if (i != 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }

            // agar aakhri command nahi hai, stdout agle pipe ke write-end mein bhejo
            if (i != num_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // is child ko har pipe ke raw fds band karne hain (unused ones)
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execvp(commands[i][0], commands[i]);
            perror("shell");
            exit(1);
        }
    }

    // ---- PARENT yaha ----
    // saare pipe fds parent mein bhi close karo
    for (int i = 0; i < num_cmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // saare children ka wait karo
    for (int i = 0; i < num_cmds; i++) {
        waitpid(pids[i], NULL, 0);
    }
}
int launch_process(char **args)
{
    
    pid_t pid;
    int status;
    int background = 0;

    // check karo last argument "&" hai kya
    int i = 0;
    while (args[i] != NULL) {
        i++;
    }
    // ab i us position pe hai jaha NULL hai, matlab i-1 pe last real token hai

    if (i > 0 && strcmp(args[i-1], "&") == 0) {
        background = 1;
        args[i-1] = NULL; // "&" ko array se hata do
    }

    pid = fork();
    if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], ">") == 0) {
                    int fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);

                    args[i] = NULL;
                    break;
                }

                if (strcmp(args[i], "<") == 0) {
                    int fd = open(args[i+1], O_RDONLY);
                    dup2(fd, STDIN_FILENO);
                    close(fd);

                    args[i] = NULL;
                    break;
                }
             }

            execvp(args[0], args);
            perror("shell");
            exit(1);
    }
    else if(pid<0)
    {
        perror("Shell");
    }

    else
    {
        if (background) {
            printf("[background] pid %d started\n", pid);
        } else {
            waitpid(pid, &status, 0);
        }
    }
    return 1;

}
int handle_builtin(char **args) {
    if (args[0] == NULL) {
        return 1; // khali line, kuch mat karo
    }

    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "shell: cd needs an argument\n");
        } else {
            if (chdir(args[1]) != 0) {
                perror("shell");
            }
        }
        return 1;
    }

    return -1; // -1 matlab ye built-in nahi hai, normal command hai
}
int main(void) {
    char *line;
    char **args;
    signal(SIGINT,SIG_IGN);
    while (1) {
        printf("myshell> ");
        fflush(stdout);



        line = read_line();

        if (has_pipe(line)) {
            launch_pipe_multi(line);
            continue; // is baar loop ka baaki part skip karo
        }
        args = parse_line(line);

        int result = handle_builtin(args);
        if (result == -1) {
            launch_process(args);
        }

        free(line);
        free(args);
    }

    return 0;
}