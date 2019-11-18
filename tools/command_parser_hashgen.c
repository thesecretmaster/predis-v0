#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "cmds.c"

char *strupr(char *str) {
  int l = strlen(str);
  for (int i = 0; i < l; i++) {
    str[i] = toupper(str[i]);
  }
  return str;
}

int main(int argc, char *argv[]) {
  FILE *s = fopen("cmds.txt", "r");
  char *line = NULL;
  size_t len = 0;
  int slen = 0;
  // size_t llen;
  // char *buf;
  unsigned int hsh;
  // puts("#include \"cmds.c\"\n");
  while ((slen = getline(&line, &len, s)) != -1) {
    // printf("Line: %s\n", line);
    hsh = hash(line, slen - 1);
    // https://stackoverflow.com/questions/14069737/switch-case-error-case-label-does-not-reduce-to-an-integer-constant
    // printf("static const unsigned int %.*s_hsh = %d;\n", slen-1, line, hsh);
    printf("#define %.*s_HSH %d\n", slen - 1, strupr(line), hsh);
    free(line);
    line = NULL;
    len = 0;
    // buf = malloc(sizeof(char)*(llen + 1));
    // snprintf(buf, llen, "static const int %s_hsh = %d;", line, hsh);
    // puts("%s");
  }
  fclose(s);
  return 0;
}
