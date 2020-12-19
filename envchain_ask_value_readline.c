#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <readline/readline.h>

#include "envchain.h"

static char*
envchain_noecho_read(char* prompt)
{
  struct termios term, term_orig;
  char* str = NULL;
  ssize_t len;
  size_t n;

  if (tcgetattr(STDIN_FILENO, &term) < 0) {
    if (errno == ENOTTY) {
      fprintf(stderr, "--noecho (-n) requires stdin to be a terminal\n");
    }
    else {
      fprintf(stderr, "oops when attempted to read: %s\n", strerror(errno));
    }
    return NULL;
  }

  term_orig = term;
  term.c_lflag &= ~ECHO;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) < 0) {
    fprintf(stderr, "tcsetattr failed\n");
    exit(10);
  }

  printf("%s (noecho):", prompt);
  len = getline(&str, &n, stdin);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_orig) < 0) {
    fprintf(stderr, "tcsetattr restore failed\n");
    exit(10);
  }

  if (0 < len && str[len-1] == '\n')
    str[len - 1] = '\0';

  printf("\n");

  return str;
}

char*
envchain_ask_value(const char* name, const char* key, int noecho)
{
  char *prompt, *line;
  asprintf(&prompt, "%s.%s", name, key);

  if (noecho) {
    line = envchain_noecho_read(prompt);
  }
  else {
    printf("%s", prompt);
    line = readline(": ");
  }

  free(prompt);
  return line;
}
