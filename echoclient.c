/**
 * # 11.4.9. Example Echo Client and Server
 *
 * ## Figure 11.20: Echo client main routine
 *
 * 서버와 연결(`Open_clentfd`)을 맺은 뒤 클라이언트는 루프를 돌면서
 * stdin으로부터 text line을 입력받고(`Fgets`) 서버에게 전송한다(`Rio_writen`).
 * 서버로부터 응답을 수신하고(`Rio_readlineb`) stdout으로 에코 결과를 출력한다
 * (`Fputs`)
 */
#include "csapp.h"

int main(int argc, char **argv) {
  int clientfd;
  char *host, *port, buf[MAXLINE];
  rio_t rio;

  if (argc != 3) {
    fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
    exit(0);
  }

  host = argv[1];
  port = argv[2];

  /**
   * Initialize rio buffer.
   */
  clientfd = Open_clientfd(host, port);
  Rio_readinitb(&rio, clientfd);

  while (Fgets(buf, MAXLINE, stdin) != NULL) {
    Rio_writen(clientfd, buf, strlen(buf));
    Rio_readlineb(&rio, buf, MAXLINE);
    Fputs(buf, stdout);
  }

  // when client hit `Ctrl + D` and break while loop
  Close(clientfd);
  exit(0);
}