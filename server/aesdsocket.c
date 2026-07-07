/*
File: server/aesdsocket.c
- See Makefile for compiling instructions
*/

/*
TODO:
- getaddrinfo()
- socket()
- bind()
- listen()
- accept()
- recv()
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>

#define PORT "9000"

int main(int argc, char *argv[])
{
  // Initiate logging
  openlog("Assignment 5: Server", LOG_PID | LOG_CONS, LOG_DAEMON);

  // Allocated on stack
  struct addrinfo hints;

  // Saving the network interfaces
  struct addrinfo *res; 

  /*
   * memset(): It's a standard function in C that is used to fill a block of memory with a specific value.
   * Zeroing out the structure -> Filling every byte of that structure with literal zeroes. thus clearing the garbage values.
  */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // IPv4 (IPv6 => AF_INET6)
  hints.ai_socktype = SOCK_STREAM; // TCP (UDP => SOCK_DGRAM)
  hints.ai_flags = AI_PASSIVE; // Tell we want to bind to all network interfaces available
  
  /*
   * This function allocates memory on the heap. Hence, freeaddrinfo() is important.
   * Hostname: NULL (0.0.0.0)
   * PORT (str)
  */
  int status = getaddrinfo(NULL, PORT, &hints, &res);
  if (status != 0) {
    // gai_strerror() -> Get Address Info on String Error
    syslog(LOG_ERR, "getaddrinfo() error: %s", gai_strerror(status));
    closelog();
    return -1;
  }

  char ipstr[INET6_ADDRSTRLEN];
  for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
    void *addr;
    char *ipver;
    struct sockaddr_in *ipv4;
    struct sockaddr_in6 *ipv6;
    if (p->ai_family == AF_INET) {
      ipv4 = (struct sockaddr_in *)p->ai_addr;
      addr = &(ipv4->sin_addr);
      ipver = "IPv4";
    } else {
      ipv6 = (struct sockaddr_in6 *)p->ai_addr;
      addr = &(ipv6->sin6_addr);
      ipver = "IPv6";
    }
    
    // Convert the IP to string and print it
    inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
    printf("%s: %s\n", ipver, ipstr);
  }

  // Free the memory
  freeaddrinfo(res);

  // Close the logging tool when the program ends
  closelog();
  return EXIT_SUCCESS;
}
