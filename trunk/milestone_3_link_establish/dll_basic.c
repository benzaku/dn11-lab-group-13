#include "dll_basic.h"
#include "stdlib.h"
#include "limits.h"
#include "cnet.h"
#include "string.h"
#include "queue.h"
/* THIS FILE PROVIDES A MINIMAL RELIABLE DATALINK LAYER.  IT AVOIDS ANY
 FRAME LOSS AND CORRUPTION AT THE PHYSICAL LAYER BY CALLING
 CNET_write_physical_reliable() INSTEAD OF CNET_write_physical().
 BECAUSE "NOTHING CAN GO WRONG", WE DON'T NEED TO MANAGE ANY SEQUENCE
 NUMBERS OR BUFFERS OF FRAMES IN THIS LAYER, AND OUR DLL_FRAME
 STRUCTURE CAN CONSIST OF JUST ITS PAYLOAD (THE NL's PACKETS).
 */

static CnetTimerID lasttimer = NULLTIMER;

typedef struct {
	/* AS WE USE A RELIABLE DATALINK, WE DON'T NEED ANY OTHER FIELDS */
	char packet[MAX_FRAME_SIZE];
} DLL_FRAME;

//static BUF_ELMT *BUFFER = NULL;
//static BUF_ELMT *TAIL = NULL;
//static int BUF_SIZE = 0;
//static BUF_ELMT *LAST_BUF = NULL;

int compare(char * str1, size_t len1, char * str2, size_t len2) {
	if (len1 != len2) {
		printf("different length");
		return 0;
	}
	int i;
	for (i = 0; i < len1; i++) {
		if (str1[i] != str2[i])
			return 0;
	}
	return 1;
}

struct queueLK buf;

//static int count = 0;
//static int count_send = 0;
/*  down_to_datalink() RECEIVES PACKETS FROM THE NETWORK LAYER (ABOVE) */
static struct elemType temp;
int down_to_datalink(int link, char *packet, size_t length) {

	extern void printmsg(char *, size_t);

	//struct elemType temp;
	temp.length = length;
	temp.link = link;
	temp.packet = malloc(length);
	memcpy(temp.packet, packet, length);

	if(temp.length == 0){
		printf("==============\n");
		printf("length = 0!!!\n");
		printf("==============\n");
	}

	enQueue(&buf, temp);
	//printf("buf size = %d\n", buf.size);
	//free(packet);
	//free(&temp);

	return (0);
}

/*  up_to_datalink() RECEIVES FRAMES FROM THE PHYSICAL LAYER (BELOW) AND,
 KNOWING THAT OUR PHYSICAL LAYER IS RELIABLE, IMMEDIATELY SENDS THE
 PAYLOAD (A PACKET) UP TO THE NETWORK LAYER.
 */
static EVENT_HANDLER( up_to_datalink) {
	extern int up_to_network(char *packet, size_t length, int arrived_on);
	extern void printmsg(char *, size_t);
	DLL_FRAME f;
	size_t length;
	int link;

	//printf("Frame from physical layer\n");

	length = sizeof(DLL_FRAME);
	if(CNET_read_physical(&link, (char *) &f, &length )!= 0){
		printf("read phy err\n");
	}
	//printf("DLL frame \n");
	//printmsg((char*) &f, length);
	CHECK(up_to_network(f.packet, length, link));
}

static void timeouts(CnetEvent ev, CnetTimerID timer, CnetData data) {
	extern void printmsg(char *, size_t);
	CnetTime timeout;
	//printf("timer 1 restart! queue length = %d\n", buf.size);
	//if (timer == lasttimer) {

		if (buf.size > 0) {
			//printf("%d elements\n", buf.size);
			struct elemType temp = peekQueue(&buf);

			timeout = temp.length * (CnetTime) 8000000
					/ linkinfo[temp.link].bandwidth;
			//printf("timeout = %d", timeout);
			if(timeout == 0)
				timeout = 64000;
			lasttimer = CNET_start_timer(EV_TIMER1, 1.05 * timeout, 0);
			size_t len = temp.length;
			//printf("ready to write physical!\n");
			if(CNET_write_physical(temp.link, temp.packet, &len) !=0 )
			{
			printf("write phy err!\n");
			//(temp.packet, len);
			}

			outQueue(&buf);
			//printf("buf size after delete = %d\n", buf.size);

		} else {
			//printf("no element in queue\n");
			timeout = 50000;
			lasttimer = CNET_start_timer(EV_TIMER1, timeout, 0);
		}
	//}

	//printf("timer stop!\n");
}

void reboot_DLL(void) {
	//CnetTime timeout;
	CHECK(CNET_set_handler(EV_PHYSICALREADY, up_to_datalink, 0));
	CHECK(CNET_set_handler(EV_TIMER1, timeouts, 0));
	initQueue(&buf);
	//printf("buf init size = %d\n", buf.size);

	//char * a = "abcde";
	//printf("size of a: %d\n", sizeof(a));

	CnetTime timeout;

	timeout = 10000;
	lasttimer = CNET_start_timer(EV_TIMER1, timeout, 0);

	/* NOTHING ELSE TO DO! */
}
