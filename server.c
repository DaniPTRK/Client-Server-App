#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <math.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 20
#define ALLOC_SIZE 30

/* Structura pentru un mesaj UDP */
typedef struct UDP_message
{
  char topic[50];
  uint8_t type;
  char content[1501];
} UDP_message;

/* Structura unui mesaj TCP */
typedef struct TCP_message {
  char ip[30], topic[51], type[20];
  int port;
  char content[1501];
}TCP_message;

/* Structura specifica unui client */
typedef struct client
{
  int status, socket, ntopics;
  int max_topics;
  char id[10];
  char **topics;
} client;

/* Returneaza 0 daca topicurile sunt matching, 1 in caz contrar. */
int check_wildcards(char *client, char *received)
{
  /* Imparte in tokeni cele 2 topicuri si verifica
  daca exista wildcard-uri. */
  char *save_ptr1, *save_ptr2;
  char *client_tokens = strtok_r(client, "/", &save_ptr1);
  char *received_tokens = strtok_r(received, "/", &save_ptr2);

  while (client_tokens != NULL && received_tokens != NULL)
  {
    /* Daca avem wildcard + sau egalitate intre tokeni, mergem la
    urmatorul token. */
    if ((strcmp(client_tokens, received_tokens) == 0)
          || (strcmp(client_tokens, "+") == 0))
    {
      client_tokens = strtok_r(NULL, "/", &save_ptr1);
      received_tokens = strtok_r(NULL, "/", &save_ptr2);
    }
    else if (strcmp(client_tokens, "*") == 0)
    {
      /* Pentru *, trece prin toate token-urile pana cand
      gaseste un token egal cu cel de dupa *. */
      client_tokens = strtok_r(NULL, "/", &save_ptr1);
      if (client_tokens == NULL)
      {
        // Daca am doar * la final, topicurile sunt matching.
        return 0;
      }
      else
      {
        // Treci prin token-uri.
        while ((received_tokens != NULL) &&
                (strcmp(client_tokens, received_tokens) != 0))
        {
          received_tokens = strtok_r(NULL, "/", &save_ptr2);
        }

        if (received_tokens == NULL)
        {
          // Daca n-am gasit, topicurile nu sunt matching.
          return 1;
        }
      }
    }
    else
    {
      // Nu avem matching.
      return 1;
    }
  }

  if (client_tokens == NULL && received_tokens == NULL)
  {
    return 0;
  }
  return 1;
}

