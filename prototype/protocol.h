#ifndef REDGEAR_PROTOCOL_H
#define REDGEAR_PROTOCOL_H
#include "pad.h"
#include <stddef.h>
#include <string.h>
#define RG_PACKET_SIZE 36
#define RG_PORT 8096
#define RG_BUTTON_MASK 0x0011ffffu
/* RGP1, 16-byte pairing token, sequence LE32, buttons LE32, six axes,
 * flags (bit 0: stop), reserved byte. Never cast network data to structs. */
static inline int rg_decode(const unsigned char *p, size_t n,
                            const unsigned char token[16], pad_state *st,
                            uint32_t *seq, int *stop)
{
    if (n != RG_PACKET_SIZE || memcmp(p, "RGP1", 4) ||
        memcmp(p + 4, token, 16) || (p[34] & ~1u) || p[35]) return 0;
    *seq = (uint32_t)p[20] | (uint32_t)p[21] << 8 |
           (uint32_t)p[22] << 16 | (uint32_t)p[23] << 24;
    pad_state_reset(st);
    st->buttons = (uint32_t)p[24] | (uint32_t)p[25] << 8 |
                  (uint32_t)p[26] << 16 | (uint32_t)p[27] << 24;
    if (st->buttons & ~RG_BUTTON_MASK) return 0;
    st->lx=p[28]; st->ly=p[29]; st->rx=p[30]; st->ry=p[31];
    st->l2=p[32]; st->r2=p[33]; *stop=p[34];
    return 1;
}
static inline int rg_newer(uint32_t seq, uint32_t previous)
{
    uint32_t delta = seq - previous;
    return delta != 0 && delta < 0x80000000u;
}
#endif
