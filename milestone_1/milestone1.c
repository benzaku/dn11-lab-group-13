#include <cnet.h>
#include <stdlib.h>
#include <string.h>
/*
 *	milstone1.c
 *	last update 23:28 02.07.2011 Haiyang Xu benzaku@gmail.com
 */

typedef enum    { DL_DATA, DL_ACK }   FRAMEKIND;

typedef struct {
    char        data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
    FRAMEKIND    kind;          /* only ever DL_DATA or DL_ACK */
    size_t len;           /* the length of the msg field only */
    int          checksum;      /* checksum of the whole frame */
    int          seq;           /* only ever 0 or 1 */
    MSG          msg;
} FRAME;

#define FRAME_HEADER_SIZE  (sizeof(FRAME) - sizeof(MSG))
#define FRAME_SIZE(f)      (FRAME_HEADER_SIZE + f.len)
#define	CN_RED				"red"

static  MSG             *lastmsg;
static  size_t		    lastlength              = 0;
static  CnetTimerID     lasttimer               = NULLTIMER;

static  int             ackexpected             = 0;
static  int             nextframetosend         = 0;
static  int             frameexpected           = 0;


static void transmit_frame(MSG *msg, FRAMEKIND kind,
                           size_t length, int seqno)
{
    FRAME       f;
    int         link = 1;

    f.kind      = kind;
    f.seq       = seqno;
    f.checksum  = 0;
    f.len       = length;
	CnetTime	timeout;
    switch (kind) {
    case DL_ACK :
		break;

    case DL_DATA: {

        //printf(" DATA transmitted, seq=%d\n", seqno);
        memcpy(&f.msg, (char *)msg, (int)length);

        timeout = FRAME_SIZE(f)*((CnetTime)8000000 / linkinfo[link].bandwidth) +
                                linkinfo[link].propagationdelay;

        lasttimer = CNET_start_timer(EV_TIMER1, 1.005 * timeout, 0);
        break;
      }
    }
    length      = FRAME_SIZE(f);
    CHECK(CNET_write_physical(link, (char *)&f, &length));
}


static void application_ready(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    CnetAddr destaddr;

    lastlength  = sizeof(MSG);
    CHECK(CNET_read_application(&destaddr, (char *)lastmsg, &lastlength));
    CNET_disable_application(ALLNODES);

    //printf("down from application, seq=%d\n", nextframetosend);
    transmit_frame(lastmsg, DL_DATA, lastlength, nextframetosend);
    nextframetosend = 1-nextframetosend;
}


static void physical_ready(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    FRAME        f;
    size_t		 len;
    int          link, checksum;

    len         = sizeof(FRAME);
    CHECK(CNET_read_physical(&link, (char *)&f, &len));

    checksum    = f.checksum;
    f.checksum  = 0;

    switch (f.kind) {
    case DL_ACK :
        break;

    case DL_DATA :
        //printf("\t\t\t\tDATA received, seq=%d, ", f.seq);
        //if(f.seq == frameexpected) {
            //printf("up to application\n");
            len = f.len;
            CHECK(CNET_write_application((char *)&f.msg, &len));
            frameexpected = 1-frameexpected;
        //}
        //else
            //printf("ignored\n");
		break;
    }
}


static void draw_frame(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    CnetDrawFrame *df   = (CnetDrawFrame *)data;
    FRAME         *f    = (FRAME *)df->frame;

    switch (f->kind) {
    case DL_ACK :
		df->nfields		 = 1;
        df->colours[0]   = (f->seq == 0) ? "red" : "purple";
        df->pixels[0]   = 10;
        sprintf(df->text, "%d", f->seq);
        break;

    case DL_DATA :
		df->nfields		 = 2;
        df->colours[0]   = (f->seq == 0) ? "red" : "purple";
        df->pixels[0]   = 10;
        df->colours[1]   = "green";
        df->pixels[1]   = 30;
        sprintf(df->text, "data=%d", f->seq);
        break;
    }
}


static void timeouts(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    if(timer == lasttimer) {
        //printf("timeout, seq=%d\n", ackexpected);
    	CNET_enable_application(ALLNODES);
	}
}


static void showstate(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    printf(
    "\n\tackexpected\t= %d\n\tnextframetosend\t= %d\n\tframeexpected\t= %d\n",
                    ackexpected, nextframetosend, frameexpected);
}


void reboot_node(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    if(nodeinfo.nodenumber > 1) {
        fprintf(stderr,"This is not a 2-node network!\n");
        exit(1);
    }

    lastmsg     = malloc(sizeof(MSG));

    CHECK(CNET_set_handler( EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler( EV_PHYSICALREADY,    physical_ready, 0));
    CHECK(CNET_set_handler( EV_DRAWFRAME,        draw_frame, 0));
    CHECK(CNET_set_handler( EV_TIMER1,           timeouts, 0));
    CHECK(CNET_set_handler( EV_DEBUG0,           showstate, 0));

    CHECK(CNET_set_debug_string( EV_DEBUG0, "State"));

//    if(nodeinfo.nodenumber == 0)
        CNET_enable_application(ALLNODES);
}
