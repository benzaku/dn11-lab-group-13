#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "nl_table.h"
#include "dll_basic.h"

#define	MAXHOPS		4

/*  This file implements a much better flooding algorithm than those in
 both flooding1.c and flooding2.c. As Network Layer packets are processed,
 the information in their headers is used to update the NL table.

 The minimum observed hopcount to each (potential) remote destination
 is remembered by the NL table, as is the link on which these packets
 arrive. This link is later used to route packets leaving for that node.

 The routine NL_savehopcount() is called for both NL_DATA and NL_ACK
 packets, and we even "steal" information from Network Layer packets
 that don't belong to us!

 I don't think it's a flooding algorithm any more Toto.

 This flooding algorithm exhibits an efficiency which improves over time
 (as the NL table "learns" more).  Over the 8 nodes in the AUSTRALIA.MAP
 file, the initial efficiency is the same as that of flooding1.c (about
 2%) but as the NL table improves, the efficiency rises to over 64%.
 */

typedef enum {
	NL_DATA, NL_ACK
} NL_PACKETKIND;

typedef struct {
	CnetAddr src;
	CnetAddr dest;
	NL_PACKETKIND kind; /* only ever NL_DATA or NL_ACK */
	int seqno; /* 0, 1, 2, ... */
	int hopcount;
        int checksum;
        size_t pieceNumber;
        int pieceEnd;
	size_t length; /* the length of the msg portion only */
	char msg[MAX_MESSAGE_SIZE];
} NL_PACKET;

#define PACKET_HEADER_SIZE  (sizeof(NL_PACKET) - MAX_MESSAGE_SIZE)
#define PACKET_SIZE(p)	    (PACKET_HEADER_SIZE + p.length)

static char receiveBuffer[255][MAX_MESSAGE_SIZE];
static char *rb[255];


void printmsg(char * msg, size_t length) {
	size_t i;
	printf("msg :");
	for (i = 0; i < length; i++) {
		printf("%d", (unsigned int)msg[i]);
	}
	printf("\n");
}


// here the second parameter is the length of msg of packet!
void sendPacketPiecesToDatalink(char *packet, size_t length, int choose_link){
                printf("sendPacketPiecesToDatalink\n");
    
                size_t maxPacketLength = linkinfo[choose_link].mtu - PACKET_HEADER_SIZE;
                
                NL_PACKET *tempPacket = (NL_PACKET *) packet;
                NL_PACKET piecePacket;
                
                piecePacket.src         = tempPacket->src;
                piecePacket.dest        = tempPacket->dest;
                piecePacket.kind        = tempPacket->kind;
                piecePacket.seqno       = tempPacket->seqno;
                piecePacket.hopcount    = tempPacket->hopcount;
                piecePacket.pieceNumber = tempPacket->pieceNumber;
                piecePacket.pieceEnd    = tempPacket->pieceEnd;
                
                size_t tempLength = length;
                char *str = tempPacket->msg;
                                                
                while(tempLength > maxPacketLength){
                    piecePacket.length = maxPacketLength;
                    memcpy(piecePacket.msg, str, maxPacketLength);
            
                    CHECK(down_to_datalink(choose_link, (char *) &piecePacket, PACKET_SIZE(piecePacket)));
                    piecePacket.pieceNumber = piecePacket.pieceNumber + 1;
                    str = str + piecePacket.pieceNumber * maxPacketLength;
                    tempLength = tempLength - piecePacket.pieceNumber * maxPacketLength;
                }
                
                piecePacket.pieceEnd = 1;
                piecePacket.length = tempLength;
                memcpy(piecePacket.msg, str, tempLength);
                //printf("Required link is provided link = %d\n", choose_link);
                printf("sendPacketPiecesToDatalink %d\n", length);
                CHECK(down_to_datalink(choose_link, (char *) &piecePacket, PACKET_SIZE(piecePacket)));
                //printf("Provided link = %d sent! \n", choose_link);
}

/* ----------------------------------------------------------------------- */

/*  flood3() IS A BASIC ROUTING STRATEGY WHICH TRANSMITS THE OUTGOING PACKET
 ON EITHER THE SPECIFIED LINK, OR ALL BEST-KNOWN LINKS WHILE AVOIDING
 ANY OTHER SPECIFIED LINK.
 */
static void flood3(char *packet, size_t length, int choose_link, int avoid_link) {
    
        printf("flood3\n");
        printf("flood3 %d\n", length);
        NL_PACKET *p = (NL_PACKET *) packet;
 
	/*  REQUIRED LINK IS PROVIDED - USE IT */
	if (choose_link != 0) {
                sendPacketPiecesToDatalink(packet, p->length, choose_link);
	}

	/*  OTHERWISE, CHOOSE THE BEST KNOWN LINKS, AVOIDING ANY SPECIFIED ONE */
	else {
		int links_wanted = NL_linksofminhops(p->dest);
		int link;

		for (link = 1; link <= nodeinfo.nlinks; ++link) {

			if (link == avoid_link) /* possibly avoid this one */{
				//printf("avoid link %d\n", link);
				continue;
			}
			if (links_wanted & (1 << link)) /* use this link if wanted */
			{
                         
                                //printf("semphore=%d \n", get_semphore());

				//printf("in flood3, semphore=%d\n", get_semphore());

				//printf("send to link %d\n", link);
				//printf("send to link %d =========\n", link);
				//printmsg(packet, length);
                                
                                sendPacketPiecesToDatalink(packet, p->length, link);
// 		        	CHECK(down_to_datalink(link, packet, length));
				
				//printf("link = %d sent!\n", link);
                                
			}
		}
	}
}


