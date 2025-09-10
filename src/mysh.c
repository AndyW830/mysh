#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include "server.h"


#define MAX_BG_PROCESSES 100

typedef struct {
    pid_t pid;
    char command[256];  
    int job_id;         
} bg_process;

bg_process bg_processes[MAX_BG_PROCESSES]; 
int bg_count = 0;                           


void handle_sigint(int sig) {
    (void)sig; 
    display_message("\nmysh$ "); 
    fflush(stdout);
}


void add_bg_process(pid_t pid, const char *command) {
    if (bg_count >= MAX_BG_PROCESSES) {
        display_error("ERROR: Too many background processes", "");
        return;
    }
    bg_processes[bg_count].pid = pid;
    strncpy(bg_processes[bg_count].command, command, sizeof(bg_processes[bg_count].command) - 1);
    bg_processes[bg_count].command[sizeof(bg_processes[bg_count].command) - 1] = '\0';
    bg_processes[bg_count].job_id = bg_count + 1;

    
    char message[256];
    snprintf(message, sizeof(message), "[%d] %d\n", bg_count + 1, pid);
    display_message(message);

    bg_count++;
}

void remove_bg_process(pid_t pid) {
    for (int i = 0; i < bg_count; i++) {
        if (bg_processes[i].pid == pid) {
            
            char message[512];  
            snprintf(message, sizeof(message), "[%d]+  Done %s\n", bg_processes[i].job_id, bg_processes[i].command);

            
            display_message(message);
            display_message("mysh$ ");

            
            for (int j = i; j < bg_count - 1; j++) {
                bg_processes[j] = bg_processes[j + 1];
            }
            bg_count--;
            break;
        }
    }
}

void cmd_ps() {
    for (int i = bg_count - 1; i >= 0; i--) {
        char message[512];  
        snprintf(message, sizeof(message), "%s %d\n", bg_processes[i].command, bg_processes[i].pid);
        display_message(message);
    }
}

void handle_sigchld(int sig) {
    (void)sig; 
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_bg_process(pid); 
    }
}


void handle_pipes(char **tokens, size_t token_count) {
    int num_cmds = 1;

    // Count number of commands
    for (size_t i = 0; i < token_count; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            num_cmds++;
        }
    }

    // Split commands
    char **cmds[num_cmds];
    size_t cmd_idx = 0;
    cmds[cmd_idx] = &tokens[0];

    for (size_t i = 0; i < token_count; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            tokens[i] = NULL;
            cmd_idx++;
            cmds[cmd_idx] = &tokens[i + 1];
        }
    }

    // Create pipes
    int pipes[num_cmds - 1][2];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Fork and execute commands
    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < num_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Close all pipes
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // Execute command
            bn_ptr builtin_fn = check_builtin(cmds[i][0]);
            if (builtin_fn != NULL) {
                ssize_t err = builtin_fn(cmds[i]);
                if (err == -1) {
                    display_error("ERROR: Builtin failed: ", cmds[i][0]);
                }
                exit(err == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
            } else {
                execvp(cmds[i][0], cmds[i]);
                display_error("ERROR: Unknown command: ", cmds[i][0]);
                exit(EXIT_FAILURE);
            }
        }
    }

    // Parent closes all pipes
    for (int i = 0; i < num_cmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all child processes
    for (int i = 0; i < num_cmds; i++) {
        int status;
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FAILURE) {
            
            display_error("ERROR: Command failed: ", cmds[i][0]);
        }
    }
}

void execute_command(char **token_arr, size_t token_count,
    const char *input_buf_start, const char *input_buf_end){
    // Check for pipes
    int has_pipe = 0;
    for (size_t i = 0; i < token_count; i++) {
        if (strcmp(token_arr[i], "|") == 0) {
            has_pipe = 1;
            break;
        }
    }

    if (has_pipe) {
        handle_pipes(token_arr, token_count);

        // Free memory used by pipe commands
        for (size_t i = 0; i < token_count; i++) {
            if (token_arr[i] != NULL &&
                !(token_arr[i] >= input_buf_start && token_arr[i] < input_buf_end)) {
                free(token_arr[i]);
                token_arr[i] = NULL;
            }
        }
    }
    else{
        // Handle built-in commands
        bn_ptr builtin_fn = check_builtin(token_arr[0]);
        if (builtin_fn != NULL) {
            ssize_t err = builtin_fn(token_arr);
                if (err == -1) {
                    display_error("ERROR: Builtin failed: ", token_arr[0]);
                }
                return;
        } else if (strchr(token_arr[0], '=') != NULL) {
            // Handle variable assignment
            char temp[MAX_STR_LEN];
            strncpy(temp, token_arr[0], MAX_STR_LEN);
            temp[MAX_STR_LEN - 1] = '\0';

            char *name = strtok(temp, "=");
            char *value = strtok(NULL, "");

            if (name && value) {
                int n = set_var(name, value);
                if (n == -1) {
                    display_error("Variable definition failed", token_arr[0]);
                }
            }
        } else {
            pid_t pid = fork();
            if (pid == 0) {
                execvp(token_arr[0], token_arr);
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                int status;
                wait(&status);
                if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FAILURE) {
                    
                    display_error("ERROR: Unknown command: ", token_arr[0]);
                }
            } else {
                perror("fork");
            }
        }
    }
}


