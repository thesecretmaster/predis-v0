#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "command_parser.h"
#include "cmds.c"

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

static bool hshs_are_set = false;

static unsigned int iget_hsh = 0;
static unsigned int iset_hsh = 0;
static unsigned int set_hsh = 0;
static unsigned int get_hsh = 0;
static unsigned int update_hsh = 0;
static unsigned int delete_hsh = 0;
static unsigned int clean_hsh = 0;

static void calculate_command_hashes() {
  if (!hshs_are_set) {
    iset_hsh = hash("iset", sizeof("iset") - 1);
    iget_hsh = hash("iget", sizeof("iget") - 1);
    set_hsh = hash("set", sizeof("set") - 1);
    get_hsh = hash("get", sizeof("get") - 1);
    update_hsh = hash("update", sizeof("update") - 1);
    delete_hsh = hash("delete", sizeof("delete") - 1);
    clean_hsh = hash("clean", sizeof("clean") - 1);
    hshs_are_set = true;
  }
}

char *parse_command(struct main_struct *ms, struct return_val *rval, char **args, int arglen) {
  calculate_command_hashes();
  char *cmd;
  int idx;
  int errors;
  char* ret_buf;
  int output_len;
  cmd = args[0];
  int cmd_len = strlen(cmd);
  if (in_word_set(cmd, cmd_len)) {
    unsigned int cmd_hsh = hash(cmd, cmd_len);
    if (cmd_hsh == iset_hsh) {
      // store <type> <value>
      idx = set(args[1], ms, args[2]);
      ret_buf = print_result(idx, NULL, false);
    } else if (cmd_hsh == iget_hsh) {
      // get <type> <index>
      idx = strtol(args[2], NULL, 10);
      errors = get(args[1], ms, rval, idx);
      ret_buf = print_result(errors, rval, true);
      if (errors == 1) {
        free(rval->value);
      }
    } else if (cmd_hsh == set_hsh) {
      idx = set("string", ms, args[2]);
      errors = ht_store(ms->hashtable, args[1], idx);
      if (errors != 0) {
        ret_buf = malloc(sizeof(char)*(snprintf(NULL, 0, "HT_ERROR: %d", errors) + 1));
        sprintf(ret_buf, "HT_ERROR: %d", errors);
        del(ms, idx);
      } else {
        ret_buf = strdup("OK");
        // ret_buf = print_result(idx, NULL, false);
      }
    } else if (cmd_hsh == get_hsh) {
      idx = ht_find(ms->hashtable, args[1]);
      errors = get("string", ms, rval, idx);
      if (errors >= 0) {
        ret_buf = malloc(sizeof(char)*strlen(rval->value) + 1);
        strcpy(ret_buf, rval->value);
      } else {
        ret_buf = print_result(errors, rval, false);
      }
      if (errors == 1) {
        free(rval->value);
      }
    } else if (cmd_hsh == update_hsh) {
      // update <type> <updater> <index> <new value>
      idx = strtol(args[3], NULL, 10);
      errors = update(args[1], args[2], ms, args[4], idx);
      ret_buf = print_result(errors, NULL, false);
    } else if (cmd_hsh == delete_hsh) {
      // delete <index>
      idx = strtol(args[1], NULL, 10);
      errors = del(ms, idx);
      ret_buf = print_result(errors, NULL, false);
    } else if (cmd_hsh == clean_hsh) {
      errors = clean_queue(ms);
      output_len = snprintf(NULL, 0, "%d", errors);
      ret_buf = malloc(sizeof(command_done)+sizeof(char)*output_len+1);
      snprintf(ret_buf, sizeof(command_done)+sizeof(char)*output_len+1, command_done, errors);
    } else {
      printf("gperf: Some kind of error occured\n");
      ret_buf = malloc(sizeof(unrecognized_command)+sizeof(char)*cmd_len+1);
      snprintf(ret_buf, sizeof(unrecognized_command)+sizeof(char)*cmd_len + 1, unrecognized_command, cmd);
    }
  } else {
    ret_buf = malloc(sizeof(unrecognized_command)+sizeof(char)*cmd_len+1);
    snprintf(ret_buf, sizeof(unrecognized_command)+sizeof(char)*cmd_len + 1, unrecognized_command, cmd);
  }
  return ret_buf;
}
