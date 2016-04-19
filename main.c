#include "main.h"

int main(void) 
{

}

int pfpacket(int eth_index)
{
  int sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  struct tpacket_req req;
  req.tp_block_size = getpagesize() << 2;
  req.tp_frame_size = TPACKET_ALIGNMENT << 7;
  req.tp_block_nr = blocks;
  req.tp_frame_nr = ring->req.tp_block_size / 
    (ring->req.tp_frame_size *
     ring->req.tp_block_nr);

  ring->mm_len = ring->req.tp_block_size * ring->req.tp_block_nr;
  return sock
}
