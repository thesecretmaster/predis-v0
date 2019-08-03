#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "predis.h"

char *readline(char *prompt) {
  printf("%s", prompt);
  fflush(stdout);
  char *buff = NULL;
  size_t size = 0;
  getline(&buff, &size, stdin);
  buff[strlen(buff)-1] = '\0';
  return buff;
}

static void print_result(int res) {
  printf("%s: %d\n", res < 0 ? "ERROR" : "DONE", res);
}

int main() {
  char* line = NULL;
  struct main_struct *ms = init(200);
  struct thread_info_list *ti = register_thread(ms);
  char *cmd;
  // char *remainder;
  char *args[10];
  int idx;
  int errors;
  struct return_val *rval = malloc(sizeof(struct return_val));
  while (line == NULL || strcmp(line, "exit") != 0) {
    free(line);
    line = readline("predis> ");
    cmd = strtok(line, " ");
    // remainder = line+strlen(cmd)+1;
    if (strcmp(cmd, "store") == 0) {
      args[0] = strtok(NULL, " "); // type
      args[1] = strtok(NULL, " "); // value
      idx = set(args[0], ms, args[1]);
      print_result(idx);
    } else if (strcmp(cmd, "get") == 0) {
      args[0] = strtok(NULL, " "); // type
      args[1] = strtok(NULL, " "); // index
      idx = strtol(args[1], NULL, 10);
      errors = get(args[0], ms, rval, idx);
      if (errors < 0) {
        printf("ERROR: %d\n", errors);
      } else {
        printf("VALUE: %s\n", rval->value);
        if (errors == 1) {
          free(rval->value);
        }
      }
    } else if (strcmp(cmd, "update") == 0) {
      args[0] = strtok(NULL, " "); // type
      args[1] = strtok(NULL, " "); // updater name
      args[2] = strtok(NULL, " "); // idx
      args[3] = strtok(NULL, " "); // newval
      idx = strtol(args[2], NULL, 10);
      errors = update(args[0], args[1], ms, args[3], idx);
      print_result(errors);
    } else if (strcmp(cmd, "delete") == 0) {
      args[0] = strtok(NULL, " "); // idx
      idx = strtol(args[0], NULL, 10);
      errors = del(ms, idx);
      print_result(errors);
    } else if (strcmp(cmd, "clean") == 0) {
      errors = clean_queue(ms);
      printf("DONE: %d\n", errors);
    } else if (strcmp(cmd, "exit") == 0) {
      printf("Exiting...\n");
    } else {
      printf("Unrecognized command: %s\n", cmd);
    }
  }
  free(rval);
  free(line);
  free_predis(ms);
  return 0;
}
