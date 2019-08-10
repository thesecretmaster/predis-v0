#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "predis.h"

char *parse_command(struct main_struct*, struct return_val*, char**, int);
char *strtok_ptr(char*, char*, char**);

#endif // COMMAND_PARSER_H
