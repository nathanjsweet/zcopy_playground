#include <sys/socket.h>
//
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
//

#include <linux/if_packet.h>

//
#include <linux/filter.h>
//

#include <inttypes.h>

//
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
//

#include <errno.h>

//
#include <bits/wordsize.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
//

#ifndef __aligned_tpacket
# define __aligned_tpacket __attribute__((aligned(TPACKET_ALIGNMENT)))
#endif

#ifndef __align_tpacket
# define __align_tpacket(x) __attribute__((aligned(TPACKET_ALIGN(x))))
#endif

#define NUM_PACKETS 100
#define ALIGN_8(x) (((x) + 8 - 1) & ~(8 - 1))

union frame_map {
  struct {
    struct tpacket_hdr tp_h __aligned_tpacket;
    struct sockaddr_ll s_ll __align_tpacket(sizeof(struct tpacket_hdr));
  } *hdr;
  void *raw;
};

struct frame {
  int offset;
  uint8_t *mem;
};

struct pfsocket {
  int sock;
  int frame_nr;
  struct frame *rx_frames;
  struct frame *tx_frames;
};

struct pfsocket* pfsocket();
int pf_listen(struct pfsocket*);
int user_ready(struct tpacket_hdr*);
int set_kernel_ready(struct tpacket_hdr*);

void print_mac_dest(uint8_t*);
void print_mac_source(uint8_t*);
void print_8(uint8_t,char*);
