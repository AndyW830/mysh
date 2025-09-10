#include <string.h>
#include <signal.h>

#include "builtins.h"
#include "io_helpers.h"
#include "server.h"
// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens) {
    ssize_t index = 1;

    if (tokens[index] != NULL) {
        while (tokens[index] != NULL) {
        display_message(tokens[index]);
        if(tokens[index + 1] != NULL){
            display_message(" ");
        }
        index += 1;
    }
    }
    display_message("\n");

    return 0;
}
//helper function for ls
ssize_t ls(char *path){
    DIR *fpath = opendir(path);
    if (!fpath){
        struct stat path_stat;
        if (stat(path, &path_stat) == -1) {
            display_error("ERROR: Invalid path: ", path);
            return -1;
        }
        if (S_ISREG(path_stat.st_mode)){
            display_message(path);
            display_message("\n");
            return 0;
        }
        display_error("ERROR: Invalid path:", path);
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(fpath)) != NULL){
        display_message(entry->d_name);
        display_message("\n");
    }
    closedir(fpath);
    return 0;
}

ssize_t ls_recursive(char *path, int depth) {
    if (depth < 1) {
        return 0;
    }

    DIR *fpath = opendir(path);
    if (!fpath) {
        struct stat path_stat;
        if (stat(path, &path_stat) == -1) {
            display_error("ERROR: Invalid path: ", path);
            return -1;
        }
        if (S_ISREG(path_stat.st_mode)){
            display_message(path);
            display_message("\n");
            return 0;
        }
        display_error("ERROR: Invalid path:", path);
        return -1;
    }

    struct dirent *entry;
    if (depth == 1) {
        while ((entry = readdir(fpath)) != NULL) {
            display_message(entry->d_name);
            display_message("\n");
        }
    } else {
        while ((entry = readdir(fpath)) != NULL) {
            if (entry->d_name[0] == '.'){
                display_message(entry->d_name);
                display_message("\n");
                continue;
            }

            char newpath[1280];
            snprintf(newpath, sizeof(newpath), "%s/%s", path, entry->d_name);
            display_message(entry->d_name);
            display_message("\n");
            struct stat st;
            if (stat(newpath, &st) != 0 || !S_ISDIR(st.st_mode)) {
                continue;
            }

            ls_recursive(newpath, depth - 1);
        }
    }
    closedir(fpath);
    return 0;
}

ssize_t ls_recursive_nodepth(char *path) {

    DIR *fpath = opendir(path);
    if (!fpath) {
        struct stat path_stat;
        if (stat(path, &path_stat) == -1) {
            display_error("ERROR: Invalid path: ", path);
            return -1;
        }
        if (S_ISREG(path_stat.st_mode)){
            display_message(path);
            display_message("\n");
            return 0;
        }
        display_error("ERROR: Invalid path:", path);
        return -1;
    }

    struct dirent *entry;
        while ((entry = readdir(fpath)) != NULL) {
            if (entry->d_name[0] == '.'){
                display_message(entry->d_name);
                display_message("\n");
            }

            char newpath[1280];
            snprintf(newpath, sizeof(newpath), "%s/%s", path, entry->d_name);
            display_message(entry->d_name);
            display_message("\n");
            struct stat st;
            if (stat(newpath, &st) != 0 || !S_ISDIR(st.st_mode)) {
                continue;
            }

            ls_recursive_nodepth(newpath);
        }
    closedir(fpath);
    return 0;
}

ssize_t ls_f(char *path, char *substring){
    DIR *fpath = opendir(path);
    if (!fpath){
        struct stat path_stat;
        if (stat(path, &path_stat) == -1) {
            display_error("ERROR: Invalid path: ", path);
            return -1;
        }
        if (S_ISREG(path_stat.st_mode)){
            if (strstr(path, substring)!=NULL){
                display_message(path);
                display_message("\n");
                return 0;
            }
        }
        display_error("ERROR: Invalid path:", path);
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(fpath)) != NULL){
        if (strstr(entry->d_name, substring) != NULL){
            display_message(entry->d_name);
            display_message("\n");
        }
    }
    closedir(fpath);
    return 0;
}

