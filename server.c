#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include "command_parser.h"
#include "predis.h"

#define BUFSIZE 1024

void *connhandler(void*);

struct conn_info {
  struct main_struct *ms;
  int fd;
};

void parseCmd(int fd, struct main_struct *ms, char *cmd) {
  printf("CMD: %s\n", cmd);
  struct return_val* rval = malloc(sizeof(struct return_val));
  char *output = parse_command(ms, rval, cmd);
  free(rval);
  char *prefix = "+";
  char *postfix = "\r\n";
  char *buf = malloc(sizeof(char)*(strlen(prefix) + strlen(postfix) + strlen(output)) + 1);
  strcpy(buf, prefix);
  strcpy(buf + strlen(prefix), output);
  strcpy(buf + strlen(prefix) + strlen(output), postfix);
  buf[strlen(prefix) + strlen(postfix) + strlen(output)] = '\0';
  send(fd, buf, strlen(buf), 0);
  free(buf);
}

static const char delimiter[] = "\r\n";

int main() {
  struct addrinfo *addrinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
  if (getaddrinfo("0.0.0.0", "8080", &hints, &addrinfo) != 0) {
    printf("bad\n");
    return 1;
  }
  // AF_INET/AF_UNIX/AF_INET6 all work
  int socket_fd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
  if (socket_fd < 0) {
    printf("pad3\n");
    return 3;
  }
  int tru = 1;
  setsockopt(socket_fd, SOL_SOCKET ,SO_REUSEADDR, &tru, sizeof(int));
  if (bind(socket_fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
    printf("bad2\n");
    return 2;
  }
  if (listen(socket_fd, 10) != 0) {
    printf("bad4\n");
    return 4;
  }
  int client_sock;
  struct sockaddr_storage their_addr;
  socklen_t addr_size = sizeof(their_addr);
  pthread_t pid;
  struct main_struct *ms = init(200);
  while (1) {
    client_sock = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);
    printf("Accepted a conn %d!\n", client_sock);
    struct conn_info *obj = malloc(sizeof(struct conn_info));
    obj->fd = client_sock;
    obj->ms = ms;
    pthread_create(&pid, NULL, connhandler, obj);
  }
  return 0;
}

void *connhandler(void *ptr) {
  struct conn_info *obj = ptr;
  int fd = obj->fd;
  struct main_struct *ms = obj->ms;
  struct thread_info_list *ti = register_thread(ms);

  char sockbuf[BUFSIZE];
  int sptr, eptr;
  char *cmdend;
  int recvret;

  sptr = 0;
  eptr = 0;
  while ((recvret = recv(fd, sockbuf + eptr, (BUFSIZE - eptr)-1, 0)) > 0) {
    // printf("\nRecieved %d chars to buffer index %d\n", recvret, eptr);
    // printf("Cheaty cheaty here it is\n%s==========\n", sockbuf + sptr);
    sockbuf[eptr + recvret] = '\0';
    eptr = eptr + recvret;
    // printf("Setting end pointer to %d\n", eptr);
    while ((cmdend = strstr(sockbuf + sptr, delimiter)) != NULL) {
      // printf("Parsing a command starting at index %d\n", sptr);
      *(cmdend) = '\0';
      parseCmd(fd, ms, sockbuf + sptr);
      // printf("Incrimenting sptr by %ld\n", (cmdend - (sockbuf + sptr)) + sizeof(delimiter));
      sptr = sptr + (cmdend - (sockbuf + sptr)) + 1;
    }
    // printf("Finished parsing commands. Copying %d chars starting at %d to index %d\n", eptr -sptr, sptr, 0);
    memcpy(sockbuf, sockbuf + sptr, eptr - sptr);
    eptr = eptr - sptr;
    sptr = 0;
  }
  shutdown(fd, 2);
  printf("Closed %d!\n", fd);
  close(fd);
  free(ptr);
  deregister_thread(ti);
  return NULL;
}
