#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "variables.h"
#include "io_helpers.h"
Var *var_list = NULL;
int var_count = 0;
int array_size = 0;

int set_var(char *name, char *value) {
    if (name == NULL || value == NULL) return -1;

    // Create the list
    if (var_list == NULL) {
        array_size = 10;
        var_list = malloc(array_size * sizeof(Var));
        if (var_list == NULL) return -1;
        for (int i = 0; i < array_size; i++) {
            var_list[i].name = NULL;
            var_list[i].value = NULL;
        }
    } else if (var_count >= array_size) { //If memory is not enough, double it
        int new_size = array_size * 2;
        Var *new_list = realloc(var_list, new_size * sizeof(Var));
        if (new_list == NULL) return -1;
        var_list = new_list;
        for (int i = array_size; i < new_size; i++) {
            var_list[i].name = NULL;
            var_list[i].value = NULL;
        }
        array_size = new_size;
    }

    // Check if the variable is already defined
    for (int i = 0; i < var_count; i++) {
        if (var_list[i].name != NULL && strcmp(var_list[i].name, name) == 0) {
            char *new_value = strdup(value);
            if (new_value == NULL) return -1;
            free(var_list[i].value);
            var_list[i].value = new_value;
            return 0;
        }
    }

    // Add new variables
    char *new_name = strdup(name);
    char *new_value = strdup(value);
    var_list[var_count].name = new_name;
    var_list[var_count].value = new_value;
    var_count++;
    return 0;
}

char* get_var(char *var_name) {
    if (var_name == NULL || var_list == NULL) return "";
    for (int i = 0; i < var_count; i++) {
        if (var_list[i].name != NULL && strcmp(var_list[i].name, var_name) == 0) {
            return var_list[i].value;
        }
    }
    return "";
}

void clean() {
    if (var_list == NULL) return;
    for (int i = 0; i < var_count; i++) {
        free(var_list[i].name);
        free(var_list[i].value);
    }
    free(var_list);
    var_list = NULL;
    var_count = 0;
    array_size = 0;
}