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
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <stdbool.h>

#define PORT "9000"
#define BACKLOG 5
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define RECV_BUF_SIZE 1024

static volatile sig_atomic_t exit_requested = 0;

static void signal_handler(int signo) {
  (void)signo;
  exit_requested = 1;
}

static int setup_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    return -1;
  }

  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    return -1;
  }

  return 0;
}

static int daemonize_process(void) {
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }

  if (pid > 0) {
    exit(0);
  }

  // Create a new session which detaches child from the controlling terminal
  if (setsid() == -1) {
    return -1;
  }

  // Control over file permissions
  umask(0);

  if (chdir("/") == -1) {
    return -1;
  }

  // Redirecting file I/O to /dev/null preventing the terminal I/O problems
  int fd = open("/dev/null", O_RDWR);
  if (fd == -1) {
    return -1;
  }

  if (dup2(fd, STDIN_FILENO) == -1) {
    return -1;
  }
  
  if (dup2(fd, STDOUT_FILENO) == -1) {
    return -1;
  }
  
  if (dup2(fd, STDERR_FILENO) == -1) {
    return -1;
  }
  
  if (fd > STDERR_FILENO) {
    close(fd);
  }

  return 0;
}

int receive_append_packets(int client_fd) {
  char recv_buff[RECV_BUF_SIZE];
  char *packet_buff = NULL;
  size_t packet_size = 0;

  while (!exit_requested) {
    ssize_t recv_bytes = recv(client_fd, recv_buff, sizeof(recv_buff), 0);
    
    // Failure to recv()
    if (recv_bytes < 0) {
      syslog(LOG_ERR, "recv() failure: %s", strerror(errno));
      free(packet_buff);
      return -1;
    }

    // Client closing connection
    if (recv_bytes == 0) {
      syslog(LOG_INFO, "Client closed connection.");

      if (packet_size > 0) {
        int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
          syslog(LOG_ERR, "open() failure: %s", strerror(errno));
          free(packet_buff);
          return -1;
        }

        size_t total_written = 0;
        while (total_written < packet_size) {
          ssize_t written = write(fd,
                                  packet_buff + total_written,
                                  packet_size - total_written);
          if (written < 0) {
            syslog(LOG_ERR, "write() failure: %s", strerror(errno));
            close(fd);
            free(packet_buff);
            return -1;
          }
          total_written += written;
        }

        close(fd);
      }

      break;
    }

    // Allocation of buffer
    char *new_buff = realloc(packet_buff, packet_size + recv_bytes);
    if (new_buff == NULL) {
      syslog(LOG_ERR, "realloc() failure: %s", strerror(errno));
      free(packet_buff);
      return -1;
    }

    packet_buff = new_buff;
    memcpy(packet_buff + packet_size, recv_buff, recv_bytes);
    packet_size += recv_bytes;

    while (true) {
      char *newline_ptr = memchr(packet_buff, '\n', packet_size);
      if (newline_ptr == NULL) {
        syslog(LOG_INFO, "Pointing to null: newline_ptr");
        break;
      }

      size_t complete_packet_len = (newline_ptr - packet_buff) + 1;

      int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (fd < 0) {
        syslog(LOG_ERR, "open() failure: %s", strerror(errno));
        free(packet_buff);
        return -1;
      }
    
      ssize_t total_written = 0;
      while (total_written < complete_packet_len) {
        ssize_t written = write(fd, packet_buff + total_written, complete_packet_len - total_written);
        if (written < 0) {
          syslog(LOG_ERR, "write() failure: %s", strerror(errno));
          close(fd);
          free(packet_buff);
          return -1;
        }
        total_written += written;
      }

      close(fd);

      fd = open(DATA_FILE, O_RDONLY);
      if (fd < 0){
        syslog(LOG_ERR, "open() for read failure: %s", strerror(errno));
        free(packet_buff);
        return -1;
      }

      char file_buff[RECV_BUF_SIZE];
      ssize_t read_bytes;
      while ((read_bytes = read(fd, file_buff, sizeof(file_buff))) > 0) {
          size_t total_sent = 0;
          while (total_sent < (size_t)read_bytes) {
              ssize_t sent = send(client_fd,
                                  file_buff + total_sent,
                                  read_bytes - total_sent,
                                  0);
              if (sent < 0) {
                  syslog(LOG_ERR, "send() failure: %s", strerror(errno));
                  close(fd);
                  free(packet_buff);
                  return -1;
              }
              total_sent += sent;
          }
      }

      if (read_bytes < 0) {
          syslog(LOG_ERR, "read() failure: %s", strerror(errno));
          close(fd);
          free(packet_buff);
          return -1;
      }

      close(fd);

      size_t remaining = packet_size - complete_packet_len;
      if (remaining > 0) {
        memmove(packet_buff, packet_buff + complete_packet_len, remaining);
      }
      packet_size = remaining;

      if (packet_size == 0) {
        free(packet_buff);
        packet_buff = NULL;
        break;
      }

      char *shrunk = realloc(packet_buff, packet_size);
      if (shrunk != NULL) {
        packet_buff = shrunk;
      }
    }
  }

  free(packet_buff);
  return 0;
}


