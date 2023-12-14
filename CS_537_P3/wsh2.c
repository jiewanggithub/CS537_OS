#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

void read_input(char** input, FILE* input_source) {
    size_t len = 0;

    if (getline(input, &len, input_source) == -1) {
        fclose(input_source);
        exit(-1);
    }

    int input_len = strlen(*input);
    if (input_len > 0 && (*input)[input_len - 1] == '\n') {
        (*input)[input_len - 1] = '\0';
    }
}

void parse_input(char* input, job* j, int* command_length, int* is_background) {
    char* input_cpy;
    input_cpy = malloc(strlen(input) + 1);
    strcpy(input_cpy, input);

    strcpy(j->input, input);
    j->pgid = 0;

    char* token = strtok(input_cpy, " ");
    process* proc = (struct process*)malloc(sizeof(struct process));
    proc->is_completed = 0;
    proc->is_stopped = 0;
    j->first_proc = proc;
    int i = 0;

    while (token != NULL && i < 1024) {
        if (strcmp(token, "|") == 0) {
            proc->argv[i] = NULL;
            proc->next_proc = (struct process*)malloc(sizeof(struct process));
            proc = proc->next_proc;
            proc->is_completed = 0;
            proc->is_stopped = 0;
            i = 0;
        } else {
            proc->argv[i] = malloc(strlen(token) + 1);
            strcpy(proc->argv[i], token);
            i++;
        }
        token = strtok(NULL, " ");
    }

    if (i > 1 && strcmp(proc->argv[i - 1], "&") == 0) {
        *is_background = 1;
        proc->argv[i - 1] = NULL;
        i--;
    } else {
        proc->argv[i] = NULL;
    }

    proc->next_proc = NULL;
    *command_length = i;
    free(input_cpy);
}

int is_job_completed(job* j) {
    for (process* proc = j->first_proc; proc; proc = proc->next_proc) {
        if (!proc->is_completed) {
            return 0;
        }
    }
    return 1;
}

int is_job_stopped(job* j) {
    for (process* proc = j->first_proc; proc; proc = proc->next_proc) {
        if (!proc->is_stopped && !proc->is_completed) {
            return 0;
        }
    }
    return 1;
}

int update_job_status(pid_t pid, int status) {
    if (pid > 0) {
        for (int i = 0; i < 1024; i++) {
            process* proc;
            for (proc = jobs[i].first_proc; proc; proc = proc->next_proc) {
                if (proc->pid == pid) {
                    if (WIFSTOPPED(status)) {
                        proc->is_stopped = 1;
                    } else if (WIFEXITED(status || WIFSIGNALED(status))) {
                        proc->is_completed = 1;
                    }
                    return 0;
                }
            }
        }
        return -1;
    } else {
        return -1;
    }
}

void maintain_jobs() {
    for (int i = 0; i < 1024; i++) {
        if (jobs[i].pgid != -1) {
            process* proc;
            for (proc = jobs[i].first_proc; proc != NULL; proc = proc->next_proc) {
                pid_t pid = 0;
                int status = 0;
                pid = waitpid(proc->pid, &status, WNOHANG);
                if (pid > 0) {
                    if (WIFSTOPPED(status)) {
                        proc->is_stopped = 1;
                    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                        proc->is_completed = 1;
                    }
                }
            }

            if (is_job_completed(&jobs[i])) {
                jobs[i].pgid = -1;
                struct process* head = jobs[i].first_proc;
                struct process* temp;
                while (head != NULL) {
                    temp = head;
                    head = head->next_proc;
                    free(temp);
                }
            }
        }
    }
}

