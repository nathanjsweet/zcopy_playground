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
  if((int)sock0 < 0){
    fprintf(stderr, "ethernet, index %d, is invalid: %d\n", index0, errno);
    return 1;
  }
  struct pfsocket* sock1 = pfsocket(index1);
  if((int)sock1 < 0){
    fprintf(stderr, "ethernet, index %d, is invalid: %d\n", index0, errno);
    return 1;
  }
  int err = pf_listen(sock0, sock0_listener);
  fprintf(stderr, "error: %d\n", err);
  return 0;
}

void sock0_listener(uint8_t* pay, unsigned int len)
{
  print_mac_dest(pay);
  print_mac_source(pay);
}

struct pfsocket* pfsocket(int eth_index, int options)
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

  int rx_enabled = options & ENABLE_RX,
    tx_enabled = options & ENABLE_TX,
    val = TPACKET_V1;
  setsockopt(pfsock->sock, SOL_PACKET, PACKET_VERSION, &val, sizeof(val));
  if(rx_enabled)
    setsockopt(pfsock->sock, SOL_PACKET, PACKET_RX_RING, (void*)&req, sizeof(req));
  if(tx_enabled)
    setsockopt(pfsock->sock, SOL_PACKET, PACKET_TX_RING, (void*)&req, sizeof(req));

  int size = req.tp_block_size * req.tp_block_nr * (rx_enabled && tx_enabled ? 2 : 1);
  uint8_t* m = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED | MAP_POPULATE, pfsock->sock, 0);
  if((int)m == -1)
    return PF_ERR_SYS;

  pfsock->frame_nr = req.tp_frame_nr;
  if(rx_enabled)
    pfsock->rx_frames = (uint8_t*)malloc(sizeof(uint8_t*) * req.tp_frame_nr);
  else
    pfsock->rx_frames = NULL;
  if(tx_enabled)
    pfsock->tx_frames = (struct tx_frame*)malloc(sizeof(struct tx_frame) * req.tp_frame_nr);
  else
    pfsock->tx_frames = NULL;
  
  pfsock->last_tx_index = 0;
  pfsock->listening = 0;
  
  int i,t,fr_loc;
  if(rx_enabled)
    for(i = 0; i < pfsock->frame_nr; i++) {
      fr_loc = i * req.tp_frame_size;
      pfsock->rx_frames[i] = m + fr_loc;
    }
  if(tx_enabled)
    for(t = 0; t < pfsock->frame_nr; t++, i++){
      fr_loc = i * req.tp_frame_size;
      pfsock->tx_frames[t].mem = m + fr_loc;
      pfsock->tx_frames[t].inuse = 0;
    }
  return pfsock;
}

int pf_listen(struct pfsocket* sock, void (*fx)(uint8_t*,unsigned int))
{
  if(sock->rx_frames == NULL)
    return PF_ERR_DISABLED;
  if(!__sync_bool_compare_and_swap(&sock->listening, 0, 1))
    return PF_ERR_LISTENING;

  struct pollfd pfd;
  memset(&pfd, 0, sizeof(pfd));
  pfd.fd = sock->sock;
  pfd.events = POLLIN | POLLERR;
  pfd.revents = 0;
  unsigned int rx_index = 0;
  union frame_map ppd;
  while(1) {
    for(ppd.raw = sock->rx_frames[rx_index];
	user_ready(ppd.raw);
	rx_index = (rx_index + 1) % sock->frame_nr) {

      fx((uint8_t *)(ppd.raw + ppd.hdr->tp_h.tp_mac), ppd.hdr->tp_h.tp_len);
      set_kernel_ready(ppd.raw);
    }
    if(poll(&pfd, 1, 1) == -1) {
      sock->listening = 0;
      return PF_ERR_SYS;
    }
  }
}

int pf_write(struct pfsocket* sock, uint8_t* buf, int len)
{
  if(sock->tx_frames == NULL)
    return PF_ERR_DISABLED;
  struct pollfd pfd;
  memset(&pfd, 0, sizeof(pfd));
  pfd.fd = sock->sock;
  pfd.events = POLLOUT | POLLERR;
  pfd.revents = 0;
  union frame_map ppd;
  int index_acc;
  int tx_index, tries;
  for(tries = 0; tries < MAX_WRITE_ATTEMPTS; tries++) {
    index_acc = 0;
    for(tx_index = _get_available_tx_index(sock, &index_acc);
	index_acc < sock->frame_nr;
	tx_index = _get_available_tx_index(sock, &index_acc)) {
      ppd.raw = sock->tx_frames[tx_index].mem;
      if(kernel_ready(ppd.raw)) {
	ppd.hdr->tp_h.tp_snaplen = len;
	ppd.hdr->tp_h.tp_len = len;
	memcpy((uint8_t *) ppd.raw + TPACKET_HDRLEN - sizeof(struct sockaddr_ll), buf, len);
	set_user_ready(ppd.raw);
	sock->tx_frames[tx_index].inuse = 0;
	return len;
      }
      sock->tx_frames[tx_index].inuse = 0;
    }
    if(poll(&pfd, 1, 1) == -1)
      return PF_ERR_SYS;
  }
  return PF_ERR_MAX_WRITE_ATTEMPTS;
}

int _get_available_tx_index(struct pfsocket* sock, int *index_acc)
{
  int tx_index;
  int acc = 0;
  for(tx_index = _iterate_tx_index(sock, &acc);
      !__sync_bool_compare_and_swap(&sock->tx_frames[tx_index].inuse, 0, 1);
      tx_index = _iterate_tx_index(sock, &acc))
    if(acc >= sock->frame_nr)
      break;
  if(index_acc != NULL)
    *index_acc += acc;
  return tx_index;
}

int _iterate_tx_index(struct pfsocket* sock, int *index_acc)
{
  int tx_index = sock->last_tx_index;
  int frame_nr = sock->frame_nr;
  int acc = 0;
  while(!__sync_bool_compare_and_swap(&sock->last_tx_index,
				      tx_index,
				      (tx_index + 1) % frame_nr)) {
    int n_index = sock->last_tx_index;
    if(n_index < tx_index)
      acc += (frame_nr - tx_index) + n_index;
    else
      acc += n_index - tx_index;
    tx_index = n_index;
  }
  if(index_acc != NULL)
    *index_acc += acc;
  return tx_index;  
}

int user_ready(struct tpacket_hdr* hdr)
{
  return ((hdr->tp_status & TP_STATUS_USER) == TP_STATUS_USER);
}

void set_user_ready(struct tpacket_hdr* hdr)
{
  hdr->tp_status = TP_STATUS_SEND_REQUEST;
  __sync_synchronize();
}

int kernel_ready(struct tpacket_hdr* hdr)
{
  return !(hdr->tp_status & (TP_STATUS_SEND_REQUEST | TP_STATUS_SENDING));
}

void set_kernel_ready(struct tpacket_hdr* hdr)
{
  hdr->tp_status = TP_STATUS_KERNEL;
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
