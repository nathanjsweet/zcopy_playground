
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <linux/if_packet.h>

#ifdef TPACKET_V3
#define TPACKET_VERSION TPACKET_V3
#elif
#define TPACKET_VERSION TPACKET_V1
#endif

#ifndef __aligned_tpacket
# define __aligned_tpacket__attribute__((aligned(TPACKET_ALIGNMENT)))
#endif

#ifndef __align_tpacket
# define __align_tpacket(x)__attribute__((aligned(TPACKET_ALIGN(x))))
#endif

#define NUM_PACKETS100
#define ALIGN_8(x)(((x) + 8 - 1) & ~(8 - 1))

union frame_map {
  struct {
    struct tpacket_hdr tp_h __aligned_tpacket;
    struct sockaddr_ll s_ll __align_tpacket(sizeof(struct tpacket_hdr));
  } *v1;
  struct {
    struct tpacket2_hdr tp_h __aligned_tpacket;
    struct sockaddr_ll s_ll __align_tpacket(sizeof(struct tpacket2_hdr));
  } *v2;
  void *raw;
};

typedef struct frame {
  int frame_i;
  int offset;
  int type;
  uint8_t *mem;
} s_frame;

typedef struct ring {
  int frame_nr;
  s_frame *frames;
} s_ring;

typedef struct pfsocket_s {
  int sock;
  s_ring ring;
} s_pfsocket;

s_pfsocket* pfsocket();
int listen(s_pfsocket*);
