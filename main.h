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

#define ALIGN_8(x) (((x) + 8 - 1) & ~(8 - 1))

#define PF_ERR_SYS		-1 /* system call error; check errno */
#define PF_ERR_LISTENING	-2 /* socket already being listened to */
#define PF_ERR_MAX_WRITE	-3 /* socket reached maximum amount of tries to write to socket; write failure */

union frame_map {
  struct {
    struct tpacket_hdr tp_h __aligned_tpacket;
    struct sockaddr_ll s_ll __align_tpacket(sizeof(struct tpacket_hdr));
  } *hdr;
  void *raw;
};

struct tx_frame {
  uint8_t *mem;
  uint8_t inuse;
};

struct pfsocket {
  int listening;
  int sock;
  int frame_nr;
  int last_tx_index;
  uint8_t **rx_frames;
  struct tx_frame *tx_frames;
};

struct pfsocket* pfsocket();
int pf_listen(struct pfsocket*, void (*fx)(uint8_t*,unsigned int));
int pf_write(struct pfsocket*, uint8_t*, int);
void set_user_ready(struct tpacket_hdr*);
int user_ready(struct tpacket_hdr*);
void set_kernel_ready(struct tpacket_hdr*);
int kernel_ready(struct tpacket_hdr*);
int _get_available_tx_index(struct pfsocket*, int*);
int _iterate_tx_index(struct pfsocket*, int*);

void print_mac_dest(uint8_t*);
void print_mac_source(uint8_t*);
void print_8(uint8_t,char*);
