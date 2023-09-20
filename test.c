#include <assert.h>
#include <stdio.h>

#include "csapp.h"

#define PRINT_S(x) printf("%s: \"%s\"\n", (#x), (char *)(x))
#define TEST_IN printf("[*] begin test (%s)...\n", __func__);
#define TEST_OUT printf("[*] end test (%s)...\n", __func__);

extern char *trimwhitespace(char *str);

extern void read_requesthdrs(rio_t *rp, char *host, size_t hostlen);
static void test_read_requesthdrs() {
  TEST_IN
  rio_t rp;
  char host_result[MAXLINE];
  const char host_answer[MAXLINE] = "www.example.com";
  int stub_hdr_fd;

  stub_hdr_fd = open("stub_header.txt", O_RDONLY);
  rio_readinitb(&rp, stub_hdr_fd);
  read_requesthdrs(&rp, host_result, MAXLINE);

  PRINT_S(host_result);

  assert(strcmp(host_answer, host_result) == 0);
  TEST_OUT
}

static void test_rio_readlineb() {
  TEST_IN
  rio_t rp;
  int stub_hdr_fd, threshold = 30, cnt = 0;
  char buf[MAXLINE];

  stub_hdr_fd = open("stub_header.txt", O_RDONLY);

  rio_readinitb(&rp, stub_hdr_fd);

  while (Rio_readlineb(&rp, buf, MAXLINE) > 0 && strncmp(buf, "\r\n", 2) != 0 &&
         cnt++ < threshold) {
    printf("%s", buf);
  }
  assert(cnt < threshold);
  TEST_OUT
}

extern int split(const char *line, char *left, size_t leftlen, char *right,
                 size_t rightlen, const char delem);
static void test_split() {
  TEST_IN
  const char sample[] =
      "Accept: text/html, application/xhtml+xml, application/xml;q=0.9, "
      "image/webp, */*;q=0.8\r\n\r\n",
             key_answer[] = "Accept",
             value_answer[] =
                 "text/html, application/xhtml+xml, application/xml;q=0.9, "
                 "image/webp, */*;q=0.8";
  char key_result[MAXLINE], value_result[MAXLINE];

  split(sample, key_result, MAXLINE, value_result, MAXLINE, ':');

  PRINT_S(key_result);
  assert(strcmp(key_answer, key_result) == 0);
  PRINT_S(trimwhitespace(value_result));
  assert(strcmp(value_answer, trimwhitespace(value_result)) == 0);
  TEST_OUT
}

extern int parse_uri(const char *uri, char *host, size_t hostlen, char *path,
                     size_t pathlen);
static void test_parse_uri() {
  TEST_IN
  static const char sample[] =
      "https://choiwheatley.github.io:3000/proxylab/"
      "#client-and-server-socket-programming",
                    host_answer[] = "choiwheatley.github.io:3000",
                    path_answer[] =
                        "/proxylab/"
                        "#client-and-server-socket-programming";
  char host_result[MAXLINE], path_result[MAXLINE];

  memset(host_result, 0, MAXLINE);
  memset(path_result, 0, MAXLINE);

  assert(parse_uri(sample, host_result, MAXLINE, path_result, MAXLINE));

  PRINT_S(host_result);
  assert(strcmp(host_answer, host_result) == 0);
  PRINT_S(path_result);
  assert(strcmp(path_answer, path_result) == 0);
  TEST_OUT
}

int main(void) {
  test_split();
  test_rio_readlineb();
  test_read_requesthdrs();
  test_parse_uri();
  printf("OK\n");
}