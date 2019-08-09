#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "predis.h"
#include "command_parser.h"

char *readline(char *prompt) {
  printf("%s", prompt);
  fflush(stdout);
  char *buff = NULL;
  size_t size = 0;
  getline(&buff, &size, stdin);
  buff[strlen(buff)-1] = '\0';
  return buff;
}

int main() {
  char* line = NULL;
  char* output;
  struct main_struct *ms = init(200);
  struct thread_info_list *ti = register_thread(ms);
  struct return_val *rval = malloc(sizeof(struct return_val));
  while (line == NULL || strcmp(line, "exit") != 0) {
    free(line);
    line = readline("predis> ");
    if (strcmp(line, "exit") == 0) {
      printf("Exiting\n");
      break;
    }
    output = parse_command(ms, rval, line);
    puts(output);
    free(output);
  }
  free(rval);
  free(line);
  free_predis(ms);
  return 0;
}
