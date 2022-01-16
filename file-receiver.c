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

#define TIMEOUT  -1

int main(int argc, char *argv[]) {
  char *file_name = argv[1];
  int port = atoi(argv[2]);
  int ws = atoi(argv[3]);

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

  //***********************************************
  //(1) 
  //***********************************************
  struct sockaddr_in src_addr;
  data_pkt_t data_pkt;
  ack_pkt_t ack_pkt;


  //**********************************************
  //    Defenition of the timeout of 4s
  //**********************************************
  struct timeval tv;
  tv.tv_sec = 4;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  //**********************************************
  //    Defenition of auxiliar variables
  //**********************************************
  int bot_pkt = 0; // seq_num first packet in receiver windows that we are waiting
  uint32_t received_pkts = 0;

  ssize_t len;
  int last_pkt = -1; // last packet

  do { // Iterate over segments, until last the segment is detected.
    // Receive segment.
    printf("\n");
    //(1)


    len =
        recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                 (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});


    if(len == -1)
    {
      printf("ABORTING\n");
      close(sockfd);
      fclose(file); 
      exit(EXIT_FAILURE);
    }

    printf("Received segment %d, size %ld.\n", ntohl(data_pkt.seq_num), len);

    //**********************************************
    // Drop that packet and resend our bot packet
    //********************************************** 
    printf("    (%d-%d)\n",ntohl(data_pkt.seq_num), bot_pkt);
    if(ntohl(data_pkt.seq_num)-bot_pkt>=ws){ //Drop packet that arent inside receiver window
      printf("DROP PACKET AND SEND BOT PACKET%d\n", ntohl(data_pkt.seq_num));
      len = sizeof(data_pkt_t);
      ack_pkt.seq_num = htonl(bot_pkt);
      //printf("(RECEIVER) ASKING FOR (%d)\n", ntohl(ack_pkt.seq_num));
      sendto(sockfd, &ack_pkt,  sizeof(ack_pkt), 0,
                    (struct sockaddr *)&src_addr, sizeof(src_addr));
      continue;
    }

    received_pkts = received_pkts | (1<<ntohl(data_pkt.seq_num));
    // Write data to file.
    if(ntohl(data_pkt.seq_num)==bot_pkt){
      printf(" ADVANCE RECEIVER WINDOW\n");
      ++bot_pkt; //We received 
      //ADVANCE RECEIVER WINDOW TO NEXT EMPTY PACKT
      //ws = 2 here
      for(int i=bot_pkt; i<bot_pkt+ws; i++){
        printf(" i(%d)", i);
        if((received_pkts>>i)%2 == 0){
          bot_pkt = i;
          printf("\t\t\t NEW BOTTOM PACKET %d\n", bot_pkt);
          break;
        }
      }
    }



    // sendto(sockfd, &ack_pkt,  sizeof(ack_pkt), 0,
    //               (struct sockaddr *)&src_addr, sizeof(src_addr));
    ack_pkt.selective_acks = htonl(received_pkts);
    ack_pkt.seq_num = htonl(bot_pkt);
    sendto(sockfd, &ack_pkt, sizeof(ack_pkt_t)+offsetof(ack_pkt_t, seq_num)+offsetof(ack_pkt_t, selective_acks), 0,
                  (struct sockaddr *)&src_addr, sizeof(src_addr));
          


    //WRITE DATA ON CERTAIN POSITION BASED ON data_pkt.seq_num
    fseek(file, (ntohl(data_pkt.seq_num))*sizeof(data_pkt.data), SEEK_SET);
    fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);

    //FAZER AQUI UM CONDICAO QUE VERIFICA QUE CASO RECEBEMOS ULTIMO PACKET MAS 
    // AINDA EXISTEM SEGMENTOS QUE NAO FORAM RESPONDIDOS FAZER NAO PERMITIR TERMINAR
    //FAZER 

    if(len != sizeof(data_pkt_t)){ 
      last_pkt = ntohl(data_pkt.seq_num);
    }

    int num_of_nack = 0;
    for(int i = 0; i <= last_pkt; i++){
      if((received_pkts>>i)%2 == 0){
        ++num_of_nack;
      }
    }

    //if(last_pkt != -1 && num_of_nack==0){
    if(last_pkt == -1 || num_of_nack!=0){
      //printf("                      RECEIVED NOT ALL PACKETS\n");
      len = sizeof(data_pkt_t);
    }

    //printf("                                      NUM OF NACKS %d\n", num_of_nack);
    

  } while ((len == sizeof(data_pkt_t)));
  // }while(true);

  //
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  //**********************************************
  //    Waiting aditional 2 sec before closing
  // if in this interval we reecive another ack
  // resend last ack_pkt that contains 
  // ack for last pkt plus bit vector
  //********************************************** 
  
  do{
      printf("WAITING ADITIONAL 2 SEC\n");
      len =
        recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                 (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)}); 

      if(len != TIMEOUT){
        ack_pkt.seq_num = htonl(bot_pkt);
        sendto(sockfd, &ack_pkt,  sizeof(ack_pkt), 0,
                  (struct sockaddr *)&src_addr, sizeof(src_addr));
      }
  }while(len != TIMEOUT);
          
  // Clean up and exit.
  printf("RECEIVER CLOSING\n");
  close(sockfd);
  fclose(file);
  return EXIT_SUCCESS;
}
