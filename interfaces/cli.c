#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../predis.h"
#include "../command_parser.h"

struct arg_list {
  struct arg_list *next;
  char *arg;
};

char *readline(char *prompt) {
  printf("%s", prompt);
  fflush(stdout);
  char *buff = NULL;
  size_t size = 0;
  if (getline(&buff, &size, stdin) == -1) {
    return NULL;
  }
  buff[strlen(buff)-1] = '\0';
  return buff;
}

int main() {
  char* line = NULL;
  char* output;
  struct main_struct *ms = init(200);
  register_thread(ms);
  struct return_val *rval = malloc(sizeof(struct return_val));
  rval->value = NULL;
  struct arg_list *arg_list_head;
  struct arg_list *arg_list_ptr;
  struct arg_list *arg_list_ptr_prev;
  char **args;
  int arg_list_len;
  int i;
  while (line == NULL || strcmp(line, "exit") != 0) {
    free(line);
    line = readline("predis> ");
    if (strcmp(line, "exit") == 0) {
      printf("Exiting\n");
      break;
    }
    arg_list_head = malloc(sizeof(struct arg_list));
    arg_list_head->arg = strtok(line, " ");
    arg_list_ptr = malloc(sizeof(struct arg_list));
    arg_list_head->next = arg_list_ptr;
    arg_list_head->next->next = NULL;
    arg_list_len = 1;
    while ((arg_list_ptr->arg = strtok(NULL, " ")) != NULL) {
      arg_list_ptr->next = malloc(sizeof(struct arg_list));
      arg_list_ptr->next->next = NULL;
      arg_list_ptr = arg_list_ptr->next;
      arg_list_len++;
    }
    args = malloc(sizeof(char*)*(arg_list_len + 1));
    arg_list_ptr = arg_list_head;
    i = 0;
    while (arg_list_ptr != NULL) {
      args[i] = arg_list_ptr->arg;
      arg_list_ptr_prev = arg_list_ptr;
      arg_list_ptr = arg_list_ptr->next;
      free(arg_list_ptr_prev);
      i++;
    }
    free(arg_list_ptr);
    output = parse_command(ms, rval, args, arg_list_len);
    puts(output);
    free(output);
  }
  free(rval);
  free(line);
  free_predis(ms);
  return 0;
}
