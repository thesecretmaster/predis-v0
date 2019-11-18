#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include "../command_parser.h"
#include "../predis.h"

void *connhandler(void*);

static struct main_struct *global_ms;

struct conn_info {
  struct main_struct *ms;
  int fd;
};

void parseCmd(int fd, struct main_struct *ms, char **cmd, int cmdargs) {
  struct return_val* rval = malloc(sizeof(struct return_val));
  char *output = parse_command(ms, rval, cmd, cmdargs);
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
  free(output);
}

void intHandler(int foo) {
  free_predis(global_ms);
  exit(222);
}

int main() {
  printf("Starting server from process %d\n", getpid());
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
  struct main_struct *ms = init(20000);
  global_ms = ms;
  signal(SIGINT, intHandler);
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

#define MAX_ARRAY_LEN_DIGITS 5
#define MAX_BULKSTR_LEN_DIGITS 9

// This should prolly return a status int and take a **char for it's return val
char *parseElement(int fd) {
  char type;
  char bulkstrlenbuf[MAX_BULKSTR_LEN_DIGITS + 3]; // 2 for the /r/n
  int bulkstrcharlenguess;
  long int bulkstrlen;
  char *bulkstrlenbufend;
  char *bulkstrbuf;

  if (recv(fd, &type, 1, MSG_WAITALL) <= 0) { return NULL; }
  switch (type) {
    case '$':
      // Peek ahead to find the total bulkstr length
      bulkstrcharlenguess = 1;
      memset(bulkstrlenbuf, '\0', MAX_BULKSTR_LEN_DIGITS + 3);
      bulkstrlenbuf[MAX_BULKSTR_LEN_DIGITS] = '\0';
      while (strstr(bulkstrlenbuf, "\r\n") == NULL) {
        recv(fd, &bulkstrlenbuf, bulkstrcharlenguess, MSG_PEEK);
        bulkstrcharlenguess++;
        if (bulkstrcharlenguess > MAX_BULKSTR_LEN_DIGITS) {
          printf("This is very bad. Should abort and require new connection\n");
          return NULL;
        }
      }
      bulkstrlen = strtol(bulkstrlenbuf, &bulkstrlenbufend, 10);
      // Eat everything up to the start of the actual bulkstring
      recv(fd, &bulkstrlenbuf, (bulkstrlenbufend - bulkstrlenbuf) + 2, MSG_WAITALL);
      bulkstrbuf = malloc(sizeof(char)*bulkstrlen + 1);
      bulkstrbuf[bulkstrlen] = '\0';
      recv(fd, bulkstrbuf, bulkstrlen, MSG_WAITALL);
      // Eat the trailing \r\n
      recv(fd, &bulkstrlenbuf, 2, MSG_WAITALL);
      return bulkstrbuf;
    default:
      return NULL;
  }
}

void *connhandler(void *ptr) {
  struct conn_info *obj = ptr;
  int fd = obj->fd;
  struct main_struct *ms = obj->ms;
  struct thread_info_list *ti = register_thread(ms);

  char type;
  char arraylenbuf[MAX_ARRAY_LEN_DIGITS + 3];
  char *arraylenbufend;
  arraylenbuf[MAX_ARRAY_LEN_DIGITS + 2] = '\0';
  long int arraylen;
  char **arrayelems;
  int i;

  while (recv(fd, &type, 1, MSG_WAITALL) > 0) {
    switch (type) {
      case '*':
        // Peek ahead to find the total array length
        recv(fd, &arraylenbuf, MAX_ARRAY_LEN_DIGITS, MSG_WAITALL | MSG_PEEK);
        arraylen = strtol(arraylenbuf, &arraylenbufend, 10);
        // Eat everything up to the start of the actual array
        recv(fd, &arraylenbuf, (arraylenbufend - arraylenbuf) + 2, MSG_WAITALL);
        arrayelems = malloc(sizeof(char*)*arraylen);
        for (i = 0; i < arraylen; i++) {
          arrayelems[i] = parseElement(fd);
          if (arrayelems[i] == NULL) {
            printf("Yikes! Invalid\n");
          }
        }
        parseCmd(fd, ms, arrayelems, arraylen);
        for (i = 0; i < arraylen; i++) {
          free(arrayelems[i]);
        }
        free(arrayelems);
        break;
      default:
        printf("Invalid type %c\n", type);
    }
  }

  // char sockbuf[BUFSIZE];
  // int sptr, eptr;
  // char *cmdend;
  // int recvret;
  //
  // sptr = 0;
  // eptr = 0;
  // while ((recvret = recv(fd, sockbuf + eptr, (BUFSIZE - eptr)-1, 0)) > 0) {
  //   // printf("\nRecieved %d chars to buffer index %d\n", recvret, eptr);
  //   // printf("Cheaty cheaty here it is\n%s==========\n", sockbuf + sptr);
  //   sockbuf[eptr + recvret] = '\0';
  //   eptr = eptr + recvret;
  //   // printf("Setting end pointer to %d\n", eptr);
  //
  //   // Well... so it turns out that redis commands aren't _exactly_ \r\n terminated.
  //   // Really, parseCmd should be parsing an array of bulk strings:
  //   //   *<number of args>\r\n$<charlen_arg1>\r\n<arg1>\r\n$<charlen_arg2>\r\narg2\r\n...
  //
  //   while ((cmdend = strstr(sockbuf + sptr, delimiter)) != NULL) {
  //     // printf("Parsing a command starting at index %d\n", sptr);
  //     *(cmdend) = '\0';
  //     parseCmd(fd, ms, sockbuf + sptr);
  //     // printf("Incrimenting sptr by %ld\n", (cmdend - (sockbuf + sptr)) + sizeof(delimiter));
  //     sptr = sptr + (cmdend - (sockbuf + sptr)) + 1;
  //   }
  //   // printf("Finished parsing commands. Copying %d chars starting at %d to index %d\n", eptr -sptr, sptr, 0);
  //   memcpy(sockbuf, sockbuf + sptr, eptr - sptr);
  //   eptr = eptr - sptr;
  //   sptr = 0;
  // }
  shutdown(fd, 2);
  printf("Closed %d!\n", fd);
  close(fd);
  free(ptr);
  deregister_thread(ms, ti);
  return NULL;
}
