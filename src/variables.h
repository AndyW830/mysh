#ifndef __VARIABLES_H__
#define __VARIABLES_H__

#include <stddef.h>
typedef struct var{
    char *name;
    char *value;
}Var;

/* Return: 0 if setting is good and -1 if setting fails.
*/
int set_var(char *name, char *value);

/* Return: The value of this Var or NULL if it's empty.
*/
char* get_var(char *var_name);
void clean();
#endif