/* SPDX-License-Identifier: GPL-3.0-or-later
 * Redgear USB receiver -> AnyPad virtual DualSense. */
#include "report.h"
#include "ps5_vpad.h"
#include "ps5_power.h"
#include "lock.h"
#include "log.h"
#include "util.h"
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static volatile sig_atomic_t stopping;
static void on_signal(int s) { (void)s; stopping=1; }
typedef struct {
    int fd, initialized;
    struct usb_fs_endpoint ep[2];
    void *ptr[2][1]; uint32_t len[2][1]; unsigned char buf[2][64];
} receiver;
static void usb_close(receiver *r) {
    if(r->fd<0) return;
    if(r->initialized) { struct usb_fs_uninit u={0}; ioctl(r->fd,USB_FS_UNINIT,&u); }
    close(r->fd); r->fd=-1; r->initialized=0;
}
static int start(receiver *r,int i,unsigned n) {
    r->len[i][0]=n; r->ep[i].nFrames=1; r->ep[i].aFrames=0; r->ep[i].status=0;
    struct usb_fs_start s={.ep_index=(uint8_t)i};
    return ioctl(r->fd,USB_FS_START,&s)==0;
}
static int usb_open(receiver *r) {
    DIR *d=opendir("/dev"); struct dirent *e;
    if(!d) return 0;
    while((e=readdir(d))) {
        if(strncmp(e->d_name,"ugen",4)) continue;
        char path[300]; snprintf(path,sizeof path,"/dev/%s",e->d_name);
        int fd=open(path,O_RDWR); if(fd<0) continue;
        struct usb_device_descriptor dd;
        if(ioctl(fd,USB_GET_DEVICE_DESC,&dd) || UGETW(dd.idVendor)!=0x045e || UGETW(dd.idProduct)!=0x028e) { close(fd); continue; }
        log_line("Redgear found at %s (045e:028e)",path);
        r->fd=fd; break;
    }
    closedir(d); if(r->fd<0) return 0;
    unsigned char cfg[1024]; unsigned in=0,out=0; int match=0;
    struct usb_gen_descriptor gd={0}; gd.ugd_data=cfg; gd.ugd_maxlen=sizeof cfg; gd.ugd_config_index=0xff;
    if(ioctl(r->fd,USB_GET_FULL_DESC,&gd) || gd.ugd_actlen>sizeof cfg) goto fail;
    for(unsigned o=0;o+2<=gd.ugd_actlen;) {
        unsigned n=cfg[o]; if(n<2 || o+n>gd.ugd_actlen) goto fail;
        if(cfg[o+1]==4 && n>=9) match=cfg[o+3]==0 && cfg[o+5]==0xff && cfg[o+6]==0x5d && cfg[o+7]==1;
        if(match && cfg[o+1]==5 && n>=7 && (cfg[o+3]&3)==3) {
            if(cfg[o+2]&0x80) in=cfg[o+2]; else out=cfg[o+2];
        }
        o+=n;
    }
    if(!in) { log_line("No Xbox 360 interrupt input interface"); goto fail; }
    for(int i=0;i<2;i++) {
        r->ptr[i][0]=r->buf[i]; r->ep[i].ppBuffer=r->ptr[i]; r->ep[i].pLength=r->len[i];
        r->ep[i].flags=USB_FS_FLAG_SINGLE_SHORT_OK; r->ep[i].nFrames=1;
    }
    struct usb_fs_init init={.pEndpoints=r->ep,.ep_index_max=2};
    if(ioctl(r->fd,USB_FS_INIT,&init)) goto fail;
    r->initialized=1;
    struct usb_fs_open op={.ep_index=0,.ep_no=(uint8_t)in,.max_bufsize=64,.max_frames=1};
    if(ioctl(r->fd,USB_FS_OPEN,&op) || !start(r,0,64)) goto fail;
    if(out) {
        op=(struct usb_fs_open){.ep_index=1,.ep_no=(uint8_t)out,.max_bufsize=64,.max_frames=1};
        if(!ioctl(r->fd,USB_FS_OPEN,&op)) {
            r->buf[1][0]=1; r->buf[1][1]=3; r->buf[1][2]=6;
            start(r,1,3); /* Xbox player-one steady LED. */
        }
    }
    log_line("USB ready: input %#x output %#x",in,out); return 1;
fail:
    log_line("USB initialization failed errno %d",errno); usb_close(r); return 0;
}
static int read_pad(receiver *r,pad_state *s) {
    int updated=0;
    for(int count=0;count<32;count++) {
        struct usb_fs_complete c={0};
        if(ioctl(r->fd,USB_FS_COMPLETE,&c)) {
            if(errno==EAGAIN || errno==EBUSY || errno==EINTR) return updated;
            log_line("USB completion error %d",errno); return -1;
        }
        if(c.ep_index>1) return -1;
        if(c.ep_index==1) continue;
        if(r->ep[0].status) { log_line("USB input status %d",r->ep[0].status); return -1; }
        if(r->ep[0].aFrames && r->len[0][0]<=64 && rg_report(r->buf[0],r->len[0][0],s)) updated=1;
        if(!start(r,0,64)) return -1;
    }
    return updated;
}
int main(void) {
    receiver r={.fd=-1}; pad_state state; int created=0,ready=0; long created_at=0,next_scan=0; unsigned reports=0;
    mkdir("/data/redgear",0755);
    if(!lock_take("/data/redgear/bridge.lock")) return 1;
    log_open("/data/redgear/usb.log");
    signal(SIGINT,on_signal); signal(SIGTERM,on_signal);
    unlink("/data/redgear/stop"); pad_state_reset(&state);
    if(!vpad_init()) goto done;
    notify("Redgear USB: connect receiver and switch controller on");
    while(!stopping) {
        long now=now_ms(); int power=power_state();
        if(power==POWER_GOING_TO_REST || power==POWER_IN_REST || access("/data/redgear/stop",F_OK)==0) break;
        if(r.fd<0 && now>=next_scan) { usb_open(&r); next_scan=now+2000; }
        if(r.fd>=0) {
            int got=read_pad(&r,&state);
            if(got<0) { usb_close(&r); pad_state_reset(&state); log_line("Receiver disconnected; controls released"); }
            if(got>0) {
                if(!reports++) log_line("First valid controller report");
                if(!created) { created=vpad_add(0); created_at=now; if(!created) break; }
            }
        }
        vpad_poll(now);
        if(created && vpad_live(0)) {
            vpad_update(0,&state);
            if(!ready) { ready=1; notify("Redgear USB connected"); log_line("READY: direct USB virtual pad"); }
        } else if(created && now-created_at>4500) { log_line("Virtual pad failed to become ready"); break; }
        usleep(4000);
    }
done:
    usb_close(&r);
    if(created) {
        pad_state_reset(&state); vpad_update(0,&state);
        for(int i=0;i<900 && !vpad_live(0);i++) { vpad_poll(now_ms()); usleep(4000); }
        vpad_remove(0);
    }
    vpad_creds_restore(); unlink("/data/redgear/stop");
    log_line("Stopped after %u reports",reports); log_close(); lock_release(); return 0;
}
