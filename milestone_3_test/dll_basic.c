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

        printf("down_to_datalink\n");
        printf("length down_to_datalink %d\n", length);
	extern void printmsg(char *, size_t);

        
	//struct elemType temp;

        temp.length = length;
	temp.link = link;
	temp.packet = malloc(length);
	memcpy(temp.packet, packet, length);

	enQueue(&buf, temp);
	//free(packet);
	//free(&temp);

	return (0);
}

/*  up_to_datalink() RECEIVES FRAMES FROM THE PHYSICAL LAYER (BELOW) AND,
 KNOWING THAT OUR PHYSICAL LAYER IS RELIABLE, IMMEDIATELY SENDS THE
 PAYLOAD (A PACKET) UP TO THE NETWORK LAYER.
 */
static EVENT_HANDLER( up_to_datalink) {
        printf("up_to_datalink\n");
	extern int up_to_network(char *packet, size_t length, int arrived_on);
	extern void printmsg(char *, size_t);
	
        DLL_FRAME f;
	size_t length;
	int link;

	//printf("Frame from physical layer\n");

	length = sizeof(DLL_FRAME);
	CHECK(CNET_read_physical(&link, (char *) &f, &length));
	//printf("DLL frame : %s\n", (char *) &f);
// 	printmsg((char*) &f, length);
        printf("length up_to_datalink %d\n", length);
	CHECK(up_to_network(f.packet, length, link));
}

static void timeouts(CnetEvent ev, CnetTimerID timer, CnetData data) {
        printf("timeouts\n");
	extern void printmsg(char *, size_t);
	CnetTime timeout;
	if (timer == lasttimer) {

		if (buf.size > 0) {

			struct elemType temp = peekQueue(&buf);

			timeout = temp.length * (CnetTime) 8000000
					/ linkinfo[temp.link].bandwidth;
			lasttimer = CNET_start_timer(EV_TIMER1, 1.05 * timeout, 0);
			size_t len = temp.length;
                        printf("length timeouts %d\n", len);
			CHECK(CNET_write_physical_reliable(temp.link, temp.packet, &len));
                        printf("write_physical\n");
			//(temp.packet, len);

			outQueue(&buf);
			//printf("buf size after delete = %d\n", buf.size);

		} else {

			timeout = 5000;
			lasttimer = CNET_start_timer(EV_TIMER1, timeout, 0);
		}
	}

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
