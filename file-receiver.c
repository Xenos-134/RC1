#include "packet-format.h"
#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char *file_name = argv[1];
  int port = atoi(argv[2]);
  int window_size = atoi(argv[3]);
  printf("SIZE OF WINDOW RECEIVER %d \n", window_size);

  FILE *file = fopen(file_name, "w");
  if (!file) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  // Prepare server socket.
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Allow address reuse so we can rebind to the same port,
  // after restarting the server.
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = htonl(INADDR_ANY),
      .sin_port = htons(port),
  };
  if (bind(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) {
    perror("bind");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "Receiving on port: %d\n", port);
  data_pkt_t data_pkt;
  ack_pkt_t ack_pkt; //Ack packet

  ssize_t len;
  uint32_t received_pkts = 0; //binary list of received packets
  int bottom_pkt = 0; //Packet com seq # mais baixo da janela

  //TIMER FOR RECEIVER
  struct timeval tv;
  tv.tv_sec = 4;
  tv.tv_usec = 4;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  // bool finished = false;
  // bool last_loop = false;

  //DEFENITION OF THE SENDER ADRESS AND PORT
  char ip[20];
  int sport = 0;


  do { // Iterate over segments, until last the segment is detected.
    // Receive segment.
    //if(finished) last_loop = true; // To execute one more loop

    struct sockaddr_in src_addr;

    len =
        recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                 (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});

    if(sport == 0) // IT OUR FIRST CONNECTION
    {
      sport = htons(src_addr.sin_port);
      inet_ntop(AF_INET, &src_addr.sin_addr, ip, sizeof(ip));
    }


    if(strcmp(inet_ntoa(src_addr.sin_addr), ip)!=0 || htons(src_addr.sin_port)!=sport)
    { //IF THE SENDER IS NOT THE SAME
      continue;
    }

    printf("Received segment %d, size %ld.\n", ntohl(data_pkt.seq_num), len);

    if(len == -1)
    {
        close(sockfd);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // if(finished && len == -1)
    // {
    //   printf(">>>>WAITING ADITIONAL 2 SEC\n");
    //   close(sockfd);
    //   fclose(file);
    //   exit(EXIT_FAILURE);
    // }


    // if(len != sizeof(data_pkt_t)) //WE FINISHED AND WANT WAIT ADITIONAL SECONDS
    // {
    //   printf("HELLO THERe\n");
    //   tv.tv_sec = 2;
    //   tv.tv_usec = 2;
    //   setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    //   finished = true;
    
    // }

    //Parte de GBN
    //*****************************************
    // Caso Packet esta fora da janela
    //*****************************************

    if(!(ntohl(data_pkt.seq_num)<bottom_pkt+window_size))
    {
      ack_pkt.seq_num = htonl(bottom_pkt);
        sendto(sockfd, &ack_pkt,  sizeof(ack_pkt_t)+offsetof(ack_pkt_t, seq_num)+offsetof(ack_pkt_t, selective_acks), 0,
                  (struct sockaddr *)&src_addr, sizeof(src_addr));

      continue;
    }

    //*****************************************
    // Caso Packet esta dentro da janela
    //*****************************************
    //printf("(IN ORDER %d - %d) (%d)\n", bottom_pkt, bottom_pkt+window_size, ntohl(data_pkt.seq_num));
    //**************************
    received_pkts = received_pkts | (1<<ntohl(data_pkt.seq_num));
    ack_pkt.selective_acks = htonl(received_pkts);
    //**************************



    bottom_pkt = ntohl(data_pkt.seq_num)+1;

    //Answer to sender that we received that socket
    ack_pkt.seq_num = htonl(ntohl(data_pkt.seq_num)+1);


    sendto(sockfd, &ack_pkt, sizeof(ack_pkt_t)+offsetof(ack_pkt_t, seq_num)+offsetof(ack_pkt_t, selective_acks), 0,
                  (struct sockaddr *)&src_addr, sizeof(src_addr));

    // Write data to file.
    fseek(file, (ntohl(data_pkt.seq_num))*sizeof(data_pkt.data), SEEK_SET);
    fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);

  } while (len == sizeof(data_pkt_t));

  // Clean up and exit.
  close(sockfd);
  fclose(file);
  return EXIT_SUCCESS;
}
