/**
 * CSAPP Practice Problem 11.3
 *
 * Write a program `dd2hex.c` that converts its 16-bit network byte order to a
 * 16-bit hex number and prints the result.
 *
 * For example,
 * ```shell
 * linux> ./dd2hex 1024
 * 0x400
 * ```
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csapp.h"

int main(int argc, char const *argv[]) {
  uint16_t addr;
  struct in_addr inaddr;
  char buf[MAXBUF];

  if (argc != 2) {
    fprintf(stderr, "usage: %s <dec>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  sscanf(argv[1], "%hd", &addr);
  inaddr.s_addr = ntohs(addr);
  if (!inet_ntop(AF_INET, &inaddr, buf, MAXBUF)) {
    unix_error("inet_ntop error");
    exit(EXIT_FAILURE);
  }
  printf("0x%x (%s)\n", inaddr.s_addr, buf);

  return 0;
}
