#include "report.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 unsigned char p[20]={0,20}; pad_state s;
 assert(rg_report(p,20,&s)); assert(s.lx==128 && s.ly==128 && !s.buttons);
 p[3]=0xf3; p[4]=255; p[5]=128;
 assert(rg_report(p,20,&s)); assert(s.buttons==(PAD_L1|PAD_R1|PAD_CROSS|PAD_CIRCLE|PAD_SQUARE|PAD_TRIANGLE|PAD_L2|PAD_R2));
 p[6]=0; p[7]=0x80; p[8]=0xff; p[9]=0x7f;
 p[10]=0xff; p[11]=0x7f; p[12]=0; p[13]=0x80;
 assert(rg_report(p,20,&s)); assert(s.lx==0 && s.ly==0 && s.rx==255 && s.ry==255);
 p[2]=0x30; p[3]=0; p[4]=p[5]=0;
 assert(rg_report(p,20,&s)); assert(s.buttons==PAD_PS);
 p[2]=0x60; assert(rg_report(p,20,&s)); assert(s.buttons==PAD_TOUCHPAD);
 for(unsigned n=0;n<20;n++) assert(!rg_report(p,n,&s));
 p[0]=1; assert(!rg_report(p,20,&s)); p[0]=0; p[1]=19; assert(!rg_report(p,20,&s));
 puts("USB report tests passed");
}
