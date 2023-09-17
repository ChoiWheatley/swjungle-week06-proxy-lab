/**
 * CSAPP Practice Problem 11.2
 *
 * Write a program `hex2dd.c` that converts its 16-bit hex argument to a 16-bit
 * network byte order and prints the result.
 *
 * For example,
 * ```shell
 * linux> ./hex2dd 0x400
 * 1024
 * ```
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csapp.h"

int main(int argc, char const *argv[]) {
  struct in_addr inaddr;
  uint16_t addr;
  char buf[MAXBUF];

  if (argc != 2) {
    fprintf(stderr, "usage: %s <hex>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  sscanf(argv[1], "%hx", &addr);
  inaddr.s_addr = htons(addr);

  if (!inet_ntop(AF_INET, &inaddr, buf, MAXBUF)) {
    unix_error("inet_ntop error");
  }
  printf("%s\n", buf);

  return 0;
}
