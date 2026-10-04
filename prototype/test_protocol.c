#include "protocol.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    unsigned char token[16]={0}, p[RG_PACKET_SIZE]={0};
    pad_state st; uint32_t seq; int stop;
    memcpy(p,"RGP1",4); p[20]=7; p[25]=0x40;
    p[28]=128; p[29]=0; p[30]=255; p[31]=128; p[32]=42;
    assert(rg_decode(p,sizeof p,token,&st,&seq,&stop));
    assert(seq==7 && st.buttons==PAD_CROSS && st.lx==128 && st.ly==0 && st.rx==255 && st.l2==42);
    for(size_t n=0;n<sizeof p;n++) assert(!rg_decode(p,n,token,&st,&seq,&stop));
    assert(!rg_decode(p,sizeof p+1,token,&st,&seq,&stop));
    p[4]=1; assert(!rg_decode(p,sizeof p,token,&st,&seq,&stop)); p[4]=0;
    p[34]=2; assert(!rg_decode(p,sizeof p,token,&st,&seq,&stop)); p[34]=1;
    assert(rg_decode(p,sizeof p,token,&st,&seq,&stop) && stop);
    p[27]=0x80; assert(!rg_decode(p,sizeof p,token,&st,&seq,&stop));
    assert(rg_newer(0,UINT32_MAX)); assert(!rg_newer(1,1)); assert(!rg_newer(1,2));
    puts("Packet validation, mapping and sequence rollover passed");
}
