#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 256

void parse_input(char *input, char **args, int *num_args, int *is_background);
int find_executable(char *cmd, char **executable_path);
char **excludeAmpersand(char *args[], int num_args);
void execute_command(char **args, int num_args, int is_background);
void add_job(char **args, int num_args, int status, pid_t pid, int id);
void interactive_mode();
void jobs_command();
void fg_command(int job_id);
void bg_command(int job_id);
void cd_command(char **args);

struct Job
{
    pid_t pgid;
    pid_t pid;           // process pid
    int id;              // background id
    char **command_args; // arguments from index 0
    int num_args;        // number of arguments from index 0
    int status;          // 0: background 1: forground 2: stopped
};

int shell_pgid;
char *PATH[] = {"/bin", "/usr/bin", NULL};
struct Job *jobs[MAX_ARGS];
int id[MAX_ARGS] = {0};
int cur_id = -1;

void sigint_handler(int sig)
{
    // Handle CTRL-C here
    if (cur_id != -1 && jobs[cur_id - 1] != NULL)
    {
        kill(jobs[cur_id - 1]->pgid, SIGINT);
        jobs[cur_id - 1]->status = 1;
    }
}
void sigtstp_handler(int sig)
{
    // Handle CTRL-Z here
    if (cur_id != -1 && jobs[cur_id - 1] != NULL)
    {
        kill(jobs[cur_id - 1]->pgid, SIGSTOP);
        jobs[cur_id - 1]->status = 2;
    }
}

int find_executable(char *cmd, char **executable_path)
{
    char file_path[MAX_INPUT_SIZE];
    char **path_ptr = PATH;

    while (*path_ptr != NULL)
    {
        snprintf(file_path, sizeof(file_path), "%s/%s", *path_ptr, cmd);
        if (access(file_path, X_OK) == 0)
        {
            *executable_path = strdup(file_path);
            return 1;
        }
        path_ptr++;
    }
    return 0;
}

char **excludeAmpersand(char *args[], int num_args)
{
    char **new_args = (char **)malloc((num_args + 1) * sizeof(char *)); // Allocate memory for the new array of strings
    int new_num_args = 0;

    // Copy arguments to the new array, excluding the "&"
    for (int i = 0; i < num_args; ++i)
    {
        if (strcmp(args[i], "&") != 0)
        {
            new_args[new_num_args] = strdup(args[i]); // Copy the argument to the new array
            new_num_args++;
        }
    }

    new_args[new_num_args] = NULL;
    return new_args;
}

void jobs_command()
{
    for (int i = 0; i < MAX_ARGS && jobs[i] != NULL; ++i)
    {
        if ((jobs[i]->status == 0) || (jobs[i]->status == 2))
        {
            printf("%d:", jobs[i]->id);
            for (int j = 0; jobs[i]->command_args[j] != NULL; ++j)
            {
                printf(" %s", jobs[i]->command_args[j]);
            }
            printf("\n");
        }
    }
}

void cd_command(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "Error: Missing argument for cd\n");
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror("chdir");
        }
    }
}

void parse_input(char *input, char **args, int *num_args, int *is_background)
{
    *num_args = 0;
    *is_background = 1;
    char *token = strtok(input, " ");
    while (token != NULL)
    {
        args[*num_args] = token;
        (*num_args)++;
        token = strtok(NULL, " ");
    }
    if (*num_args > 0 && strcmp(args[*num_args - 1], "&") == 0)
    {
        *is_background = 0;
    }
    args[*num_args] = NULL;
}

int next_proc_id()
{
    int new_id = 0;
    for (int i = 0; i < MAX_ARGS; i++)
    {
        if (id[i] == 0)
        {
            new_id = i + 1;
            id[i] = 1;
            break;
        }
    }
    return new_id;
}

void sigchild_handler(int sig)
{
    if (cur_id != -1 && jobs[cur_id - 1]->status == 1)
    {
        jobs[cur_id - 1] = NULL;
        free(jobs[cur_id - 1]);
        jobs[cur_id - 1] = NULL;
        id[cur_id - 1] = 0;
    }
    cur_id = -1;
}

