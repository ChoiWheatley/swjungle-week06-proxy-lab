#include <ctype.h>
#include <stdio.h>

#include "csapp.h"

typedef enum {
  GET,
  HEAD,
  POST,
  PUT,
  DELETE,
  CONNECT,
  OPTIONS,
  TRACE,
  PATCH,
  UNKNOWN
} method_t;

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/**SECTION - Function Declarations*/
/***/

/// @brief do recv client request, send server request, recv server response,
/// send server response yadda yadda
/// @param fd connected descriptor, which can communicate with the client
void doit(int fd);

/// @brief Read request headers except the first line
/// @param rp RIO object that can read robustly
/// @param host value of the key `HOST`
void read_requesthdrs(rio_t *rp, char *host, size_t hostlen);

/// @brief Proxy server assums that uri MUST include whole valid URI of
/// endpoint.
/// @param uri [in] whole URI such as
/// `http://cmu.edu.net/foo/bar?some&additional&arguments`
/// @param host [out] caller-initialize buffer that saves host section from full
/// URI
/// @param hostlen length of `host`
/// @param path [out] caller-initialize buffer that saves except host section
/// from full URI
/// @param pathlen length of `path`
/// @return idk
int parse_uri(const char *uri, char *host, size_t hostlen, char *path,
              size_t pathlen);

void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/// @brief split into two parts specified with delimeter
/// @param line [in]
/// @param left [out]
/// @param right [out]
/// @return 1 if success, 0 if failure, no delimeter found
int split(const char *line, char *left, size_t leftlen, char *right,
          size_t rightlen, const char delim);

/// @brief get method from request header line
/// @return method_t, an enum type
inline static method_t __get_method(char *header, size_t len);

/// @brief This function returns a pointer to a substring of the original
/// string. If the given string was allocated dynamically, the caller must not
/// overwrite that pointer with the returned value, since the original pointer
/// must be deallocated using the same allocator with which it was allocated.
/// The return value must NOT be deallocated using free() etc.
/// @note https://stackoverflow.com/a/122721/21369350
char *trimwhitespace(char *str);
/***/
/**!SECTION - Function Declarations*/

#ifndef LIB
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // automatically call socket(2), bind(2), listen(2)
  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(clientaddr);
    // TODO - Part II. Dealing with multiple concurrent requests
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("[*] Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
  }

  return 0;
}
#endif  // LIB

void doit(int fd) {
  rio_t rio;
  char buf[MAXLINE], method_str[MAXLINE], uri_str[MAXLINE],
      version_str[MAXLINE], hostval[MAXLINE];
  method_t method;

  // read request headers
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("[*] Request headers: \n");
  printf("%s", buf);
  sscanf("%s %s %s", method_str, uri_str, version_str);

  if ((method = __get_method(method_str, strlen(method_str) + 1)) ==
      (method_t)UNKNOWN) {
    clienterror(fd, method_str, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio, hostval, MAXLINE);
}

/// @brief split into two parts specified with delimeter
/// @param line [in]
/// @param left [out]
/// @param right [out]
/// @return 1 if success, 0 if failure, no delimeter found
int split(const char *line, char *left, size_t leftlen, char *right,
          size_t rightlen, const char delim) {
  char *pos = strchr(line, (int)delim);
  if (pos == NULL) {
    return 0;
  }

  // do copy left
  for (const char *itr = line; (itr - line) < leftlen && *itr != delim; ++itr) {
    size_t idx = itr - line;
    if (isspace(*itr)) continue;
    left[idx] = *itr;
  }

  // do copy right
  strncpy(right, pos + 1, rightlen);
  return 1;
}

void read_requesthdrs(rio_t *rp, char *host, size_t hostlen) {
  char buf[MAXLINE], key[MAXLINE], value[MAXLINE];

  while (Rio_readlineb(rp, buf, MAXLINE) > 0 && strcmp(buf, "\r\n") != 0) {
    printf("%s", buf);

    split((const char *)buf, key, MAXLINE, value, MAXLINE, ':');

    if (strncasecmp(key, "HOST", 4) == 0) {
      // if header HOST found, copy value into `host`

      strncpy(host, trimwhitespace(value), MIN(hostlen, MAXLINE));
    }
  }
}

int parse_uri(const char *uri, char *host, size_t hostlen, char *path,
              size_t pathlen) {}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  // Build the HTTP response body
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body,
          "%s<body bgcolor="
          "ff1111"
          ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // Print the HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-Type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-Length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

inline method_t __get_method(char *header, size_t len) {
  // read first bytes and check
  if (strncmp(header, "GET", 3) == 0) {
    return GET;
  } else if (strncmp(header, "HEAD", 4) == 0) {
    return HEAD;
  } else if (strncmp(header, "POST", 4) == 0) {
    return POST;
  } else if (strncmp(header, "PUT", 3) == 0) {
    return PUT;
  } else if (strncmp(header, "DELETE", 6) == 0) {
    return DELETE;
  } else if (strncmp(header, "CONNECT", 7) == 0) {
    return CONNECT;
  } else if (strncmp(header, "OPTIONS", 7) == 0) {
    return OPTIONS;
  } else if (strncmp(header, "TRACE", 5) == 0) {
    return TRACE;
  } else if (strncmp(header, "PATCH", 5) == 0) {
    return PATCH;
  }
  return UNKNOWN;
}

char *trimwhitespace(char *str) {
  char *end;

  // Trim leading space
  while (isspace((unsigned char)*str)) str++;

  if (*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}