/*  down_to_network() RECEIVES NEW MESSAGES FROM THE APPLICATION LAYER AND
 PREPARES THEM FOR TRANSMISSION TO OTHER NODES.
 */
static EVENT_HANDLER( down_to_network) {
    
        printf("down_to_network\n");
	NL_PACKET p;

	p.length = sizeof(p.msg);
	//printf("p.length = %d, MAX_MESSAGE_SIZE = %d", p.length, MAX_MESSAGE_SIZE);
	CHECK(CNET_read_application(&p.dest, p.msg, &p.length));
	CNET_disable_application(p.dest);

	p.src = nodeinfo.address;
	p.kind = NL_DATA;
	p.hopcount = 0;
	p.seqno = NL_nextpackettosend(p.dest);
        p.pieceNumber = 0;
        p.pieceEnd = 0;

	//printmsg((char *) &p, PACKET_SIZE(p));

        flood3((char *) &p, PACKET_SIZE(p), 0, 0);
        
}

/*  up_to_network() IS CALLED FROM THE DATA LINK LAYER (BELOW) TO ACCEPT
 A PACKET FOR THIS NODE, OR TO RE-ROUTE IT TO THE INTENDED DESTINATION.
 */
int up_to_network(char *packet, size_t length, int arrived_on_link) {
	NL_PACKET *p = (NL_PACKET *) packet;

	printf("up to network\n");
	++p->hopcount; /* took 1 hop to get here */
	//printf("me = %d, dest = %d =======\n", nodeinfo.address, p->dest);
        /*  IS THIS PACKET IS FOR ME? */
	if (p->dest == nodeinfo.address) {
		printf("it's for me!\n========");
		switch (p->kind) {
		case NL_DATA:
			if (p->seqno == NL_packetexpected(p->src)) {
                            
                                printf("piece for this node arrives\n");
                                length = p->length;
                                memcpy(rb[p->src], (char *) p->msg, length);
                                rb[p->src] = rb[p->src] + length;
				
                                if(p->pieceEnd) {
                                    printf("last piece for this node arrives\n");
                                    CnetAddr tmpaddr;

                                    length = p->pieceNumber * linkinfo[arrived_on_link].mtu + p->length;
                                    printf("length write_application %d\n", length);
//                                     printf("before up to network packet = %s\n", (char *) p);
                                    
                                    CHECK(CNET_write_application(receiveBuffer[p->src], &length));
                                    rb[p->src] = &receiveBuffer[p->src][0];
                                    
                                    inc_NL_packetexpected(p->src);

                                    NL_savehopcount(p->src, p->hopcount, arrived_on_link);

                                    tmpaddr = p->src; /* swap src and dest addresses */
                                    p->src = p->dest;
                                    p->dest = tmpaddr;

                                    p->kind = NL_ACK;
                                    p->hopcount = 0;
                                    p->length = 0;
                                    //printf("right frame! up to app\n");
                                    flood3(packet, PACKET_HEADER_SIZE, arrived_on_link, 0);
                                    //printf("send ack\n");

                                }
			}
			break;

		case NL_ACK:
			if (p->seqno == NL_ackexpected(p->src)) {
				//printf("ACK come!\n");
				inc_NL_ackexpected(p->src);
				NL_savehopcount(p->src, p->hopcount, arrived_on_link);
				CHECK(CNET_enable_application(p->src));
			}
			break;
		default:
			printf("it's nothing!====================\n");
			break;
		}
	}
	/* THIS PACKET IS FOR SOMEONE ELSE */
	else {
		printf("hopcount = %d\n", p->hopcount);
		printf("MAXHOPS = %d\n", MAXHOPS);
		if (p->hopcount < MAXHOPS) { /* if not too many hops... */
			//printf("other's frame!\n");
                        if(p->pieceEnd){
                            printf("last piece for another node arrives\n");
                            length = p->pieceNumber * linkinfo[arrived_on_link].mtu + p->length;
                        
                            NL_savehopcount(p->src, p->hopcount, arrived_on_link);
                            flood3(packet, length, 0, arrived_on_link);
                            rb[p->src] = &receiveBuffer[p->src][0];
                            
                        }
                        else{
                            printf("piece for another node arrives\n");
                            length = p->length;
                            memcpy(rb[p->src], (char *) p->msg, length);
                            rb[p->src] = rb[p->src] + length;
                        }
			
		} else

			printf("drop\n");
		/* silently drop */;
	}
	return (0);
}

/* ----------------------------------------------------------------------- */

EVENT_HANDLER( reboot_node) {
	if (nodeinfo.nlinks > 32) {
		fprintf(stderr, "flood3 flooding will not work here\n");
		exit(1);
	}
	
	for(int i = 0; i<=255; i++){
            rb[i] = receiveBuffer[i];
        }

	reboot_DLL();
	reboot_NL_table();

	CHECK(CNET_set_handler(EV_APPLICATIONREADY, down_to_network, 0));
	CHECK(CNET_enable_application(ALLNODES));
}
