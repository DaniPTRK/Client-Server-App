#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

void run_client(int sockfd)
{
  // Porneste clientul.
  char buf[MSG_MAXSIZE + 1];
  memset(buf, 0, MSG_MAXSIZE + 1);

  struct chat_packet sent_packet;
  struct chat_packet recv_packet;

  struct pollfd poll_fds[2];
  int rc;

  poll_fds[0].fd = sockfd;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = STDIN_FILENO;
  poll_fds[1].events = POLLIN;

  while (1)
  {
    rc = poll(poll_fds, 2, -1);
    DIE(rc < 0, "poll");

    if (poll_fds[0].revents & POLLIN)
    {
      // Primim un mesaj de la server.
      rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
      if (rc <= 0)
      {
        break;
      }

      // Daca mesajul este exit, inchide sesiunea curenta.
      if (strcmp(recv_packet.message, "exit\n") == 0)
      {
        rc = send_all(sockfd, &recv_packet, sizeof(recv_packet));
        DIE(rc < 0, "[Subs] Couldn't send data.\n");
        exit(0);
      }
      else
      {
        /* Am primit mesaj de la unul dintre topicurile la care suntem abonati
        sau am primit confirmare de abonare/dezabonare. */
        printf("%s", recv_packet.message);
      }
    }

    if (poll_fds[1].revents & POLLIN)
    {
      // Trimite un mesaj catre server.
      if (!fgets(buf, sizeof(buf), stdin) || isspace(buf[0]))
      {
        break;
      }
      sent_packet.len = strlen(buf) + 1;
      strcpy(sent_packet.message, buf);
      char *command = strtok(buf, " ");

      if (strcmp(sent_packet.message, "exit\n") == 0)
      {
        /* Pentru exit, inchide sesiunea curenta si trimite 
        un mesaj serverului. */
        rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
        DIE(rc < 0, "[Subs] Couldn't send data.\n");
        shutdown(sockfd, SHUT_RDWR);
        return;
      }
      else if ((strcmp(command, "subscribe") == 0) || (strcmp(command, "unsubscribe") == 0))
      {
        // Pentru subscribe/unsubscribe, trimite comanda si topicul.
        int OK = 0;
        char topicaux[51];
        if (strcmp(command, "subscribe") == 0)
        {
          OK = 1;
        }
        command = strtok(NULL, " ");
        strcpy(topicaux, command);
        command = strtok(NULL, " ");

        // Verifica daca doar topicul a fost inserat.
        if (command == NULL)
        {
          // Trimite mesajul catre server.
          topicaux[strlen(topicaux) - 1] = 0;
          if (OK == 1)
          {
            strcpy(sent_packet.message, "subscribe ");
          }
          else
          {
            strcpy(sent_packet.message, "unsubscribe ");
          }

          strcat(sent_packet.message, topicaux);
          sent_packet.len = strlen(sent_packet.message);
          rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
          DIE(rc < 0, "[Subs] Couldn't send subscribe information.\n");
        }
      }
    }
  }
}

int main(int argc, char *argv[])
{
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  if (argc != 4)
  {
    printf("\n Usage: %s <id_subscriber> <ip> <port>\n", argv[0]);
    return 1;
  }

  // Parsam port-ul ca un numar.
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP pentru conectarea la server.
  const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  struct chat_packet send_packet;
  memcpy(send_packet.message, argv[1], strlen(argv[1]) + 1);
  send_packet.len = strlen(send_packet.message);

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare.
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // Ne conectăm la server.
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  // Trimitem ID-ul catre server.
  rc = send_all(sockfd, &send_packet, sizeof(send_packet));
  DIE(rc < 0, "send");

  run_client(sockfd);

  // Inchidem conexiunea si socketul creat.
  close(sockfd);

  return 0;
}
