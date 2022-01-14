#include "packet-format.h"
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char *file_name = argv[1];
  char *host = argv[2];
  int port = atoi(argv[3]);
  int window_size = atoi(argv[4]);

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

  //Timeout defenition
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 1;

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  uint32_t seq_num = 0;
  data_pkt_t data_pkt;
  ack_pkt_t ack_pkt; //Ack packet

  size_t data_len;
  ssize_t sent_len;
  int bottom_pkt = 0;  //Packet com seq # mais baixo na janela
  //int top_pkt=-1; //

  fseek(file, 0L, SEEK_END);
  int num_of_packets = ftell(file)/sizeof(data_pkt.data);
  fseek(file, 0L, SEEK_SET);

  int num_of_fr = 3; // number of failed retransmissions
  uint32_t prev_acks = 0; // bit array representing previous acks
  uint32_t cur_acks = 0; // bit array representig current acks


  do { // Generate segments from file, until the the end of the file.
    // Prepare data segment.
    //seq_num = bottom_pkt;
    if(prev_acks == cur_acks)
    {
      ++num_of_fr;
      if(num_of_fr == 3)
      {
        close(sockfd);
        fclose(file);
        exit(EXIT_FAILURE);
      }
    }

    while((seq_num-bottom_pkt)<window_size && seq_num != num_of_packets+1)
    {
      data_pkt.seq_num = htonl(seq_num++);
      // Load data from file.
      if((cur_acks>>(seq_num-1))%2 == 1)
      {
        //printf(" ############### WE ALREADY WERE SUPOSED TO SAVE THAT PACKET %d \n", seq_num-1);
        //LISTENER ALREADY SAVED THAT PACKET
        continue;
      }
      fseek(file, (seq_num-1)*sizeof(data_pkt.data), SEEK_SET);
      data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

      // Send segment.
      sent_len =
          sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                (struct sockaddr *)&srv_addr, sizeof(srv_addr));
      printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
            offsetof(data_pkt_t, data) + data_len);
    }


    //********************************************************
    //Waiting For Response From Receiver 
    //  timeout == -1?(resend):(send next)
    //********************************************************
    int timeout  =  recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt_t) + offsetof(ack_pkt_t, selective_acks)+offsetof(ack_pkt_t, seq_num), 0,
                 (struct sockaddr *)&srv_addr, &(socklen_t){sizeof(srv_addr)});


    if(timeout != -1) //If ack arrived in indicated time window
    {
      //printf("    SENDER SELECTIVE ACKS %d\n\t\t", ntohl(ack_pkt.selective_acks));
      // for(int i=0; i<32; i++)
      // {
      //   printf("%d ", (ntohl(ack_pkt.selective_acks)>>i)%2);
      // }
      // printf("\n");
      prev_acks = cur_acks;
      cur_acks = ntohl(ack_pkt.selective_acks);
      if(cur_acks != prev_acks) num_of_fr=0;

      if(ntohl(ack_pkt.seq_num) == bottom_pkt+1) // If we received ack for the bottom ack we can advance our window
      {
        ++bottom_pkt;
      } 
    }else{
      //printf("XXX TimeOut For Packet (%d)\n", ntohl(data_pkt.seq_num));
      seq_num=bottom_pkt;
      prev_acks = cur_acks;
    }


    if (sent_len != offsetof(data_pkt_t, data) + data_len) {
      fprintf(stderr, "Truncated packet.\n");
      exit(EXIT_FAILURE);
    }
  } while (!(feof(file) && data_len < sizeof(data_pkt.data) && bottom_pkt == num_of_packets+1));

  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return EXIT_SUCCESS;
}