int main(int argc, char *argv[])
{
  // Enable signal handler
  if (setup_signal_handlers() == -1) {
    syslog(LOG_ERR, "sigaction() failed: %s", strerror(errno));
    closelog();
    return -1;
  }

  // Initiate logging
  openlog("Assignment 5: Server", LOG_PID | LOG_CONS, LOG_DAEMON);

  // Check daemon mode
  bool daemon_mode = false;

  if (argc == 2 && (strcmp(argv[1], "-d") == 0)) {
    daemon_mode = true;
  }

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
    return EXIT_FAILURE;
  }

  // Going through the returned linked list (res)
  for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
    void *addr;
    char *ipver;
    struct sockaddr_in *ipv4;
    struct sockaddr_in6 *ipv6;

    // Hold a human readable text based IP address (regardless of IPv4 or IPv6)
    char ipstr[INET6_ADDRSTRLEN];

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
  
  // Get the socket file descriptor
  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd < 0) {
    syslog(LOG_ERR, "socket() error: %s", strerror(errno));
    freeaddrinfo(res);
    closelog();
    return EXIT_FAILURE;
  }

  // Reusing address
  int reuse_address = 1;
  if (setsockopt(
        sockfd, 
        SOL_SOCKET, 
        SO_REUSEADDR, 
        &reuse_address, 
        sizeof(reuse_address)
      ) == -1) {
    syslog(LOG_ERR, "setsockopt() failed: %s", strerror(errno));
    close(sockfd);
    freeaddrinfo(res);
    closelog();
    return EXIT_FAILURE;
  }

  // Bind the port to the socket
  int bind_status = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (bind_status < 0) {
    syslog(LOG_ERR, "bind() error: %s", strerror(errno));
    close(sockfd);
    freeaddrinfo(res);
    closelog();
    return EXIT_FAILURE;
  }

  // Free the memory as res is useful only during setup
  freeaddrinfo(res);
  res = NULL;

  if (daemon_mode) {
    if (daemonize_process() == -1) {
      syslog(LOG_ERR, "daemonize_process() failed: %s", strerror(errno));
      close(sockfd);
      closelog();
      return -1;
    }
  }

  /* Listen for incoming connections
   * int listen(int sockfd, int backlog)
   * Limiting the number of connections on incoming queue (max. 20)
  */
  int lstn_status = listen(sockfd, BACKLOG);
  if (lstn_status < 0) {
    syslog(LOG_ERR, "listen() error: %s", strerror(errno));
    close(sockfd);
    closelog();
    return EXIT_FAILURE;
  }

  while (!exit_requested) {
    // Accept the incoming connections
    struct sockaddr_storage clnt_addr; // Storage for incoming connections
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    int clnt_fd = accept(sockfd, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
    if (clnt_fd < 0) {
      if (errno == EINTR && exit_requested) {
        break;
      }
      syslog(LOG_ERR, "accept() error: %s", strerror(errno));
      continue;
    }

    // Hold a human readable text based IP address (regardless of IPv4 or IPv6)
    char clnt_str[INET6_ADDRSTRLEN];
    struct sockaddr_in *ipv4_clnt = (struct sockaddr_in *)&clnt_addr;

    // Convert the IP to string and print it
    if (inet_ntop(
          AF_INET, 
          &(ipv4_clnt->sin_addr), 
          clnt_str, 
          sizeof(clnt_str)
        ) == NULL) {
      syslog(LOG_ERR, "inet_ntop() failure: %s", strerror(errno));
      // Copying the "unknown" into the string buffer in case real IP conversion fails
      strncpy(clnt_str, "unknown", sizeof(clnt_str));

      // Ending the string with null character so its a valid C string
      clnt_str[sizeof(clnt_str) - 1] = '\0';
    }
    syslog(LOG_INFO, "Accepted connection from %s", clnt_str);

    if (receive_append_packets(clnt_fd) == -1) {
      syslog(LOG_ERR, "receive_append_packets() failure: %s", strerror(errno));
    }
    syslog(LOG_INFO, "Closed connection from %s", clnt_str);

    // Close the client file descriptor
    close(clnt_fd);
  }

  syslog(LOG_INFO, "Caught signal. Exiting ...");

  // Close the socket file descriptor
  close(sockfd);

  // Deleting the data file
  unlink(DATA_FILE);

  // Close the logging tool when the program ends
  closelog();

  return EXIT_SUCCESS;
}
