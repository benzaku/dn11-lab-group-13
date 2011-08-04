#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/*
 *	milstone1.c
 *	Finished coding		23:28		02.07.2011
 *						Haiyang 	Xu
 *						benzaku@gmail.com
 *	Refine				14:50 		03.07.2011
 *						Haiyang		Xu
 *						benzaku@gmail.com
 */

typedef enum    { DL_DATA, DL_ACK }   FRAMEKIND;

typedef struct
{
    char        data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct
{
    FRAMEKIND	kind;          /* only ever DL_DATA or DL_ACK */
    size_t 		len;           /* the length of the msg field only */
    int         checksum;      /* checksum of the whole frame */
    int         seq;           /* only ever 0 or 1 */
    int         frameEnd;
    MSG         msg;
} FRAME;

#define FRAME_HEADER_SIZE  (sizeof(FRAME) - sizeof(MSG))
#define FRAME_SIZE(f)      (FRAME_HEADER_SIZE + f.len)
#define	CN_RED				"red"

static  MSG             *lastmsg;
static  size_t		lastlength              = 0;
static  CnetTimerID     lasttimer               = NULLTIMER;

static  int             ackexpected             = 0;
static  int             nextframetosend         = 0;
static  int             frameexpected           = 0;

static char             receiveBuffer[MAX_MESSAGE_SIZE];

/*
 *	function to handle frame transmition
 */
static void transmit_frame(MSG *msg, FRAMEKIND kind,
                           size_t length, int seqno)
{
  FRAME       f;
  int         link = 1;

  f.kind      = kind;
  f.seq       = seqno;
  f.checksum  = 0;
  f.len       = linkinfo[link].mtu - FRAME_HEADER_SIZE;
  f.frameEnd  = 0;
  CnetTime	timeout;
  
  char *str = msg->data;
  size_t frameSize;
  printf("length of msg %d, len of frame %d\n", length, f.len);
  
  CnetTime sumOfTimeout = 0;
  
  while(length > 0){
  
    switch (kind)
    {
      case DL_ACK:
	break;

      case DL_DATA:
      {	 
        if(length < f.len){ 
          memcpy(&f.msg, str, length);
          f.frameEnd = 1;
          printf("case of last frame\n");
          
          timeout = FRAME_SIZE(f)*((CnetTime)8000000 / (linkinfo[link].bandwidth * 1024)) + linkinfo[link].propagationdelay;
          sumOfTimeout = sumOfTimeout + timeout;
          lasttimer = CNET_start_timer(EV_TIMER1, sumOfTimeout, 0);
        }
	else {
	  memcpy(&f.msg, str, f.len);
          str = str + f.len;
          printf("case of frame reduce\n");
          
          timeout = FRAME_SIZE(f)*((CnetTime)8000000 / (linkinfo[link].bandwidth * 1024)) + linkinfo[link].propagationdelay;
          printf("timeout is %d\n", timeout);
          sumOfTimeout = sumOfTimeout + timeout;
	}
	break;	
      }
    }
    
    printf(" last frame? %d\n", f.frameEnd);
    frameSize = FRAME_SIZE(f);
    printf(" frame size %d\n", frameSize);
    CHECK(CNET_write_physical(link, (char *)&f, &frameSize));
    length = length - f.len;
    printf(" remaining length of msg %d\n", length);
    
  }
}

/*
 *  function to handle application ready event
 */
static void application_ready(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    CnetAddr destaddr;
    
    lastlength  = sizeof(MSG);
    CHECK(CNET_read_application(&destaddr, (char *)lastmsg, &lastlength));
    CHECK(CNET_disable_application(ALLNODES));

    transmit_frame(lastmsg, DL_DATA, lastlength, nextframetosend);
    nextframetosend = 1-nextframetosend;
}

/*
 *	function to handle physical ready event
 */
static void physical_ready(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    FRAME        f;
    size_t		 len;
    int          link, checksum;
        
    len         = sizeof(FRAME);
    CHECK(CNET_read_physical(&link, (char *)&f, &len));
    
//     printf("%d", f.frameEnd);

    checksum    = f.checksum;
    f.checksum  = 0; 
    
    strcat(receiveBuffer,(char *)&f.msg);

    if(f.frameEnd < 1) {
      
      switch (f.kind)
      {
      case DL_ACK :
          break;

      case DL_DATA :
        len =  sizeof(receiveBuffer);
        CHECK(CNET_write_application(receiveBuffer, &len));
        strcpy(receiveBuffer,"");
        frameexpected = 1-frameexpected;
        break;
      }
    }

}


static void draw_frame(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    CnetDrawFrame *df   = (CnetDrawFrame *)data;
    FRAME         *f    = (FRAME *)df->frame;

    switch (f->kind)
    {
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

/*
 *	function to handle timeout event
 *	When a frame has been put down to physical layer, it needs time to transmit
 *	During its transmition all applications are disabled to avoid collision
 *	So a timer is started and will expired until the estimated transmitted time
 *	Then applications in all nodes are enabled again
 */
static void timeouts(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    if(timer == lasttimer)
    {
//         CHECK(CNET_enable_application(ALLNODES));
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
    if(nodeinfo.nodenumber > 1)
    {
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

    CHECK(CNET_enable_application(ALLNODES));
}
