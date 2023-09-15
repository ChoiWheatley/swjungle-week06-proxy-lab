/*
 * 11.5.4 Serving Dynamic Content
 *
 * a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  // Extract the two arguments
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);    // arg1 = buf[0:p]
    strcpy(arg2, p + 1);  // arg2 = buf[p + 1:]
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  // make the response body
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcom to add.com: ");
  sprintf(content, "%sThe Internet additional portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p?", content, n1, n2,
          n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  // generate the HTTP response
  // we can achieve send response very easily via descriptor redirection `dup2`,
  // which redirects stdout into socket descriptor!!
  printf("Connection: close\r\n");
  printf("Content-Length: %d\r\n", (int)strlen(content));
  printf("Content-Type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
