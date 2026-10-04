/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef REDGEAR_REPORT_H
#define REDGEAR_REPORT_H
#include "pad.h"
#include <stddef.h>
static inline unsigned rg_axis(const unsigned char *p, int invert) {
    int v=(int16_t)((unsigned)p[0]|((unsigned)p[1]<<8));
    if (invert) v=-v;
    v=(v+32768)/256;
    return v>255?255:(unsigned)v;
}
static inline int rg_report(const unsigned char *p, size_t n, pad_state *s) {
    if(n!=20 || p[0]!=0 || p[1]!=20) return 0;
    pad_state_reset(s);
    static const unsigned lo[8]={PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT,PAD_OPTIONS,PAD_CREATE,PAD_L3,PAD_R3};
    static const unsigned hi[8]={PAD_L1,PAD_R1,PAD_PS,0,PAD_CROSS,PAD_CIRCLE,PAD_SQUARE,PAD_TRIANGLE};
    for(int i=0;i<8;i++) { if(p[2]&(1<<i)) s->buttons|=lo[i]; if(p[3]&(1<<i)) s->buttons|=hi[i]; }
    s->l2=p[4]; s->r2=p[5];
    if(s->l2) s->buttons|=PAD_L2;
    if(s->r2) s->buttons|=PAD_R2;
    s->lx=rg_axis(p+6,0); s->ly=rg_axis(p+8,1); s->rx=rg_axis(p+10,0); s->ry=rg_axis(p+12,1);
    if((s->buttons&(PAD_CREATE|PAD_OPTIONS))==(PAD_CREATE|PAD_OPTIONS))
        s->buttons=(s->buttons&~(PAD_CREATE|PAD_OPTIONS))|PAD_PS;
    if((s->buttons&(PAD_CREATE|PAD_L3))==(PAD_CREATE|PAD_L3))
        s->buttons=(s->buttons&~(PAD_CREATE|PAD_L3))|PAD_TOUCHPAD;
    return 1;
}
#endif
