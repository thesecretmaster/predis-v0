#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "command_parser.h"

static const char unrecognized_command[] = "Unrecognized command: %s\n";
static const char command_done[] = "DONE: %d\n";
static const char error_head[] = "ERROR";
static const char done_head[] = "DONE";

static char *print_result(int res) {
  int output_len = snprintf(NULL, 0, "%d", res);
  size_t size = (res < 0 ? sizeof(error_head) : sizeof(done_head)) + sizeof(char) * (2 /* colon space */ + output_len + 1 /* newline */) + 1 /* nul */;
  char *ret_buf = malloc(size);
  snprintf(ret_buf, size, "%s: %d\n", res < 0 ? error_head : done_head, res);
  return ret_buf;
}

// str: The initial string or NULL
// delims: The delimiters
// ptr: An internal pointer. Initally null passed it, then used;
char *strtok_ptr(char *str, char *delims, char **ptr) {
  if (str != NULL) {
    *ptr = str;
  }
  char *rval = *ptr;
  int prefix_len = strcspn(*ptr, delims);
  *ptr = *ptr + prefix_len;
  **ptr = '\0';
  *ptr = *ptr + strspn(*ptr, delims) + 1;
  return rval;
}

char *parse_command(struct main_struct *ms, struct return_val *rval, char *line) {
  char *cmd;
  int idx;
  int errors;
  char* ret_buf;
  int output_len;
  char *iptr = NULL;
  char *args[10];
  cmd = strtok_ptr(line, " ", &iptr);
  int cmd_len = strlen(cmd);
  if (strcmp(cmd, "store") == 0) {
    args[0] = strtok_ptr(NULL, " ", &iptr); // type
    args[1] = strtok_ptr(NULL, " ", &iptr); // value
    idx = set(args[0], ms, args[1]);
    ret_buf = print_result(idx);
  } else if (strcmp(cmd, "get") == 0) {
    args[0] = strtok_ptr(NULL, " ", &iptr); // type
    args[1] = strtok_ptr(NULL, " ", &iptr); // index
    idx = strtol(args[1], NULL, 10);
    errors = get(args[0], ms, rval, idx);
    ret_buf = print_result(errors);
    if (errors == 1) {
      free(rval->value);
    }
  } else if (strcmp(cmd, "update") == 0) {
    args[0] = strtok_ptr(NULL, " ", &iptr); // type
    args[1] = strtok_ptr(NULL, " ", &iptr); // updater name
    args[2] = strtok_ptr(NULL, " ", &iptr); // idx
    args[3] = strtok_ptr(NULL, " ", &iptr); // newval
    idx = strtol(args[2], NULL, 10);
    errors = update(args[0], args[1], ms, args[3], idx);
    ret_buf = print_result(errors);
  } else if (strcmp(cmd, "delete") == 0) {
    args[0] = strtok_ptr(NULL, " ", &iptr); // idx
    idx = strtol(args[0], NULL, 10);
    errors = del(ms, idx);
    ret_buf = print_result(errors);
  } else if (strcmp(cmd, "clean") == 0) {
    errors = clean_queue(ms);
    output_len = snprintf(NULL, 0, "%d", errors);
    ret_buf = malloc(sizeof(command_done)+sizeof(char)*output_len+1);
    snprintf(ret_buf, sizeof(command_done)+sizeof(char)*output_len+1, command_done, errors);
  } else {
    ret_buf = malloc(sizeof(unrecognized_command)+sizeof(char)*cmd_len+1);
    snprintf(ret_buf, sizeof(unrecognized_command)+sizeof(char)*cmd_len + 1, unrecognized_command, cmd);
  }
  return ret_buf;
}
