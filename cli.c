#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include "predis.h"

int main() {
  char* line = NULL;
  struct main_struct *ms = init(200);
  struct thread_info_list *ti = register_thread(ms);
  char *cmd;
  // char *remainder;
  char *args[10];
  int idx;
  struct return_val *rval = malloc(sizeof(struct return_val));
  while (line == NULL || strcmp(line, "exit") != 0) {
    free(line);
    line = readline("predis> ");
    cmd = strtok(line, " ");
    // remainder = line+strlen(cmd)+1;
    if (strcmp(cmd, "store") == 0) {
      args[0] = strtok(NULL, " "); // type
      args[1] = strtok(NULL, " "); // value
      idx = set(strdup(args[0]), ms, args[1]);
      printf("DONE: %d\n", idx);
    } else if (strcmp(cmd, "get") == 0) {
      args[0] = strtok(NULL, " "); // type
      args[1] = strtok(NULL, " "); // index
      idx = strtol(args[1], NULL, 10);
      get(args[0], ms, rval, idx);
      printf("DONE: %s\n", rval->value);
    } else if (strcmp(cmd, "exit") == 0) {
      printf("Exiting...\n");
    } else {
      printf("Unrecognized command: %s\n", cmd);
    }
  }
  return 0;
}