ssize_t ls_recursive_nodepth_f(char *path, char *substring) {

    DIR *fpath = opendir(path);
    if (!fpath) {
        struct stat path_stat;
        if (stat(path, &path_stat) == -1) {
            display_error("ERROR: Invalid path: ", path);
            return -1;
        }
        if (S_ISREG(path_stat.st_mode)){
            if (strstr(path, substring)!= NULL){
                display_message(path);
                display_message("\n");
                return 0;
            }
        }
        display_error("ERROR: Invalid path:", path);
        return -1;
    }

    struct dirent *entry;
        while ((entry = readdir(fpath)) != NULL) {
            if (entry->d_name[0] == '.'){
                continue;
                
            }

            char newpath[1280];
            snprintf(newpath, sizeof(newpath), "%s/%s", path, entry->d_name);
            if (strstr(entry->d_name, substring) != NULL){
                display_message(entry->d_name);
                display_message("\n");
            }
            struct stat st;
            if (stat(newpath, &st) != 0 || !S_ISDIR(st.st_mode)) {
                continue;
            }

            ls_recursive_nodepth_f(newpath, substring);
        }
    closedir(fpath);
    return 0;
}

ssize_t ls_recursive_f(char *path, int depth, char *substring) {
    if (depth < 1) {
        return 0;
    }

    DIR *fpath = opendir(path);
    if (!fpath) {
        struct stat path_stat;
        if (stat(path, &path_stat) == -1) {
            display_error("ERROR: Invalid path: ", path);
            return -1;
        }
        if (S_ISREG(path_stat.st_mode)){
            if (strstr(path, substring) != NULL){
                display_message(path);
                display_message("\n");
                return 0;
            }
        }
        display_error("ERROR: Invalid path:", path);
        return -1;
    }

    struct dirent *entry;
    if (depth == 1) {
        while ((entry = readdir(fpath)) != NULL) {
            if (strstr(entry->d_name, substring) != NULL){
                display_message(entry->d_name);
                display_message("\n");
            }
        }
    } else {
        while ((entry = readdir(fpath)) != NULL) {
            if (entry->d_name[0] == '.'){
                continue;
            }

            char newpath[1280];
            snprintf(newpath, sizeof(newpath), "%s/%s", path, entry->d_name);
            if (strstr(entry->d_name, substring) != NULL){
                display_message(entry->d_name);
                display_message("\n");
            }
            struct stat st;
            if (stat(newpath, &st) != 0 || !S_ISDIR(st.st_mode)) {
                continue;
            }

            ls_recursive_f(newpath, depth - 1, substring);
        }
    }
    closedir(fpath);
    return 0;
}

//main builtin function
ssize_t bn_ls(char **tokens){
    ssize_t index = 1;
    char *substring = NULL;
    int rec = 0;
    int depth = -1;
    char *filepath = NULL;
    if (tokens[index] == NULL){filepath = ".";}
    while (tokens[index] != NULL){

        if (strcmp(tokens[index], "--f") == 0){
            index++;
            if (tokens[index] == NULL){
                display_error("ERROR: Not enough arguments", "ls --f");
                return -1;
            }
            substring = tokens[index];
            if (strncmp(substring, "--", 2) == 0){
                display_error("ERROR: Invalid substring", substring);
                return -1;
            }
            if (strncmp(substring, "..", 2) == 0 || strncmp(substring, ".", 1) == 0){
                display_error("ERROR: Invalid substring", substring);
                return -1;
            }
            index++;
            continue;
        }

        if (strcmp(tokens[index], "--rec") == 0){
            rec = 1;
            index++;
            continue;
        }

        if (strcmp(tokens[index], "--d") == 0){
            index++;
            if (tokens[index] == NULL) {
                display_error("ERROR: Missing depth argument", "ls --d");
                return -1;
            }
            char *endpointer;
            depth = strtol(tokens[index], &endpointer, 10);
            if (*endpointer != '\0' || depth < 0) {
                display_error("ERROR: Invalid depth", tokens[index]);
                return -1;
            }
            index++;
            continue;
        }
        else{
            if (filepath == NULL){
                filepath = tokens[index];
                index++;
                continue;
            }
            else{
                display_error("ERROR: Too many arguments: ", "ls takes a single path");
                return -1;
            }
        }
    }
    if (filepath == NULL){
        filepath = ".";
    }

    if (depth != -1 && rec == 0){
        display_error("ERROR: Invalid depth", "ls --d");
        return -1;
    }


    if (substring != NULL && rec == 0){
        return ls_f(filepath, substring);
    }

    if(substring == NULL && rec != 0 && depth != -1){
        return ls_recursive(filepath, depth);
    }

    if(substring == NULL && rec != 0 && depth == -1){
        return ls_recursive_nodepth(filepath);
    }

    if(substring != NULL && rec != 0 && depth == -1){
        return ls_recursive_nodepth_f(filepath, substring);
    }

    if (substring != NULL && rec != 0 && depth != -1){
        return ls_recursive_f(filepath, depth, substring);
    }
    return ls(filepath);
    
}