void fg_command(int job_id)
{
    if (job_id == -1)
    {
        int largest_id = -1;
        for (int i = 0; i < MAX_ARGS; ++i)
        {
            if (jobs[i] != NULL && jobs[i]->id > largest_id && (jobs[i]->status == 0 || jobs[i]->status == 2))
            {
                largest_id = jobs[i]->id;
            }
        }
        if (largest_id != -1)
        {
            job_id = largest_id;
        }
        else
        {
            printf("No background jobs available.\n");
            return;
        }
    }

    struct Job *job = NULL;
    for (int i = 0; i < MAX_ARGS; ++i)
    {
        if (jobs[i] != NULL && jobs[i]->id == job_id && (jobs[i]->status == 0 || jobs[i]->status == 2))
        {
            jobs[i]->status = 1;
            job = jobs[i];
            break;
        }
    }

    if (job != NULL)
    {
        // Bring the job to the foreground
        tcsetpgrp(STDIN_FILENO, job->pgid);
        kill(-job->pgid, SIGCONT); // Resume the job in the foreground
        int status;

        waitpid(job->pgid, &status, WUNTRACED); // Wait for the job to finish or stop
        if (WIFSTOPPED(status))
        {
            job->status = 2;
        }
        tcsetpgrp(STDIN_FILENO, shell_pgid); // Set shell as the foreground process group again
    }
}

void bg_command(int job_id)
{
    if (job_id == -1)
    {
        // Find the largest id in the jobs array
        int largest_id = -1;
        for (int i = 0; i < MAX_ARGS; ++i)
        {
            if (jobs[i] != NULL && jobs[i]->id > largest_id && jobs[i]->status == 2)
            {
                largest_id = jobs[i]->id;
            }
        }
        if (largest_id != -1)
        {
            job_id = largest_id;
        }
        else
        {
            printf("No stopped background jobs available.\n");
            return;
        }
    }

    // Bring the specified stopped background job to the foreground
    struct Job *job = NULL;
    for (int i = 0; i < MAX_ARGS; ++i)
    {
        if (jobs[i] != NULL && jobs[i]->id == job_id && jobs[i]->status == 2)
        {
            jobs[i]->status = 0; // Change status to background
            job = jobs[i];
            break;
        }
    }

    if (job != NULL)
    {
        // Resume the job in the background
        kill(job->pgid, SIGCONT);
    }
    else
    {
        printf("No such stopped background job with id %d\n", job_id);
    }
}

void add_job(char **args, int num_args, int status, pid_t pid, int index)
{
    jobs[index - 1] = malloc(sizeof(struct Job));
    jobs[index - 1]->pid = pid;
    jobs[index - 1]->pgid = pid;
    jobs[index - 1]->id = index;
    jobs[index - 1]->command_args = malloc((num_args + 1) * sizeof(char *));
    for (int i = 0; i < num_args; ++i)
    {
        jobs[index - 1]->command_args[i] = strdup(args[i]);
    }
    jobs[index - 1]->command_args[num_args] = NULL;
    jobs[index - 1]->num_args = num_args;
    jobs[index - 1]->status = status;
}

void remove_finished_jobs()
{
    for (int i = 0; i < MAX_ARGS; ++i)
    {
        if (id[i] == 1)
        {
            int status;
            pid_t result = waitpid(jobs[i]->pid, &status, WNOHANG | WUNTRACED);

            if (result > 0)
            {
                free(jobs[i]->command_args);
                free(jobs[i]);
                jobs[i] = NULL;
                id[i] = 0;
            }
        }
    }
}

void execute_command(char **args, int num_args, int is_background)
{
    char *executable_path = NULL;
    if (strcmp(args[0], "exit") == 0)
    {
        exit(0);
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        cd_command(args);
    }
    else if (strcmp(args[0], "jobs") == 0)
    {
        jobs_command();
    }
    else if (strcmp(args[0], "fg") == 0 && (num_args == 1 || num_args == 2))
    {
        int job_id = (num_args == 2) ? atoi(args[1]) : -1;
        fg_command(job_id);
    }
    else if (strcmp(args[0], "bg") == 0 && (num_args == 1 || num_args == 2))
    {
        int job_id = (num_args > 1) ? atoi(args[1]) : -1;
        bg_command(job_id);
    }
    else if (find_executable(args[0], &executable_path))
    {
        int status;
        pid_t pid;

        if ((pid = fork()) == 0)
        {
            // Child process
            setpgid(0, 0);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            if (strcmp(args[num_args - 1], "&") == 0)
            {
                execvp(executable_path, excludeAmpersand(args, num_args));
            }
            else
            {
                execvp(executable_path, args);
            }
        }
        else if (pid > 0)
        {
            // Parent process
            int id = next_proc_id();
            add_job(args, num_args, is_background, pid, id);
            if (strcmp(args[num_args - 1], "&") == 0)
            {
                waitpid(pid, &status, WNOHANG);
            }
            else
            {
                cur_id = id;
                waitpid(pid, &status, WUNTRACED);
                tcsetpgrp(STDIN_FILENO, shell_pgid);
            }
        }
    }
    free(executable_path);
}