void run_server(int tcpfd, int udpfd)
{
  // Porneste serverul.
  struct pollfd *poll_fds;
  int num_sockets = 3;
  int rc;

  struct sockaddr_in cli_addr;
  socklen_t cli_len = sizeof(cli_addr);

  client *clients;

  int curr_max_poll = ALLOC_SIZE;
  int curr_max_clients = ALLOC_SIZE;
  int num_clients = 0;

  poll_fds = malloc(curr_max_poll * sizeof(struct pollfd));
  DIE(poll_fds == NULL, "[Server] Malloc polls failed.");
  clients = malloc(curr_max_clients * sizeof(client));
  DIE(clients == NULL, "[Server] Malloc clients failed.");

  struct chat_packet received_packet;
  memset(received_packet.message, 0, BUFSIZ);
  received_packet.len = 0;

  // Setam socket-ul TCP pentru ascultare.
  rc = listen(tcpfd, MAX_CONNECTIONS);
  DIE(rc < 0, "[Server] TCP failed.");

  // Adaugam noii file descriptori in poll_fds.
  poll_fds[0].fd = STDIN_FILENO;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = tcpfd;
  poll_fds[1].events = POLLIN;

  poll_fds[2].fd = udpfd;
  poll_fds[2].events = POLLIN;

  while (1)
  {
    // Asteptam sa primim ceva pe unul dintre cei num_sockets sockets.
    rc = poll(poll_fds, num_sockets, -1);
    DIE(rc < 0, "[Server] Poll failed.");

    for (int i = 0; i < num_sockets; i++)
    {
      if (poll_fds[i].revents & POLLIN)
      {
        if (poll_fds[i].fd == tcpfd)
        {
          /* Am primit o cerere de conexiune pe socketul TCP, pe care
           o acceptam.
            Adaugam noul socket intors de accept() la multimea descriptorilor
           de citire. */
          int j;
          const int newsockfd = accept(tcpfd, (struct sockaddr *)&cli_addr, 
                                        &cli_len);
          DIE(newsockfd < 0, "[Server] Accept failed.");

          // Verificam daca trebuie alocata mai multa memorie pentru socketi.
          if (num_sockets == curr_max_poll)
          {
            curr_max_poll += ALLOC_SIZE;
            poll_fds = realloc(poll_fds, 
                                curr_max_poll * sizeof(struct pollfd));
            DIE(poll_fds == NULL, "[Server] Realloc poll failed.");
          }

          poll_fds[num_sockets].fd = newsockfd;
          poll_fds[num_sockets].events = POLLIN;
          num_sockets++;

          int rc = recv_all(newsockfd, &received_packet, 
                              sizeof(received_packet));
          DIE(rc < 0, "[Server] Failed to receive ID.");

          /* Verificam daca clientul este deja conectat sau daca a fost
          conectat pe server. */
          for (j = 0; j < num_clients; j++)
          {
            if (strcmp(clients[j].id, received_packet.message) == 0)
            {
              if (clients[j].status == 0)
              {
                // Client deconectat.
                clients[j].status = 1;
                clients[j].socket = newsockfd;
                printf("New client %s connected from %s:%d\n",
                       received_packet.message, inet_ntoa(cli_addr.sin_addr),
                       ntohs(cli_addr.sin_port));
                break;
              }
              else
              {
                // Client conectat.
                printf("Client %s already connected.\n",
                          received_packet.message);
                num_sockets--;
                close(poll_fds[num_sockets].fd);
                break;
              }
            }
          }

          /* Daca am trecut prin toate ID-urile si n-am gasit clientul, trecem
          noul client in vectorul de clienti ai server-ului. */
          if (j == num_clients)
          {
            printf("New client %s connected from %s:%d\n",
                   received_packet.message, inet_ntoa(cli_addr.sin_addr), 
                   ntohs(cli_addr.sin_port));
            if (num_clients == curr_max_clients)
            {
              curr_max_clients += ALLOC_SIZE;
              clients = realloc(clients, curr_max_clients * sizeof(client));
              DIE(clients == NULL, "[Server] Realloc client failed.");
            }
            strcpy(clients[num_clients].id, received_packet.message);

            // Initializeaza clientul.
            clients[num_clients].status = 1;
            clients[num_clients].socket = newsockfd;
            clients[num_clients].ntopics = 0;
            clients[num_clients].max_topics = ALLOC_SIZE;
            clients[num_clients].topics = malloc(ALLOC_SIZE * sizeof(char *));
            DIE(clients[num_clients].topics == NULL,
                  "[Server] Client topics malloc failed.");

            for (j = 0; j < clients[num_clients].max_topics; j++)
            {
              clients[num_clients].topics[j] = malloc(51 * sizeof(char));
              DIE(clients[num_clients].topics[j] == NULL,
                    "[Server] Malloc topics failed.");
            }

            num_clients++;
          }
        }
        else if (poll_fds[i].fd == udpfd)
        {
          // Receptioneaza un mesaj primit de la un client UDP.
          TCP_message recv_message;
          UDP_message *udpmes = malloc(sizeof(UDP_message));
          DIE(udpmes == NULL, "[Server] Malloc UDP message failed.");
          char topic_aux[2][51];

          int rc = recvfrom(poll_fds[i].fd, udpmes, sizeof(struct UDP_message),
                              0, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(rc < 0, "[Server] Couldn't get UDP message.");

          // Pregateste TCP-ul pentru a converti mesajul.
          strcpy(recv_message.ip, inet_ntoa(cli_addr.sin_addr));
          recv_message.port = cli_addr.sin_port;
          strcpy(recv_message.topic, udpmes->topic);

          // Verifica ce tip de payload am primit si prelucreaza-l.
          switch (udpmes->type)
          {
            case 0:
              // Cazul pentru int.
              uint32_t num_received = ntohl(*(uint32_t *)
                                            (udpmes->content + 1));
              if (udpmes->content[0] == 1)
              {
                num_received *= -1;
              }
              sprintf(recv_message.content, "%d", num_received);
              strcpy(recv_message.type, "INT");
              break;

            case 1:
              // Cazul pentru short real.
              float float_received = abs(ntohs(*(uint16_t *)udpmes->content));
              float_received /= 100;
              sprintf(recv_message.content, "%.2f", float_received);
              strcpy(recv_message.type, "SHORT_REAL");
              break;

            case 2:
              // Cazul pentru float.
              float first_num = ntohl(*(uint32_t *)(udpmes->content + 1));
              if (udpmes->content[0] == 1)
              {
                first_num *= -1;
              }
              first_num /= pow(10, udpmes->content[5]);
              sprintf(recv_message.content, "%.*f", 
                            udpmes->content[5], first_num);
              strcpy(recv_message.type, "FLOAT");
              break;

            default:
              // Cazul pentru string.
              strcpy(recv_message.content, udpmes->content);
              strcpy(recv_message.type, "STRING");
              break;
          }

          /* Cauta prin toti clientii pentru a gasi acei clienti care sunt
            abonati la topic. */
          for (int k = 0; k < num_clients; k++)
          {
            for (int l = 0; l < clients[k].ntopics; l++)
            {
              strcpy(topic_aux[0], clients[k].topics[l]);
              strcpy(topic_aux[1], recv_message.topic);

              // Verifica daca topicurile sunt matching.
              if ((strcmp(topic_aux[0], topic_aux[1]) == 0) ||
                  (check_wildcards(topic_aux[0], topic_aux[1]) == 0))
              {
                // Trimite mesajul daca clientul este conectat.
                if (clients[k].status == 1)
                {
                  sprintf(received_packet.message, "%s:%d - %s - %s - %s\n",
                          recv_message.ip, recv_message.port, 
                          recv_message.topic, recv_message.type, 
                          recv_message.content);
                  received_packet.len = strlen(received_packet.message);
                  int rc = send_all(clients[k].socket, &received_packet,
                              sizeof(received_packet));
                  DIE(rc < 0, 
                          "[Server] Client couldn't receive the message.\n");
                }
                break;
              }
            }
          }
          free(udpmes);
        }
        else if (poll_fds[i].fd == STDIN_FILENO)
        {
          // Input de la tastatura. Verificam daca se cere exit pe server.
          fgets(received_packet.message, MSG_MAXSIZE, stdin);
          received_packet.len = strlen(received_packet.message);
          if (strcmp(received_packet.message, "exit\n") == 0)
          {
            for (int j = 0; j < num_clients; j++)
            {
              // Inchide sesiunea tuturor clientilor activi.
              if (clients[j].status == 1)
              {
                int rc = send_all(clients[j].socket, &received_packet,
                                      sizeof(received_packet));
                DIE(rc < 0, "[Server] Client didn't close.\n");
              }
            }
            free(poll_fds);
            for(int j = 0; j < num_clients; j++) {
              for(int k = 0; k < clients[j].max_topics; k++) {
                free(clients[j].topics[k]);
              }
              free(clients[j].topics);
            }
            free(clients);
            exit(0);
          }
        }
        else
        {
          /* Am primit date pe unul din socketii de client, asa ca le
          receptionam. */
          int rc = recv_all(poll_fds[i].fd, &received_packet,
                            sizeof(received_packet));
          DIE(rc < 0, "[Server] Failed to receive packet\n");

          char *command = strtok(received_packet.message, " ");
          char topic_aux[51];

          if (rc == 0 || (strcmp(received_packet.message, "exit\n") == 0))
          {
            /* Daca datele primite returneaza 0 octeti sau daca comanda primita
            este exit, inchide clientul. */
            for (int j = 0; j < num_clients; j++)
            {
              if (clients[j].socket == poll_fds[i].fd)
              {
                clients[j].status = 0;
                clients[j].socket = -1;
                printf("Client %s disconnected.\n", clients[j].id);
                break;
              }
            }
            poll_fds[i].revents = 0;
            close(poll_fds[i].fd);

            // Scoatem din multimea de citire socketul inchis.
            for (int j = i; j < num_sockets - 1; j++)
            {
              poll_fds[j] = poll_fds[j + 1];
            }

            num_sockets--;
          }
          else if (strcmp(command, "subscribe") == 0 ||
                    strcmp(command, "unsubscribe") == 0)
          {
            // Receptioneaza ce comanda s-a trimis.
            int OK = 0;
            if (strcmp(command, "subscribe") == 0)
            {
              OK = 1;
            }

            // Extrage topicul din comanda data.
            command = strtok(NULL, " ");
            strcpy(topic_aux, command);

            // Cauta clientul care a trimis datele.
            for (int j = 0; j < num_clients; j++)
            {
              if (clients[j].socket == poll_fds[i].fd)
              {
                int k;
                /* Pentru unsubscribe, cauta topicul si muta celelalte
                topicuri peste. */
                if (OK == 0)
                {
                  for (k = 0; k < clients[j].ntopics; k++)
                  {
                    if (strcmp(clients[j].topics[k], topic_aux) == 0)
                    {
                      for (int l = k; l < clients[j].ntopics - 1; l++)
                      {
                        clients[j].topics[l] = clients[j].topics[l + 1];
                      }
                      clients[j].ntopics--;

                      break;
                    }
                  }
                  // Construieste mesajul.
                  sprintf(received_packet.message, 
                                "Unsubscribed from topic %s\n", topic_aux);
                  received_packet.len = strlen(received_packet.message);
                }
                else
                {
                  /* Insereaza topicul doar daca acesta nu se afla deja in
                  lista de topicuri. */
                  for (k = 0; k < clients[j].ntopics; k++)
                  {
                    if (strcmp(clients[j].topics[k], topic_aux) == 0)
                    {
                      break;
                    }
                  }
                  /* Daca nu a fost gasit, atunci insereaza topicul in vectorul
                  de topicuri. */
                  if (k == clients[j].ntopics)
                  {
                    if (clients[j].ntopics == clients[j].max_topics)
                    {
                      clients[j].max_topics += ALLOC_SIZE;
                      clients[j].topics = realloc(clients[j].topics,
                                      clients[j].max_topics * sizeof(char[51]));
                      DIE(clients[j].topics == NULL, "[Server] Realloc topic.");

                      for (k = clients[j].max_topics - ALLOC_SIZE; 
                              k < clients[j].max_topics; k++)
                      {
                        clients[j].topics[k] = malloc(51 * sizeof(char));
                        DIE(clients[j].topics[k] == NULL, 
                              "[Server] Malloc topics.");
                      }
                    }

                    strcpy(clients[j].topics[clients[j].ntopics], topic_aux);
                    clients[j].ntopics++;
                  }
                  // Construieste mesajul.
                  sprintf(received_packet.message,
                              "Subscribed to topic %s\n", topic_aux);
                  received_packet.len = strlen(received_packet.message);
                }
                int rc = send_all(poll_fds[i].fd, &received_packet,
                                    sizeof(received_packet));
                DIE(rc < 0, "[Server] Feedback failure.");
                break;
              }
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[])
{
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  if (argc != 2)
  {
    printf("\n Usage: %s <port>\n", argv[0]);
    return 1;
  }

  // Parsam port-ul ca un numar.
  uint16_t port;
  int rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "[Server] Given port is invalid");

  // Obtinem un socket UDP pentru receptionarea mesajelor.
  const int udpfd = socket(PF_INET, SOCK_DGRAM, 0);
  DIE(udpfd < 0, "[Server] socket");

  // Obtinem un socket TCP pentru receptionarea conexiunilor.
  const int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcpfd < 0, "[Server] socket");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare.
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  // Dezactivez algoritmul lui Nagle.
  const int enable = 1;
  if (setsockopt(tcpfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  // Asociem adresa serverului cu socketii creati folosind bind.
  rc = bind(tcpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "[Server] Bind failure.");

  rc = bind(udpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "[Server] Bind failure.");

  run_server(tcpfd, udpfd);

  // Inchidem fd.
  close(udpfd);
  close(tcpfd);

  return 0;
}
