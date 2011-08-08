#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    int         seq;           // seqno of frame within one msg
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
static  int             nextframetosend         = 0; // this denote the seqno of frame within one msg
static  int             frameexpected           = 0;

static char receiveBuffer[MAX_MESSAGE_SIZE];
static int lengthOfReceiveBuffer = 0;
static int nextmsgtosend = 1;


/*
 *      function to handle frame transmition
 */
static void transmit_frame(MSG *msg, FRAMEKIND kind,
                           size_t length, int seqno)
{
    printf("we are in transmit_frame\n");

    FRAME       f;
    int         link = 1;

    f.kind      = kind;
    f.seq       = seqno;
    f.checksum  = 0;
    f.len       = linkinfo[link].mtu - FRAME_HEADER_SIZE;
    f.frameEnd  = 0;
    CnetTime   timeout;

    char *str = msg->data;
  
    switch (kind)
    {
        case DL_ACK:
            break;

        case DL_DATA:  
            if((length - f.seq * f.len) <= f.len){
                str = str + f.seq * f.len;
                f.len = length - f.seq * f.len;
                memcpy(&f.msg, str, f.len);
                f.frameEnd = 1;
                nextmsgtosend = 1;
            }
            else{
                str = str + f.seq * f.len;
                memcpy(&f.msg, str, f.len);
                nextframetosend++;
            }
        
            timeout = FRAME_SIZE(f)*((CnetTime)8000000 / (linkinfo[link].bandwidth * 1024)) + linkinfo[link].propagationdelay;
            lasttimer = CNET_start_timer(EV_TIMER1, timeout, 0);
            break;  
    }
    
    length = FRAME_SIZE(f);
    CHECK(CNET_write_physical(link, (char *)&f, &length));
  
}



/*
 *  function to handle application ready event
 */
static void application_ready(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    printf("we are in application_ready\n");
    if (nextmsgtosend){
        CnetAddr destaddr;
        
        lastlength  = sizeof(MSG);
        CHECK(CNET_read_application(&destaddr, (char *)lastmsg, &lastlength));
        CHECK(CNET_disable_application(ALLNODES));
        nextmsgtosend = 0;
        nextframetosend = 0;
    }
    
    CHECK(CNET_disable_application(ALLNODES));
    
//     strcpy(lastmsg->data, "abcdefgh");
//     lastlength = strlen((char *)lastmsg);
    
    printf("contents of msg to be sent %s\n", (char *)lastmsg->data);
    printf("length of msg to be sent %d\n", lastlength);
    transmit_frame(lastmsg, DL_DATA, lastlength, nextframetosend);
}



/*
 *	function to handle physical ready event
 */
static void physical_ready(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    printf("we are in physical_ready\n");
    FRAME        f;
    size_t       len;
    int          link, checksum;
        
    len         = sizeof(FRAME);
    CHECK(CNET_read_physical(&link, (char *)&f, &len));
    
    checksum    = f.checksum;
    f.checksum  = 0; 
    
    strcat(receiveBuffer,(char *)&f.msg);
    lengthOfReceiveBuffer = lengthOfReceiveBuffer + f.len;

    if(f.frameEnd) {
        switch (f.kind){
            
            case DL_ACK :
                break;

            case DL_DATA :
                
                len = lengthOfReceiveBuffer;
                printf("contens of received msg %s\n", receiveBuffer);
                printf("length of received msg %d\n", len);
                CHECK(CNET_write_application(receiveBuffer, &len));
                strcpy(receiveBuffer,"");
                lengthOfReceiveBuffer = 0;
//                 frameexpected = 1-frameexpected;
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
        CHECK(CNET_enable_application(ALLNODES));
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
