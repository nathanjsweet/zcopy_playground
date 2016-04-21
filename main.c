#include <stdio.h>

#include "main.h"

int main(void) 
{
  struct pfsocket* sock = pfsocket(1);
  if((int)sock == -1)
    return 1;
  pf_listen(sock);
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
  void* m = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED | MAP_POPULATE, pfsock->sock, 0);
  if((int)m == -1){
    printf("errno: %d\n", errno);
    return -1;
  }
  pfsock->frame_nr = req.tp_frame_nr * 2;
  pfsock->rx_frames = (struct frame*)malloc(sizeof(struct frame) * req.tp_frame_nr);
  //  pfsock->tx_frames = (struct frame*)malloc(sizeof(struct frame) * req.tp_frame_nr);

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

int pf_listen(struct pfsocket* sock)
{
  struct pollfd pfd;

  memset(&pfd, 0, sizeof(pfd));
  pfd.fd = sock->sock;
  pfd.events = POLLIN | POLLERR;
  pfd.revents = 0;
  while(1) {
    unsigned int frame_num = 0;
    union frame_map ppd;
    ppd.raw = sock->rx_frames[frame_num].mem;
    uint8_t* pay = (uint8_t *)(ppd.raw + ppd.hdr->tp_h.tp_mac);
    while(user_ready(&ppd.hdr->tp_h)){
      print_mac_dest(pay);
      print_mac_source(pay);
      set_kernel_ready(&ppd.hdr->tp_h);
      frame_num = (frame_num + 1) % sock->frame_nr;
      ppd.raw = sock->rx_frames[frame_num].mem;
      pay = (uint8_t *)(ppd.raw + ppd.hdr->tp_h.tp_mac);
    }
    poll(&pfd, 1, 1);
  }
  return 0;
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
