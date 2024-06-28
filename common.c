#include "common.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;
  while(bytes_remaining != 0) {
    ssize_t received = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
    if(received <= 0) {
      break;
    }
    bytes_remaining -= received;
    bytes_received += received;
  }
  /*
    Returnam exact cati octeti am citit
  */
  return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;
  while(bytes_remaining) {
    ssize_t sent = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
    if(sent <= 0) {
      break;
    } 
    bytes_remaining -= sent;
    bytes_sent += sent;
  }

  /*
    Returnam exact cati octeti am trimis
  */
  return bytes_sent;
}