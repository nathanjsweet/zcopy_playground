#include "main.h"

int main(void) 
{
  s_pfsocket* sock = pfsocket(0);
  
}

s_pfsocket* pfsocket(int eth_index)
{
  s_pfsocket* pfsock = (s_pfsocket*)malloc(sizeof(s_pfsocket));
  pfsock->sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
#ifdef TPACKET_V3
  struct tpacket_req3 req;
  req.tp_retire_blk_tov = 64;
  req.tp_sizeof_priv = 0;
  req.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;
#elif
  struct tpacket_req req;
#endif
  req.tp_block_size = getpagesize() << 2;
  req.tp_frame_size = TPACKET_ALIGNMENT << 7;
  req.tp_block_nr = blocks;
  req.tp_frame_nr = ring->req.tp_block_size / 
    (ring->req.tp_frame_size *
     ring->req.tp_block_nr);

  struct sockaddr_ll sll;
  sll.sll_family = PF_PACKET;
  sll.sll_halen = ETH_ALEN;
  sll.sll_ifindex = eth_index;
  sll.sll_protocol = htons(ETH_P_ALL);
  bind(pfsock->sock, (struct sockaddr*)&sll, sizeof(sll));

  setsockopt(pfsock->sock, SOL_PACKET, PACKET_RX_RING, (void*)&req, sizeof(req));
  setsockopt(pfsock->sock, SOL_PACKET, PACKET_TX_RING, (void*)&req, sizeof(req));

  int size = req.tp_block_size * req.tp_block_nr;
  uint8_t* m = mmap(0, size * 2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED | MAP_POPULATE, pfsock->sock, 0);
  pfsock->ring.frame_nr = req.tp_frame_nr * 2;
  pfsock->frames = (s_frame*)malloc(sizeof(s_frame) * pfsock->ring.frame_nr);

  int i,
    half = pfsock->ring.frame_nr/2,
    fr_loc;
  for(i = 0; i < pfsock->ring.frame_nr; i++) {
    fr_loc = i * req.tp_frame_size;
    pfsock->frames[i].frame_i = i;
    pfsock->frames[i].offset = fr_loc;
    pfsock->frames[i].type = i < half ? PACKET_RX_RING : PACKET_TX_RING;
    pfsock->frames[i].mem = m + fr_loc;
  }
  return pfsock
}

int listen(s_pfsocket* sock)
{
  struct pollfd pfd;
  union frame_map ppd;
  unsigned int frame_num = 0;

  memset(&pfd, 0, sizeof(pfd));
  pfd.fd = sock->sock
  pfd.events = POLLOUT | POLLIN | POLLERR;
  pfd.revents = 0;
  while(1) {
    while (__v1_v2_rx_kernel_ready(ring->rd[frame_num].iov_base,
				   ring->version)) {
      ppd.raw = sock->ring.frames[frame_num].mem;
      
      switch (PACKET_VERSION) {
      case TPACKET_V1:
	test_payload((uint8_t *) ppd.raw + ppd.v1->tp_h.tp_mac,
		     ppd.v1->tp_h.tp_snaplen);
	total_bytes += ppd.v1->tp_h.tp_snaplen;
	break;

      case TPACKET_V2:
	test_payload((uint8_t *) ppd.raw + ppd.v2->tp_h.tp_mac,
		     ppd.v2->tp_h.tp_snaplen);
	total_bytes += ppd.v2->tp_h.tp_snaplen;
	break;
      }

      status_bar_update();
      total_packets++;

      __v1_v2_rx_user_ready(ppd.raw, ring->version);

      frame_num = (frame_num + 1) % ring->rd_num;
    }
    poll(&pfd, 1, 1);
  }
}

int user_ready(union frame_map fm)
{
  
}
