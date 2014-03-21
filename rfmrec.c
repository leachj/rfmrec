#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <curl/curl.h>

#include <rfm12b_ioctl.h>
#include <rfm12b_jeenode.h>

#define RF12_DEV "/dev/rfm12b.0.1"
#define JEENODE_ID      2
#define JEENODE_GROUP      210

// set the URL to make request to, This can be direct to emoncms or to something like nodered
// for emoncms use this: define URL "http://emoncms.org/input/post.json?json={%s:%d}&apikey=<API KEY>"
// point to node red
#define URL "http://192.168.1.70:1880/sensor?node=%d&type=%s&value=%d"

#define RF12_MAX_RLEN   128
#define RF12_MAX_SLEN   66

typedef struct _Payload {
    short temp;	// Temperature reading
    short light;	// Light reading
    short supplyV;	// Supply voltage
    short pir;	// Supply voltage
} Payload;

static volatile int running;

void sig_handler(int signum)
{
    signal(signum, SIG_IGN);
    running = 0;
}

void uploadValue(CURL* curl,int node, char* name, short value)
{
    char buf[1024];
    CURLcode res;
   
    snprintf(buf,1024,URL,node,name,value);
    curl_easy_setopt(curl, CURLOPT_URL, buf);
    res = curl_easy_perform(curl);
    if(res != CURLE_OK){
        fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
    }
    
}

int main(int argc, char** argv)
{
    int rfm12_fd, len, i, jee_id = JEENODE_ID,
    band_id, group_id = JEENODE_GROUP, bit_rate, ioctl_err, has_ack, has_ctl, is_dst,
    jee_addr, jee_len, send_ack;
    char* devname, obuf[RF12_MAX_RLEN+1], c;
    fd_set fds;
    
    devname = RF12_DEV;
    
    rfm12_fd = open(devname, O_RDWR);
    if (rfm12_fd < 0) {
        printf("\nfailed to open %s: %s.\n\n", devname, strerror(errno));
        return rfm12_fd;
    } else {
        printf(
               "\nsuccessfully opened %s as fd %i.\n\n",
               devname, rfm12_fd
               );
    }
    
    fflush(stdout);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    
    send_ack = 1;
    ioctl_err = 0;
    ioctl_err |= ioctl(rfm12_fd, RFM12B_IOCTL_GET_BAND_ID, &band_id);
    ioctl_err |= ioctl(rfm12_fd, RFM12B_IOCTL_GET_BIT_RATE, &bit_rate);
    ioctl_err |= ioctl(rfm12_fd, RFM12B_IOCTL_SET_GROUP_ID, &group_id);
    ioctl_err |= ioctl(rfm12_fd, RFM12B_IOCTL_SET_JEEMODE_AUTOACK, &send_ack);
    ioctl_err |= ioctl(rfm12_fd, RFM12B_IOCTL_SET_JEE_ID, &jee_id);   
    if (ioctl_err) {
        printf("\nioctl() error: %s.\n", strerror(errno));
        return -1;
    }
    
    printf("RFM12B configured to GROUP %i, BAND %i, BITRATE: 0x%.2x, JEE ID: %d\n\n",
           group_id, band_id, bit_rate, jee_id);
    
    
    running = 1;
    do {
        
            len = read(rfm12_fd, obuf, RF12_MAX_RLEN);
            
            if (len < 0) {
                printf("\nerror while receiving: %s.\n\n",
                       strerror(errno));
                return -1;
            } else if (len > 0) {
                
                
                has_ack = (RFM12B_JEE_HDR_ACK_BIT & obuf[0]) ? 1 : 0;
                has_ctl = (RFM12B_JEE_HDR_CTL_BIT & obuf[0]) ? 1 : 0;
                is_dst = (RFM12B_JEE_HDR_DST_BIT & obuf[0]) ? 1 : 0;
                jee_addr = RFM12B_JEE_ID_FROM_HDR(obuf[0]);
                jee_len = obuf[1];
                
                printf("<RECV CTL:%d ACK:%d DST:%d ADDR:%d LEN:%d>\n",
                       has_ctl, has_ack, is_dst, jee_addr, jee_len);
                
                Payload    *payload;
                payload = (Payload *) &obuf[2];
                
                printf("temp is %d light is %d pir is %d voltage is %d\n",(*payload).temp, (*payload).light,(*payload).pir, (*payload).supplyV);
                
                CURL *curl;
                
                curl = curl_easy_init();
                if(curl) {
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    
                    uploadValue(curl,jee_addr, "temp", (*payload).temp);
                    uploadValue(curl,jee_addr, "light", (*payload).light);
                    uploadValue(curl,jee_addr, "volts", (*payload).supplyV);
                    uploadValue(curl,jee_addr, "pir", (*payload).pir);
                    
                    curl_easy_cleanup(curl);
                }
            }
        } while (running);
    
    
    close(rfm12_fd);
    
    return 0;
}
