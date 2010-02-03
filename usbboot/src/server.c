#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "mem.h"
#include "cmd.h"
#include "ingenic_usb.h"
#include "ingenic_cfg.h"

#define PORT 1338

#define TYPE_READ8      0
#define TYPE_READ16     1
#define TYPE_READ32     2
#define TYPE_WRITE8     3
#define TYPE_WRITE16    4
#define TYPE_WRITE32    5

struct request {
    uint32_t addr;
    uint32_t value;
    uint8_t type;
};

struct response {
    uint32_t value;
};

#define CONFIG_FILE_PATH "/etc/xburst-tools/usbboot.cfg"

struct ingenic_dev ingenic_dev;

int main(int argc, char *argv[])
{
    int s, t;
    unsigned int sinlen;
    struct sockaddr_in sin;
    struct request request;
    struct response response;
	char *cfgpath = CONFIG_FILE_PATH;

	if (usb_ingenic_init(&ingenic_dev))
	 	return EXIT_FAILURE;

	if (parse_configure(&ingenic_dev.config, cfgpath))
		return EXIT_FAILURE;

    boot(&ingenic_dev, STAGE1_FILE_PATH, STAGE2_FILE_PATH);

    if ( (s = socket(AF_INET, SOCK_STREAM, 0 ) ) < 0) {
      return -1;
    }

    sin.sin_family = AF_INET;              /*set protocol family to Internet */
    sin.sin_port = htons(PORT);  /* set port no. */
    sin.sin_addr.s_addr = INADDR_ANY;   /* set IP addr to any interface */

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin) ) < 0 ){
         perror("bind"); return -1;  /* bind error */
    }

    /* server indicates it's ready, max. listen queue is 5 */
    if (listen(s, 5)) {
       return -1;
    }

    sinlen = sizeof(sin);
    if ( (t = accept(s, (struct sockaddr *) &sin, &sinlen) ) < 0 ){
        perror("accept ");  /* accpet error */
        return -1;
    }

    while (1) {

        if (recv(t, (char*)&request, sizeof(request), 0) < 0) {
                perror("recv");
                return -1;
        }

/*        printf("Got request %x %x %x\n", request.type, request.addr,
        request.value);*/

        switch (request.type) {
        case TYPE_READ8:
            response.value = mem_read8(&ingenic_dev, request.addr);
            break;
        case TYPE_READ16:
            response.value = mem_read16(&ingenic_dev, request.addr);
            break;
        case TYPE_READ32:
            response.value = mem_read32(&ingenic_dev, request.addr);
            break;
        case TYPE_WRITE8:
            mem_write8(&ingenic_dev, request.addr, request.value);
            break;
        case TYPE_WRITE16:
            mem_write16(&ingenic_dev, request.addr, request.value);
            break;
        case TYPE_WRITE32:
            mem_write32(&ingenic_dev, request.addr, request.value);
            break;
        }
/*        printf("Got response %x\n", response.value);*/

        if (send(t, (char*)&response, sizeof(response),0 ) < 0 ) {
            perror("send");
            return -1;
        }
    }

    close(t);
    close(s);

    return 0;
}
