#include "packet-format.h"
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TIMEOUT -1

int main(int argc, char *argv[]) {
  char *file_name = argv[1];
  char *host = argv[2];
  int port = atoi(argv[3]);
  int ws = atoi(argv[4]);//window size

  FILE *file = fopen(file_name, "r");
  if (!file) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  // Prepare server host address.
  struct hostent *he;
  if (!(he = gethostbyname(host))) {
    perror("gethostbyname");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = *((struct in_addr *)he->h_addr),
  };

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  uint32_t seq_num = 0;
  data_pkt_t data_pkt;
  ack_pkt_t ack_pkt;
  size_t data_len;

  //**********************************************
  //    Defenition of the timeout of 1s
  //**********************************************
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  //**********************************************
  //   ADITIONAL params
  //**********************************************
  int num_of_fails = 0;
  int bot_pkt = 0;
  //Get num of packets
  fseek(file, 0L, SEEK_END);
  int num_of_packets = ftell(file)/sizeof(data_pkt.data);
  fseek(file, 0L, SEEK_SET);
  
  uint32_t received_pkts = 0;

  do { // Generate segments from file, until the the end of the file.
    // Prepare data segment.
    if(num_of_fails==3){
        fclose(file);
        close(sockfd);
        printf("(FAIL) CLOSING SENDER\n");
        exit(EXIT_FAILURE);
    }

    //seq_num = bot_pkt;
    while((seq_num-bot_pkt)<ws && seq_num <= num_of_packets){ //
      data_pkt.seq_num = htonl(seq_num++);
      if((ntohl(ack_pkt.selective_acks)>>(seq_num-1))%2 == 1) {
        printf("\t\t\t WE ALREADY SENT THAT PACKET\n");
        continue;
      }
      // Load data from file.
      fseek(file, (seq_num-1)*sizeof(data_pkt.data), SEEK_SET);
      data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

      // Send segment.
      ssize_t sent_len =
          sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                (struct sockaddr *)&srv_addr, sizeof(srv_addr));


      printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
          offsetof(data_pkt_t, data) + data_len);
      if (sent_len != offsetof(data_pkt_t, data) + data_len) {
          fprintf(stderr, "Truncated packet.\n");
          exit(EXIT_FAILURE);
        }
    }

    // int timeout  =  recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0,
    //              (struct sockaddr *)&srv_addr, &(socklen_t){sizeof(srv_addr)});

    int timeout  =  recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt_t) + offsetof(ack_pkt_t, selective_acks)+offsetof(ack_pkt_t, seq_num), 0,
                 (struct sockaddr *)&srv_addr, &(socklen_t){sizeof(srv_addr)});
    printf("========================\n");
    received_pkts = ntohl(ack_pkt.selective_acks);
    for(int i=0; i<15; i++){
      if((ntohl(ack_pkt.selective_acks)>>i)%2 == 0){ 
        printf(" o");
      }else{
        printf(" v");
      }
    }
    printf("\n");

    //**********************************************
    //    If sender have not received any responses
    // in 1 sec
    //**********************************************
    if(timeout == TIMEOUT){ //Timeout ocurred just dont allow sender terminate
      printf("(SENDER) TIMEOUT\n");

      fseek(file, (seq_num-1)*sizeof(data_pkt.data), SEEK_SET);
      fseek(file, 0L, SEEK_SET);
      // if timeout ocurred than we need to restart sending all packets that are inside of the window
      seq_num = bot_pkt;
      ++num_of_fails;
      continue;
    }

    //**********************************************
    //    If sender received response
    //**********************************************
    num_of_fails = 0;
    //OUR BOTTOM PACKET WAS RECEIVED
    if(ntohl(ack_pkt.seq_num)>bot_pkt){
      bot_pkt =  ntohl(ack_pkt.seq_num);
      printf("    BOT PACKET %d\n", bot_pkt);
    }



  } while (!(feof(file) && data_len < sizeof(data_pkt.data)) || bot_pkt != num_of_packets+1);

  printf("(CLOSING SENDER %d-%d)\n", num_of_packets, bot_pkt);
  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return EXIT_SUCCESS;
}
