#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "command_parser.h"
#include "cmds.c"
#include "command_parser_hashes.h"

static const char unrecognized_command[] = "Unrecognized command: %s";
static const char command_done[] = "DONE: %d";
static const char error_head[] = "ERROR";
static const char done_head[] = "DONE";

static char *print_result(int res, struct return_val *rval, bool force_error_codes) {
  int output_len = (res < 0 || rval == NULL) ? snprintf(NULL, 0, "%d", res) : strlen(rval->value);
  size_t size = (res < 0 ? sizeof(error_head) : sizeof(done_head)) + sizeof(char) * (2 /* colon space */ + output_len + 1 /* newline */) + 1 /* nul */;
  char *ret_buf = malloc(size);
  if ((force_error_codes && res < 0) || rval == NULL) {
    snprintf(ret_buf, size, "%s: %d", res < 0 ? error_head : done_head, res);
  } else {
    snprintf(ret_buf, size, "%s: %s", res < 0 ? error_head : done_head, rval->value);
  }
  return ret_buf;
}

char *parse_command(struct main_struct *ms, struct return_val *rval, char **args, int arglen) {
  char *cmd;
  // struct main_ele *idx;
  int errors;
  char* ret_buf;
  int output_len;
  cmd = args[0];
  int cmd_len = strlen(cmd);
  if (in_word_set(cmd, cmd_len)) {
    unsigned int cmd_hsh = hash(cmd, cmd_len);
    switch (cmd_hsh) {
      case SET_HSH: {
        errors = set("string", ms, args[2], args[1]);
        // errors = ht_store(ms->hashtable, args[1], idx);
        if (errors != 0) {
          ret_buf = malloc(sizeof(char)*(snprintf(NULL, 0, "HT_ERROR: %d", errors) + 1));
          sprintf(ret_buf, "HT_ERROR: %d", errors);
          del(ms, args[2]);
        } else {
          ret_buf = strdup("OK");
          // ret_buf = print_result(idx, NULL, false);
        }
        break;
      }
      case GET_HSH: {
        errors = get("string", ms, rval, args[1]);
        if (errors >= 0) {
          ret_buf = malloc(sizeof(char)*strlen(rval->value) + 1);
          strcpy(ret_buf, rval->value);
        } else {
          printf("Get error %d\n", errors);
          ret_buf = print_result(errors, rval, true);
        }
        if (errors == 1) {
          free(rval->value);
        }
        break;
      }
      case UPDATE_HSH: {
        // update <type> <updater> <index> <new value>
        // idx = strtol(args[3], NULL, 10);
        errors = update(args[1], args[2], ms, args[4], args[3]);
        ret_buf = print_result(errors, NULL, false);
        break;
      }
      case DELETE_HSH: {
        // delete <index>
        // idx = strtol(args[1], NULL, 10);
        errors = del(ms, args[1]);
        ret_buf = print_result(errors, NULL, false);
        break;
      }
      case CLEAN_HSH: {
        errors = clean_queue(ms);
        output_len = snprintf(NULL, 0, "%d", errors);
        ret_buf = malloc(sizeof(command_done)+sizeof(char)*output_len+1);
        snprintf(ret_buf, sizeof(command_done)+sizeof(char)*output_len+1, command_done, errors);
        break;
      }
      default: {
        printf("gperf: Some kind of error occured\n");
        ret_buf = malloc(sizeof(unrecognized_command)+sizeof(char)*cmd_len+1);
        snprintf(ret_buf, sizeof(unrecognized_command)+sizeof(char)*cmd_len + 1, unrecognized_command, cmd);
      }
    }
  } else {
    ret_buf = malloc(sizeof(unrecognized_command)+sizeof(char)*cmd_len+1);
    snprintf(ret_buf, sizeof(unrecognized_command)+sizeof(char)*cmd_len + 1, unrecognized_command, cmd);
  }
  return ret_buf;
}
