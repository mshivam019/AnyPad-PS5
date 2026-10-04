/* GPL-3.0-or-later. Linux receiver -> UDP -> AnyPad virtual DualSense.
 * Does not open USB or Bluetooth devices on the PS5. */
#include "protocol.h"
#include "pairing.h"
#include "ps5_vpad.h"
#include "ps5_power.h"
#include "lock.h"
#include "log.h"
#include "util.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;
static void on_signal(int sig) { (void)sig; stopping = 1; }
static int private_peer(uint32_t raw)
{
    uint32_t ip = ntohl(raw);
    return (ip >> 24) == 10 || (ip >> 20) == 0xac1 ||
           (ip >> 16) == 0xc0a8 || (ip >> 24) == 127;
}
int main(void)
{
    int fd=-1, created=0, timed_out=0, have_peer=0, result=1;
    long last=0, created_at=0;
    uint32_t previous=0;
    struct sockaddr_in addr={0}, peer={0};
    pad_state current;
    mkdir("/data/redgear", 0755);
    if (!lock_take("/data/redgear/bridge.lock")) return 1;
    log_open("/data/redgear/bridge.log");
    signal(SIGINT, on_signal); signal(SIGTERM, on_signal);
    pad_state_reset(&current);
    fd=socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family=AF_INET; addr.sin_port=htons(RG_PORT);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0 ||
        fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        log_line("UDP setup failed: errno %d", errno); goto done;
    }
    if (!vpad_init()) { notify("Redgear: virtual pad init failed; see /data/redgear/bridge.log"); goto done; }
    notify("Redgear prototype: listening on UDP 8096 (firmware 9.00 unverified)");
    log_line("Redgear network prototype started, UDP %d", RG_PORT);
    while (!stopping) {
        long now=now_ms();
        int power=power_state();
        if (power == POWER_GOING_TO_REST || power == POWER_IN_REST ||
            access("/data/redgear/stop", F_OK) == 0) break;
        /* Limit work per tick: malformed traffic cannot starve timeout checks. */
        for (int i=0; i<32; i++) {
            unsigned char packet[RG_PACKET_SIZE+1], ack[8]={'R','G','A','1',0,0,0,0};
            struct sockaddr_in from={0}; socklen_t len=sizeof from;
            uint32_t seq; int stop; pad_state state;
            ssize_t n=recvfrom(fd,packet,sizeof packet,0,(struct sockaddr *)&from,&len);
            if (n<0) {
                if (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) stopping=1;
                break;
            }
            if (!private_peer(from.sin_addr.s_addr) ||
                !rg_decode(packet,(size_t)n,rg_token,&state,&seq,&stop)) continue;
            if (have_peer && (peer.sin_addr.s_addr!=from.sin_addr.s_addr ||
                              peer.sin_port!=from.sin_port || !rg_newer(seq,previous))) continue;
            if (!have_peer) { peer=from; have_peer=1; log_line("Sender connected: %s",inet_ntoa(peer.sin_addr)); }
            previous=seq; last=now; timed_out=0; current=state;
            if (stop) stopping=1;
            if (!created && !stop) {
                created=vpad_add(0);
                created_at=now;
                if (!created) { log_line("Could not queue virtual pad"); stopping=1; }
            }
            ack[4]=(unsigned char)(vpad_live(0)?1:0);
            sendto(fd,ack,sizeof ack,0,(struct sockaddr *)&from,sizeof from);
        }
        vpad_poll(now);
        if (created && !vpad_live(0) && now-created_at>4500) {
            log_line("Virtual pad did not become ready; stopping instead of retrying");
            notify("Redgear: virtual pad failed; retrieve /data/redgear/bridge.log");
            break;
        }
        if (have_peer && now-last>250 && !timed_out) {
            pad_state_reset(&current); timed_out=1;
            log_line("Input timeout: releasing all controls");
        }
        if (have_peer && now-last>2000) have_peer=0;
        if (created && vpad_live(0)) vpad_update(0,&current);
        usleep(4000);
    }
    result=0;
done:
    /* Allow an in-flight virtual device identification to finish before cleanup. */
    if (created) {
        pad_state_reset(&current); vpad_update(0,&current);
        for (int i=0;i<900 && !vpad_live(0);i++) { vpad_poll(now_ms()); usleep(4000); }
        vpad_remove(0);
    }
    vpad_creds_restore();
    if (fd>=0) close(fd);
    unlink("/data/redgear/stop");
    log_line("Redgear prototype stopped"); log_close(); lock_release();
    return result;
}