void handle_background_process(char **tokens, size_t token_count,
    const char *input_buf_start, const char *input_buf_end) {
    pid_t pid = fork();
    if (pid == 0) {
        
        execute_command(tokens, token_count, input_buf_start, input_buf_end);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        
        char full_command[MAX_STR_LEN] = "";
        for (size_t i = 0; i < token_count; i++) {
            if (tokens[i] != NULL) {
                strncat(full_command, tokens[i], MAX_STR_LEN - strlen(full_command) - 1);
                strncat(full_command, " ", MAX_STR_LEN - strlen(full_command) - 1);
            }
        }
        if (strlen(full_command) > 0) {
            full_command[strlen(full_command) - 1] = '\0';
        }

        add_bg_process(pid, full_command);
    } else {
        perror("fork");
    }
}



int main(__attribute__((unused)) int argc,
         __attribute__((unused)) char* argv[]) {

    char *prompt = "mysh$ ";
    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';
    char *token_arr[MAX_STR_LEN] = {NULL};
    const char *input_buf_start = input_buf;
    const char *input_buf_end = input_buf + MAX_STR_LEN;

    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, handle_sigchld);
    while (1) {


        // Clean up memory from previous iteration
        for (size_t i = 0; i < MAX_STR_LEN; i++) {
            if (token_arr[i] != NULL &&
                !(token_arr[i] >= input_buf_start && token_arr[i] < input_buf_end)) {
                free(token_arr[i]);
                token_arr[i] = NULL;
            }
        }

        // Read input
        display_message(prompt);
        int ret = get_input(input_buf);
        size_t token_count = tokenize_input(input_buf, token_arr);

        // Variable expansion
        for (size_t i = 0; i < token_count; i++) {
            char *src = token_arr[i];
            if (src == NULL) continue;

            char expanded[MAX_STR_LEN + 1] = "";
            size_t total_len = 0;
            if (strcmp(src, "$") == 0) continue;

            while (*src && total_len < MAX_STR_LEN) {
                if (*src == '$') {
                    src++;
                    char var_name[MAX_STR_LEN] = {0};
                    char *var_ptr = var_name;

                    while (*src && *src != '$' && *src != ' ') {
                        *var_ptr++ = *src++;
                    }

                    char *var_value = get_var(var_name);
                    if (var_value) {
                        size_t remain = MAX_STR_LEN - total_len;
                        strncat(expanded, var_value, remain);
                        total_len += strnlen(var_value, remain);
                    }
                } else {
                    expanded[total_len++] = *src++;
                }
                expanded[total_len] = '\0';
            }

            // Replace token with expanded version
            if (strcmp(token_arr[i], expanded) != 0) {
                
                if (token_arr[i] >= input_buf_start && token_arr[i] < input_buf_end) {
                    token_arr[i] = strdup(expanded);
                } else {
                    free(token_arr[i]);
                    token_arr[i] = strdup(expanded);
                }
                if (token_arr[i] == NULL) {
                    display_error("Memory allocation failed", "");
                    exit(EXIT_FAILURE);
                }
            }
        }

        // Exit conditions
        if (ret == 0) break;
        if (token_count == 0) continue;
        if (strcmp(token_arr[0], "exit") == 0) {
            if (server_running){
                close_server();
            }
            break;
        }
       
        int is_background = 0;
        if (token_count > 0 && strcmp(token_arr[token_count - 1], "&") == 0) {
            is_background = 1;
            token_arr[token_count - 1] = NULL; 
            token_count--;
        }
        if (strcmp(token_arr[0], "ps") == 0){
            if (token_count == 1){
                cmd_ps();
                continue;
            }
            else{
                display_error("ERROR: ", "Too many arguments: ps");
            }
        }
        if (is_background){
            handle_background_process(token_arr, token_count, input_buf_start, input_buf_end);
        }

        else{
            execute_command(token_arr, token_count, input_buf_start, input_buf_end);
        }
    }

    // Final cleanup
    for (size_t i = 0; i < MAX_STR_LEN; i++) {
        if (token_arr[i] != NULL &&
            !(token_arr[i] >= input_buf_start && token_arr[i] < input_buf_end)) {
            free(token_arr[i]);
        }
    }
    clean();

    return 0;
}