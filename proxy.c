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
static const char *g_user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3";
static const char *g_conn_hdr = "Connection: close";
static const char *g_proxy_conn_hdr = "Proxy-Connection: close";
static const char *g_version_hdr = "HTTP/1.0";
static const char g_uri_prefixes[][15] = {"http://", "https://"};
static const char g_uri_prefix_len = 2;
static const char *g_forward_port = "80";
static char *g_listen_port = NULL;

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
/// @return 1 if uri is valid, 0 if uri is invalid
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

/// @brief split into two parts specified wit delimeter
/// @param line [in]
/// @param left [out]
/// @param right [out]
/// @param delim different with `split`, it takes string, not character
/// @return 1 if success, 0 if failure, no delimeter found
int splitstr(const char *line, char *left, size_t leftlen, char *right,
             size_t rightlen, const char *delim, size_t delimlen);

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
  g_listen_port = argv[1];
  listenfd = Open_listenfd(g_listen_port);

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

void doit(int serve_fd) {
  int client_fd;
  rio_t rio_client, rio_server;
  char buf[MAXLINE], method_str[MAXLINE], uri_str[MAXLINE],
      version_str[MAXLINE], hostval[MAXLINE], path_str[MAXLINE],
      req_port[MAXLINE];
  method_t method;

  // read request headers
  Rio_readinitb(&rio_client, serve_fd);
  Rio_readlineb(&rio_client, buf, MAXLINE);
  printf("[*] Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method_str, uri_str, version_str);

  if ((method = __get_method(method_str, strlen(method_str) + 1)) ==
      (method_t)UNKNOWN) {
    clienterror(serve_fd, method_str, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }

  // parse uri into host & path from client request
  parse_uri((const char *)uri_str, hostval, MAXLINE, path_str, MAXLINE);

  // NOTE - host may be overrided by HOST attribute!!
  read_requesthdrs(&rio_client, hostval, MAXLINE);

  char host_without_port[MAXLINE] = {0};
  if (split(hostval, host_without_port, MAXLINE, req_port, MAXLINE, ':') == 0) {
    // set to default host and port
    strcpy(host_without_port, hostval);
    strcpy(req_port, g_forward_port);
  }

  // TODO - change it to `open_clientfd`

  // connect to host server as a client
  struct addrinfo hints, *serv_addr, *itr = NULL;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICSERV;  // numeric port argument
  hints.ai_flags |= AI_ADDRCONFIG;  // recommended for connections

  {
    Getaddrinfo(host_without_port, req_port, &hints, &serv_addr);

    for (itr = serv_addr; itr; itr = itr->ai_next) {
      // iterate over server address list

      // get socket for server
      client_fd = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol);
      if (client_fd < 0) continue;  // failed to open socket, try next one

      // DO connect to the server
      if (connect(client_fd, itr->ai_addr, itr->ai_addrlen) == 0)
        break;  // connect successed

      // connect failed, try another address
      Close(client_fd);
    }

    freeaddrinfo(serv_addr);
  }

  if (itr == NULL) {
    app_error("all connects failed ☠️");
  }

  // send request to host server
  char req_buf[MAXBUF] = {0};
  sprintf(req_buf, "%s %s %s\r\n", method_str, hostval, g_version_hdr);
  sprintf(req_buf, "%sHost: %s\r\n", req_buf, hostval);
  sprintf(req_buf, "%s%s\r\n", req_buf, g_user_agent_hdr);
  sprintf(req_buf, "%s%s\r\n", req_buf, g_conn_hdr);
  sprintf(req_buf, "%s%s\r\n", req_buf, g_proxy_conn_hdr);
  sprintf(req_buf, "%s\r\n", req_buf);

  printf("[*] forwarded headers:\n%s\n", req_buf);

  Rio_writen(client_fd, req_buf, MAXLINE);

  // receive response
  memset(buf, 0, sizeof(buf));
  printf("[*] response headers:\n");

  while (Rio_readlineb(&rio_server, buf, MAXLINE) > 0 &&
         strncmp(buf, "\r\n", 2) != 0) {
    // forward headers
    printf("%s", buf);
    Rio_writen(client_fd, buf, MAXLINE);
  }

  while (Rio_readlineb(&rio_server, buf, MAXLINE) > 0) {
    // forward body to client
    Rio_writen(client_fd, buf, MAXLINE);
  }
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

int splitstr(const char *line, char *left, size_t leftlen, char *right,
             size_t rightlen, const char *delim, size_t delimlen) {
  char *pos = strstr(line, delim);
  if (pos == NULL) {
    return 0;
  }

  // do copy left
  for (const char *itr = line; itr != pos; ++itr) {
    size_t idx = itr - line;
    if (isspace(*itr)) continue;
    left[idx] = *itr;
  }

  // do copy right
  strncpy(right, pos + delimlen, rightlen);
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
              size_t pathlen) {
  char tmpbuf[MAXLINE] = {0};

  // use host as temporary buffer
  splitstr(uri, host, hostlen, tmpbuf, MAXLINE, "://", 3);

  // we only accept http protocol, not https, nor ftp, etc.
  if (strcasecmp(host, "http") != 0) {
    memset(host, 0, hostlen);
    return 0;
  }

  path[0] = '/';  // path must starts with '/'
  return split(tmpbuf, host, MAXLINE, path + 1, MAXLINE, '/');
}

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