void execute_input(job* j, int command_length, int is_background) {
    if (strcmp(j->first_proc->argv[0], "exit") == 0) {
        if (command_length != 1) {
            exit(-1);
        } else {
            exit(0);
        }
    } else if (strcmp(j->first_proc->argv[0], "cd") == 0) {
        if (command_length != 2) {
            exit(-1);
        } else {
            if (chdir(j->first_proc->argv[1]) != 0) {
                exit(-1);
            }
        }
    } else if (strcmp(j->first_proc->argv[0], "jobs") == 0) {        
        for (int i = 0; i < 1024; i++) {
            if (jobs[i].pgid != -1) {
                printf("%d: %s, is_stopped = %d, is_completed = %d\n", i + 1, jobs[i].input, is_job_stopped(&jobs[i]), is_job_completed(&jobs[i]));
            }
        }
    } else if (strcmp(j->first_proc->argv[0], "fg") == 0) {
        if (command_length == 1) {
            for (int i = 1023; i >= 0; i--) {
                if (jobs[i].pgid != -1) {
                    put_job_in_foreground(&jobs[i]);
                    break;
                }
            }
        } else if (command_length == 2) {
            int jobid = *j->first_proc->argv[1] - '0' - 1;
            put_job_in_foreground(&jobs[jobid]);
        }
    } else if (strcmp(j->first_proc->argv[0], "bg") == 0) {
        if (command_length == 1) {
            for (int i = 1023; i >= 0; i--) {
                if (jobs[i].pgid != -1 && is_job_stopped(&jobs[i])) {
                    put_job_in_background(&jobs[i]);
                    break;
                }
            }
        } else if (command_length == 2) {
            int jobid = *j->first_proc->argv[1] - '0' - 1;
            put_job_in_background(&jobs[jobid]);
        }
    } else {
        process* proc;
        pid_t pid;
        int pipefd[2], in, out;

        in = STDIN_FILENO;
        for (proc = j->first_proc; proc; proc = proc->next_proc) {
            if (proc->next_proc) {
                pipe(pipefd);
                out = pipefd[1];
            } else {
                out = STDOUT_FILENO;
            }
            
            pid = fork();
            if (pid == -1) {
                exit(-1);
            } else if (pid == 0) {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);

                pid = getpid();
                if (j->pgid == 0) {
                    j->pgid = pid;
                }
                setpgid(pid, j->pgid);

                if (j->first_proc != proc) {
                    dup2(in, STDIN_FILENO);
                    close(in);
                }
                if (proc->next_proc) {
                    dup2(out, STDOUT_FILENO);
                    close(out);
                }

                exit(execvp(proc->argv[0], proc->argv));
            } else {
                proc->pid = pid;

                if (j->pgid == 0) {
                    j->pgid = pid;
                }
                setpgid(pid, j->pgid);
            }

            if (in != STDIN_FILENO) {
                close(in);
            }
            if (out != STDOUT_FILENO) {
                close(out);
            }
            in = pipefd[0];
        }

        for (int i = 0; i < 1024; i++) {
            if (jobs[i].pgid == -1) {
                jobs[i] = *j;
                break;
            }
        }

        if (!is_background) {
            put_job_in_foreground(j);
        } else {
            put_job_in_background(j);
        }
    }
}

void put_job_in_foreground(job* j) {
    tcsetpgrp(shell_terminal, j->pgid);
    if (is_job_stopped(j)) {
        if (kill(- j->pgid, SIGCONT) == 0) {
            for (process* proc = j->first_proc; proc; proc = proc->next_proc) {
                if (proc->is_stopped) {
                    proc->is_stopped = 0;
                }
            }
        }
    }

    int status;
    pid_t pid;
    do {
        pid = waitpid(-j->pgid, &status, WUNTRACED);
    } while (!update_job_status(pid, status) && !is_job_completed(j) && !is_job_stopped(j));
    tcsetpgrp(shell_terminal, shell_pid);
}

void put_job_in_background(job* j) {
    if (is_job_stopped(j)) {
        if (kill(- j->pgid, SIGCONT) == 0) {
            for (process* proc = j->first_proc; proc; proc = proc->next_proc) {
                if (proc->is_stopped) {
                    proc->is_stopped = 0;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    char* input = malloc(sizeof(char) * 1024);
    int command_length = 0;
    FILE* input_source;

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    shell_pid = getpid();
    setpgid(shell_pid, shell_pid);
    tcsetpgrp(shell_terminal, shell_pid);

    if (argc == 1) {
        input_source = stdin;
    } else if (argc == 2) {
        input_source = fopen(argv[1], "r");
        if (input_source == NULL) {
            exit(-1);
        }
    } else {
        exit(-1);
    }

    for (int i = 0; i < 1024; i++) {
        jobs[i].pgid = -1;
    }

    while (1) {
        int is_background = 0;
        struct job* j = (struct job*)malloc(sizeof(struct job));

        if (argc == 1) {
            printf("wsh> ");
        }

        read_input(&input, input_source);

        parse_input(input, j, &command_length, &is_background);

        maintain_jobs();

        if (command_length > 0) {
            execute_input(j, command_length, is_background);
        }
        free(input);
    }

    return 0;
}