void batch_mode(char *batch_file)
{
    FILE *file = fopen(batch_file, "r");
    if (file == NULL)
    {
        perror("fopen");
        return;
    }

    char input[MAX_INPUT_SIZE];
    int num_args, is_background;

    while (fgets(input, sizeof(input), file) != NULL)
    {
        input[strcspn(input, "\n")] = '\0';
        printf("Executing: %s\n", input);

        char *args[MAX_ARGS];
        parse_input(input, args, &num_args, &is_background);

        if (num_args > 0)
        {

            execute_command(args, num_args, is_background);
        }
    }

    fclose(file);
}
void execute_pipe(char **args, int num_args, int num_cmd, int is_background)
{
    char *cmds[num_cmd][256];
    int cmd_i = 0;
    int j = 0;
    int pgid = -1;
    int pid;
    int pipefd[num_cmd - 1][2];
    for (int i = 0; i < num_args; i++)
    {
        if (strcmp(args[i], "|") != 0)
        {
            cmds[cmd_i][j] = args[i];
            j++;
        }
        else
        {
            cmds[cmd_i][j] = NULL;
            cmd_i++;
            j = 0;
        }
    }
    cmds[cmd_i][j] = NULL;
    for (int i = 0; i < num_cmd - 1; i++)
    {
        if (pipe(pipefd[i]) == -1)
        {
            exit(-1);
        }
    }

    for (int i = 0; i < num_cmd; i++)
    {
        pid = fork();
        if (pid == -1)
        {
            printf("fork fails\n");
            exit(-1);
        }
        else if (pid == 0)
        {
            if (pgid == -1)
            {
                pgid = getpid();
            }
            setpgid(getpid(), pgid);
            if (i > 0)
            {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }
            if (i < num_cmd - 1)
            {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }
            for (int k = 0; k < num_cmd - 1; k++)
            {
                close(pipefd[k][0]);
                close(pipefd[k][1]);
            }
            char p[256];
            sprintf(p, "/bin/%s", cmds[i][0]);
            execvp(p, cmds[i]);
            exit(-1);
        }
    }
    for (int i = 0; i < num_cmd - 1; i++)
    {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
    while (wait(NULL) > 1)
        ;
}

void interactive_mode()
{
    char *input = NULL;
    size_t len = 0;
    ssize_t read;

    while (1)
    {
        printf("wsh> ");
        if ((read = getline(&input, &len, stdin)) == -1)
        {
            break;
        }
        int num_cmds = 1;

        input[strcspn(input, "\n")] = '\0';
        char *args[MAX_ARGS];
        int num_args, is_background;
        parse_input(input, args, &num_args, &is_background);
        for (int i = 0; i < num_args; ++i)
        {
            if (strcmp(args[i], "|") == 0)
            {
                num_cmds++;
            }
        }

        if (num_cmds == 1)
        {
            if (num_args > 0)
            {
                remove_finished_jobs();
                execute_command(args, num_args, is_background);
            }
        }
        else
        {
            execute_pipe(args, num_args, num_cmds, is_background);
        }
    }
}

int main(int argc, char *argv[])
{
    shell_pgid = tcgetpgrp(0);

    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchild_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGTTOU, SIG_IGN);
    if (argc == 1)
    {
        interactive_mode();
    }
    else if (argc == 2)
    {
        batch_mode(argv[1]);
    }
    else
    {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

// void test_command()
// {
//     printf("\n");
//     for (int i = 0; i < MAX_ARGS && jobs[i] != NULL; ++i)
//     {
//         printf("[%d] ", jobs[i]->id);
//         printf("status %d ", jobs[i]->status);
//         for (int j = 0; jobs[i]->command_args[j] != NULL; ++j)
//         {
//             printf("%s ", jobs[i]->command_args[j]);
//         }
//         printf("(%s)\n", (jobs[i]->status == 0) ? "Running" : (jobs[i]->status == 2) ? "Stopped"
//                                                                                      : "Foreground");
//     }
// }