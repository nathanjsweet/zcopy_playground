#include <stdio.h>

#include "main.h"

void sock0_listener(uint8_t*, unsigned int);

int main(int argc, char* argv[]) 
{
  if(argc < 3) {
    fprintf(stderr, "two ethernet index arguments are required\n");
    return 1;
  }
  int index0 = atoi(argv[1]),
    index1 = atoi(argv[2]);
  printf("index0 is %d:%s, index1 is %d:%s\n", index0, argv[1], index1, argv[2]);
  struct pfsocket* sock0 = pfsocket(index0);
  if((int)sock0 == -1){
    fprintf(stderr, "ethernet, index %d, is invalied\n", index0);
    return 1;
  }
  /*
  struct pfsocket* sock1 = pfsocket(index1);
  if((int)sock1 == -1){
    fprintf(stderr, "ethernet, index %d, is invalied\n", index0);
    return 1;
    }*/
  int err = pf_listen(sock0, sock0_listener);
  fprintf(stderr, "error: %d\n", err);
  return 0;
}

void sock0_listener(uint8_t* pay, unsigned int len)
{
  print_mac_dest(pay);
  print_mac_source(pay);
}

struct pfsocket* pfsocket(int eth_index)
{
  struct pfsocket* pfsock = (struct pfsocket*)malloc(sizeof(struct pfsocket));
  pfsock->sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

  struct tpacket_req req;
  req.tp_block_size = getpagesize() << 2;
  req.tp_frame_size = TPACKET_ALIGNMENT << 7;
  req.tp_block_nr = 256;
  req.tp_frame_nr = (req.tp_block_size / req.tp_frame_size) * req.tp_block_nr;

  struct sockaddr_ll sll;
  sll.sll_family = PF_PACKET;
  sll.sll_halen = ETH_ALEN;
  sll.sll_ifindex = eth_index;
  sll.sll_protocol = htons(ETH_P_ALL);
  bind(pfsock->sock, (struct sockaddr*)&sll, sizeof(sll));

  int val = TPACKET_V1;
  setsockopt(pfsock->sock, SOL_PACKET, PACKET_VERSION, &val, sizeof(val));
  setsockopt(pfsock->sock, SOL_PACKET, PACKET_RX_RING, (void*)&req, sizeof(req));
  //setsockopt(pfsock->sock, SOL_PACKET, PACKET_TX_RING, (void*)&req, sizeof(req));

  int size = req.tp_block_size * req.tp_block_nr;
  uint8_t* m = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED | MAP_POPULATE, pfsock->sock, 0);
  if((int)m == -1){
    printf("errno: %d\n", errno);
    return -1;
  }
  pfsock->frame_nr = req.tp_frame_nr;
  pfsock->rx_frames = (struct frame*)malloc(sizeof(struct frame) * req.tp_frame_nr);
  /*  pfsock->tx_frames = (struct frame*)malloc(sizeof(struct frame) * req.tp_frame_nr);
      pfsock->last_tx_index = 0;*/
  
  int i,t,fr_loc;
  for(i = 0; i < pfsock->frame_nr; i++) {
    fr_loc = i * req.tp_frame_size;
    pfsock->rx_frames[i].offset = fr_loc;
    pfsock->rx_frames[i].mem = m + fr_loc;
  }
  /*  for(t = 0; t < pfsock->frame_nr; t++, i++){
    fr_loc = i * req.tp_frame_size;
    pfsock->tx_frames[t].offset = fr_loc;
    pfsock->tx_frames[t].mem = m + fr_loc;
    }*/
  return pfsock;
}

int pf_listen(struct pfsocket* sock, void (*fx)(uint8_t*,unsigned int))
{
  struct pollfd pfd;

  memset(&pfd, 0, sizeof(pfd));
  pfd.fd = sock->sock;
  pfd.events = POLLIN | POLLERR;
  pfd.revents = 0;
  unsigned int frame_num = 0;
  union frame_map ppd;
  uint8_t* pay;
  while(1) {
    while(user_ready(sock->rx_frames[frame_num].mem)){
      ppd.raw = sock->rx_frames[frame_num].mem;
      pay = (uint8_t *)(ppd.raw + ppd.hdr->tp_h.tp_mac);
      print_mac_dest(pay);
      print_mac_source(pay);
      set_kernel_ready(ppd.raw);
      frame_num = (frame_num + 1) % sock->frame_nr;
    }
    if(poll(&pfd, 1, 1) == -1) {
      return errno;
    }
  }
  return 0;
}

int pf_write(struct pfsocket* sock, uint8_t* buf, int start, int stop)
{
  
}

int user_ready(struct tpacket_hdr* hdr)
{
  return ((hdr->tp_status & TP_STATUS_USER) == TP_STATUS_USER);
}

int set_kernel_ready(struct tpacket_hdr* hdr)
{
  hdr->tp_status = TP_STATUS_KERNEL;
  // this is to make sure that is set_kernel_ready is inlined
  // which it is likely to be that no memory operation is
  // performed after the above status set. If an interrupt
  // we're to happen that would be inconvenient.
  __sync_synchronize();
}

void print_mac_dest(uint8_t* pay)
{
  printf("mac dest:");
  print_8(*pay, ":");
  pay++;
  print_8(*pay, ":");
  pay++;
  print_8(*pay, ":");
  pay++;
  print_8(*pay, ":");
  pay++;
  print_8(*pay, ":");
  pay++;
  print_8(*pay, "\n");
}

void print_mac_source(uint8_t* pay)
{
  pay += 6;
  printf("mac source:");
  print_8(*pay, ":");
  pay++;
  print_8(*pay, ":");
  pay++;
  print_8(*pay, ":");
  pay++;
  print_8(*pay, ":");
  pay++;
  print_8(*pay, ":");
  pay++;
  print_8(*pay, "\n");
}

void print_8(uint8_t d, char* s)
{
  printf("%" PRIu8 "%s", d, s);
}
