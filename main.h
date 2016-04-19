
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

typedef struct frame {
  int frame_i;
  int offset;
  uint8_t *mem;
} s_frame;

typedef struct ring {
  int frame_rn;
  s_frame *frames;
} s_ring;

typedef struct pfsocket {
  int sock;
  s_ring ring;
} s_pfsocket;

int pfsocket();