ssize_t bn_cd(char **tokens){
    char *home = getenv("HOME");
    ssize_t index = 1;
    if (tokens[index+1] != NULL){
        display_error("ERROR: Too many arguments:", "cd takes a single path");
        return -1;
    }

    char *path = tokens[index];
    if (tokens[index] == NULL){
        path = home;
    }

    if (strcmp(path, "...") == 0) {
        path = "../..";
    } else if (strcmp(path, "....") == 0) {
        path = "../../..";
    }

    if (chdir(path) != 0) {
        display_error("ERROR: Invalid path:", path);
        return -1;
    }
    return 0;
}

ssize_t bn_cat(char **tokens){
    ssize_t index = 1;
    if (tokens[index] == NULL){
        if (!isatty(STDIN_FILENO)){
                char buffer[128];
                while (fgets(buffer, sizeof(buffer), stdin)) {
                    display_message(buffer);
            }
            return 0;
        }
        display_error("ERROR: No input source provided: ", "cat");
        return -1;
    }
    if (tokens[index+1]!=NULL){
        display_error("ERROR: Too many arguments: ", "cat takes a single file");
        return -1;
    }
    char *filename = tokens[index];
    FILE *f = fopen(filename, "r");
    char buffer[128];
    if (f == NULL){
        display_error("ERROR: Cannot open file: ", filename);
        return -1;
    }
    while (fgets(buffer, sizeof(buffer), f)){
        display_message(buffer);
    }
    fclose(f);
    return 0;
}

ssize_t bn_wc(char **tokens) {
    int std = 0; 
    char *filename = NULL;

    
    if (tokens[1] == NULL) {
        std = 1;  
    } else if (tokens[2] != NULL) {
        display_error("ERROR: Too many arguments: ", "wc takes a single file");
        return -1;
    } else {
        filename = tokens[1];  
    }

    if (!std && filename == NULL) {
        display_error("ERROR: No input source provided", "");
        return -1;
    }

    FILE *f = std ? stdin : fopen(filename, "r");
    if (f == NULL) {
        display_error("ERROR: Cannot open file: ", filename);
        return -1;
    }

    if (std && feof(f)) {
        display_error("ERROR: No input source provided", "");
        fclose(f);
        return -1;
    }

    int char_count = 0, line_count = 0, word_count = 0;
    int in_word = 0;
    char c;
    while ((c = fgetc(f)) != EOF) {
        char_count++;
        if (c == '\n') {
            line_count++;
            in_word = 0;
        } else if (c == ' ' || c == '\t') {
            in_word = 0;
        } else if (!in_word) {
            word_count++;
            in_word = 1;
        }
    }

    
    if (!std) fclose(f);

    
    printf("word count %d\n", word_count);
    printf("character count %d\n", char_count);
    printf("newline count %d\n", line_count);

    return 0;
}


ssize_t bn_kill(char **tokens) {
    if (tokens[1] == NULL) {
        display_error("ERROR: ","Missing PID\n");
        return -1;
    }

    pid_t pid = atoi(tokens[1]);
    int signum = SIGTERM; 

    if (tokens[2] != NULL) {
        signum = atoi(tokens[2]);
        if (signum <= 0 || signum > 64) {
            display_error("ERROR: ","Invalid signal specified\n");
            return -1;
        }
    }

    if (kill(pid, signum) == -1) {
        display_error("ERROR: ","The process does not exist");
    }
    return 0;
}

ssize_t bn_start_server(char **tokens){
    if (tokens[1] == NULL){
        display_error("ERROR: ","No port provided");
        return -1;
    }
    if (tokens[2] != NULL){
        display_error("ERROR: ", "Too many arguments");
        return -1;
    }
    char *endptr;
    int port = strtol(tokens[1], &endptr, 10);
    return start_server(port);
}

ssize_t bn_close_server(char **tokens){
    if (tokens[1] != NULL){
        display_error("ERROR: ", "Too many arguments");
        return -1;
    }
    else{
        return close_server();
    }
}

ssize_t bn_send(char **tokens){
    return bn_send_msg(tokens);
}

ssize_t bn_start_client(char **tokens){
    if (tokens[1] == NULL){
        display_error("ERROR: ", "No port provided");
        return -1;
    }
    if (tokens[2] == NULL){
        display_error("ERROR: ", "No hostname provided");
        return -1;
    }
    if (tokens[3] != NULL){
        display_error("ERROR: ", "Too many arguments");
        return -1;
    }
    char *endptr;
    int port = strtol(tokens[1], &endptr, 10);
    if (*endptr != '\0'){
        display_error("ERROR: ", "Invalid port number");
        return -1;
    }
    char *hostname = tokens[2];
    return start_client(port, hostname);
}