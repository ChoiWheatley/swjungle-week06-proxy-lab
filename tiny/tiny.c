/* $begin tinymain */
/*
 * # 11.6. Putting It Together: The `TINY` Web Server
 *
 * A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 */
#include <strings.h>

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

/**TODO - django-like urlpattern structure
/// @brief data parsed from rio object
typedef struct {
  const char *path; // full path described in request header, arguments include
  method_t method; // request method

} req_t;
*/

/**
 * SECTION - Function Declarations
 */

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void serve_static_malloc(int fd, char *filename, int filesize);
void serve_static_head(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/// @brief get method from request header line
/// @return method_t, an enum type
inline static method_t __get_method(char *header, size_t len);

/**TODO - django-like view structure
/// @brief path "/" that can confirm GET, HEAD method
/// @param rio_p includes request header
static void __home(rio_t *rio_p);

/// @brief path "cgi-bin/adder?<int:param1>&<int:param2>"
/// @param rio_p includes request header
static void __adder(rio_t *rio_p);
*/

/// @brief function for GET request
/// @param rio_p allocated by caller robust io object
static void __on_get(rio_t *rio_p);

/// @brief function for HEAD request
/// @param rio_p allocated by caller robust io object
static void __on_head(rio_t *rio_p);

/**
 * !SECTION - Function Declarations
 */

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

/// - cover path {'/', '/cgi-bin/adder?<int:param1>&<int:param2>/'},
/// - cover method `GET`, `HEAD`
void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  method_t req_method;

  // Read request line and headers
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  if ((req_method = __get_method(method, strlen(method) + 1)) ==
      (method_t)UNKNOWN) {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio);

  // Parse URI from GET request
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not Found",
                "Tiny couldn't find this file");
    return;
  }

  if (is_static) {
    // Serve static content
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }

    if (req_method == (method_t)GET) {
      // serve_static(fd, filename, sbuf.st_size);
      serve_static_malloc(fd, filename, sbuf.st_size);
    } else if (req_method == (method_t)HEAD) {
      serve_static_head(fd, filename, sbuf.st_size);
    }
  } else {
    // Serve dynamic content
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program 😢");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

/// @brief Tiny does not use any of the information in the request headers.
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/// @brief Tiny assumes that the home directory for static content is its
/// current directory and that the home directory for executable is `./cgi-bin`.
/// @param uri
/// @param filename regard as dynamic file as filename contains `/cgi-bin`
/// prefix
/// @param cgiargs only used with CGI program
/// @return
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {
    // Static content
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/') {
      strcat(filename, "home.html");
    }
    return 1;
  } else {
    // Dynamic content
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    } else {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // Send response headers to client
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);

  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers: \n");
  printf("%s", buf);

  // Send response body to client
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);
}

void serve_static_malloc(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // Send response headers to client
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);

  // send to client response header
  Rio_writen(fd, buf, strlen(buf));

  // just for me
  printf("Response headers: \n");
  printf("%s", buf);

  // send to client response body
  srcfd = Open(filename, O_RDONLY, 0);

  // do map with manually allocated space
  srcp = (char *)Malloc(filesize);
  Read(srcfd, (void *)srcp, (size_t)filesize);
  Rio_writen(fd, srcp, filesize);

  // free unused resources
  Close(srcfd);
  free(srcp);
}

/// @brief response header against HEAD request
void serve_static_head(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // Send response headers to client
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);

  // send to client response header
  Rio_writen(fd, buf, strlen(buf));

  // just for me
  printf("Response headers: \n");
  printf("%s", buf);

  // there are no body in this response! end function
}

/// @brief Derive file type from filename
void get_filetype(char *filename, char *filetype_out) {
  if (strstr(filename, ".html"))
    strcpy(filetype_out, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype_out, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype_out, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype_out, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype_out, "video/mp4");
  else
    strcpy(filetype_out, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = {NULL};

  // Return first part of HTTP response
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) {
    // Child Process
    // Real server would set all CGI vars here!!
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);               // Redirect stdout to client!!!
    Execve(filename, emptylist, environ);  // Run CGI program
  }
  // Parent waits for and reaps child
  Wait(NULL);
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