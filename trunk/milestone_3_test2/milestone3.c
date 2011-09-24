#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "queue.c"
#include "nl_table.c"
#include "dll_basic.c"
#include "nl_packet.h"

#define MAXHOPS         30

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
/*
 typedef enum {
 NL_DATA, NL_ACK, NL_ERR_ACK
 } NL_PACKETKIND;

 typedef struct {
 CnetAddr src;
 CnetAddr dest;
 NL_PACKETKIND kind; // only ever NL_DATA or NL_ACK
 int seqno; // 0, 1, 2, ...
 int hopcount;

 size_t pieceNumber;
 int pieceEnd;
 size_t length; // the length of the msg portion only
 //size_t no_pieces;
 int checksum;
 char msg[MAX_MESSAGE_SIZE];
 } NL_PACKET;
 */

#define PACKET_HEADER_SIZE  (sizeof(NL_PACKET) - MAX_MESSAGE_SIZE)
#define PACKET_SIZE(p)      (PACKET_HEADER_SIZE + p.length)

static char receiveBuffer[256][MAX_MESSAGE_SIZE];
static char *rb[256];
//static size_t packet_length[256];
NL_PACKET * lastPacket;
static int mtu;

void printmsg(char * msg, size_t length) {
        size_t i;
        //("msg :");
        for (i = 0; i < length; i++) {
                //("%d", (unsigned int) msg[i]);
        }
        //("\n");
}

void update_last_packet(NL_PACKET *last) {
        int index = find_address(last->dest);
        //NL_PACKET * lastsend = &(NL_table[index].lastpacket);

        memcpy(&(NL_table[index].lastpacket), last, PACKET_SIZE((*last)));
        //free(temp);
}

NL_PACKET * get_last_packet(CnetAddr address) {
        int index = find_address(address);
        return (NL_PACKET *) &(NL_table[index].lastpacket);
}

// here the second parameter is the length of msg of packet!
void sendPacketPiecesToDatalink(char *packet, size_t length, int choose_link) {
        //printf("send packet pieces to data link at %s\n", nodeinfo.nodename);
        size_t maxPacketLength = linkinfo[choose_link].mtu - PACKET_HEADER_SIZE;
	
        NL_PACKET *tempPacket = (NL_PACKET *) packet;
        size_t tempLength = length;
        char *str = tempPacket->msg;
	
	//printf("src = %d, des = %d, linkinfo[choose_link].mtu = %d, PACKET_HEADER_SIZE = %d\n", tempPacket->src, tempPacket->dest, linkinfo[choose_link].mtu, PACKET_HEADER_SIZE);
	
        NL_PACKET piecePacket;
        piecePacket.src = tempPacket->src;
        piecePacket.dest = tempPacket->dest;
        piecePacket.kind = tempPacket->kind;
        piecePacket.seqno = tempPacket->seqno;
        piecePacket.hopcount = tempPacket->hopcount;
	piecePacket.pieceNumber = tempPacket->pieceNumber;
        piecePacket.pieceEnd = tempPacket->pieceEnd;
	piecePacket.src_packet_length = tempPacket->src_packet_length;
        piecePacket.checksum = tempPacket->checksum;
	piecePacket.trans_time = tempPacket->trans_time;
	
        while (tempLength > maxPacketLength) {
		//printf("packet remains %d  bytes\n", tempLength);
                piecePacket.length = maxPacketLength;
                memcpy(piecePacket.msg, str, maxPacketLength);

                CHECK(down_to_datalink(choose_link, (char *) &piecePacket,
                                PACKET_SIZE(piecePacket)));
                //("piece %d down_to_datalink\n", piecePacket.pieceNumber);

                str = str + maxPacketLength;
                piecePacket.pieceNumber = piecePacket.pieceNumber + 1;
                tempLength = tempLength - maxPacketLength;
        }

        piecePacket.pieceEnd = 1;
        piecePacket.length = tempLength;
	//printf("last piece contains %d  bytes, pieceNumber = %d\n\n", tempLength ,piecePacket.pieceNumber);

        memcpy(piecePacket.msg, str, tempLength);
        ////("Required link is provided link = %d\n", choose_link);
        CHECK(down_to_datalink(choose_link, (char *) &piecePacket,
                        PACKET_SIZE(piecePacket)));
        //("last piece %d down_to_datalink\n", piecePacket.pieceNumber);
        ////("Provided link = %d sent! \n", choose_link);
}

/* ----------------------------------------------------------------------- */

/*  flood3() IS A BASIC ROUTING STRATEGY WHICH TRANSMITS THE OUTGOING PACKET
 ON EITHER THE SPECIFIED LINK, OR ALL BEST-KNOWN LINKS WHILE AVOIDING
 ANY OTHER SPECIFIED LINK.
 */
static void flood3(char *packet, size_t length, int choose_link, int avoid_link) {
	//printf("flood3\n\n");
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
                        if (link == avoid_link) /* possibly avoid this one */
			  continue;
                        if (links_wanted & (1 << link)) /* use this link if wanted */
			  sendPacketPiecesToDatalink(packet, p->length, link);
                }
        }
}

/*  down_to_network() RECEIVES NEW MESSAGES FROM THE APPLICATION LAYER AND
 PREPARES THEM FOR TRANSMISSION TO OTHER NODES.
 */
static EVENT_HANDLER( down_to_network) {
        NL_PACKET p;

        p.length = sizeof(p.msg);
        CHECK(CNET_read_application(&p.dest, p.msg, &p.length));
        CNET_disable_application(p.dest);

        p.src = nodeinfo.address;
        p.kind = NL_DATA;
	p.seqno = NL_nextpackettosend(p.dest);
        p.hopcount = 0;
	p.pieceNumber = 0;
        p.pieceEnd = 0;
	p.src_packet_length = (int) p.length;
        p.checksum = CNET_ccitt((unsigned char *) (p.msg), p.src_packet_length);
	p.trans_time = 0;
	
        //lastPacket = &p;
	printf("packet generated, src = %d, des = %d, seqno = %d, send_length = %d, checksum = %d\n\n", nodeinfo.address, p.dest, p.seqno, p.length, p.checksum);
        flood3((char *) &p, PACKET_SIZE(p), 0, 0);
        update_last_packet(&p);

}

//static int err_count = 0;

/*  up_to_network() IS CALLED FROM THE DATA LINK LAYER (BELOW) TO ACCEPT
 A PACKET FOR THIS NODE, OR TO RE-ROUTE IT TO THE INTENDED DESTINATION.
 */
int up_to_network(char *packet, size_t length, int arrived_on_link) {
        NL_PACKET *p = (NL_PACKET *) packet;
	//printf("up to network at %d (from %d to %d)\n", nodeinfo.address, p->src, p->dest);
        ++p->hopcount; /* took 1 hop to get here */
        mtu = linkinfo[arrived_on_link].mtu;
        p->trans_time += ((CnetTime)8000 * 1000 * mtu / linkinfo[arrived_on_link].bandwidth + linkinfo[arrived_on_link].propagationdelay)*100/mtu;
        ////("me = %d, dest = %d =======\n", nodeinfo.address, p->dest);
        /*  IS THIS PACKET IS FOR ME? */
        if (p->dest == nodeinfo.address) {
                switch (p->kind) {
                case NL_DATA:
                        if (p->seqno == NL_packetexpected(p->src)) {
                                length = p->length;
                                memcpy(rb[p->src], (char *) p->msg, length);
                                rb[p->src] = rb[p->src] + length;
				//packet_length[p->src] += length;

                                if (p->pieceEnd) {
                                        CnetAddr tmpaddr;
                                        length = p->pieceNumber * (linkinfo[arrived_on_link].mtu
                                                        - PACKET_HEADER_SIZE) + p->length;
					//length = packet_length[p->src];
					//packet_length[p->src] = 0;
                                        int p_checksum = p->checksum;
                                        int checksum = CNET_ccitt(
                                                        (unsigned char *) (receiveBuffer[p->src]),
                                                        p->src_packet_length);
					printf("%d received a packet, src = %d, des = %d, seqno = %d,  send_length = %d,receive_length = %d \n", nodeinfo.address, p->src, p->dest, p->seqno, p->src_packet_length, length);
					printf("last_piece_trans_time = %d, hopcount = %d\n", p->trans_time, p->hopcount);
					printf("src_checksum = %d calc_checksum = %d, ", p_checksum, checksum);
                                        if (p_checksum != checksum) {
                                                /***************************send back error ack**************/
						printf("error packet\n\n");
                                                NL_savehopcount(p->src, p->trans_time, arrived_on_link);

                                                tmpaddr = p->src; /* swap src and dest addresses */
                                                p->src = p->dest;
                                                p->dest = tmpaddr;

                                                p->kind = NL_ERR_ACK;                                        p->hopcount = 0;
						p->pieceNumber = 0;
						p->pieceEnd = 0;
						p->length = 0;
						p->src_packet_length = 0;
						p->checksum = 0;
						p->trans_time = 0;
						
                                                flood3(packet, PACKET_HEADER_SIZE, arrived_on_link, 0);
                                                
                                                rb[p->src] = &receiveBuffer[p->src][0];
						//printf("\n");
                                                return 0;
                                                /***************************end******************************/
                                        }
                                        /*
                                        if (CNET_write_application(receiveBuffer[p->src], &length)
                                                        == -1) {
                                                //source node should send this msg again
                                                //printf("===\ncnet_errno = %d\n===\n", cnet_errno);
                                                //printf("error count: %d\n", ++ err_count);
                                                if (cnet_errno == ER_CORRUPTFRAME) {
						   printf("corrupt packet\n");
                                                        //printf("\nWarning: frame corrupted\n==\n");
                                                } else if (cnet_errno == ER_MISSINGMSG) {
						   printf("unordered packet\n");
                                                        //printf("\nWarning: frame missed\n\n");
                                                } else if(cnet_errno == ER_BADSIZE){
						  printf("loss packet\n");
						}
                                        }
                                        */
					else {
					  printf("correct packet\n", p->dest, p->seqno, p->src);
					}
                                        rb[p->src] = &receiveBuffer[p->src][0];

                                        inc_NL_packetexpected(p->src);

                                        NL_savehopcount(p->src, p->trans_time, arrived_on_link);

                                        tmpaddr = p->src; /* swap src and dest addresses */
                                        p->src = p->dest;
                                        p->dest = tmpaddr;

                                        p->kind = NL_ACK;
                                        p->hopcount = 0;
					p->pieceNumber = 0;
					p->pieceEnd = 0;
					p->length = 0;
					p->src_packet_length = 0;
					p->checksum = 0;
					p->trans_time = 0;
					
                                        flood3(packet, PACKET_HEADER_SIZE, arrived_on_link, 0);
					printf("\n");
                                }
                        }
                        break;

                case NL_ACK:
                        if (p->seqno == NL_ackexpected(p->src)) {
                                ////("ACK come!\n");
                                inc_NL_ackexpected(p->src);
                                NL_savehopcount(p->src, p->trans_time, arrived_on_link);
                                CHECK(CNET_enable_application(p->src));
                        }
                        break;
                case NL_ERR_ACK:
                        printf("NL_ERR_ACK!\n");
                        if (p->seqno == NL_ackexpected(p->src)) {
                                NL_savehopcount(p->src, p->trans_time, arrived_on_link);
                                NL_PACKET * packettoresend = get_last_packet(p->src);
                                printf("resend packet, src = %d, des = %d, seqno = %d, send_length = %d, checksum = %d\n", packettoresend->src, packettoresend->dest, packettoresend->seqno, packettoresend->length, packettoresend->checksum);
				/*
                                int a = packettoresend->checksum;
                                int b = CNET_ccitt((unsigned char *) (packettoresend->msg),
                                                (int) (packettoresend->length));
                                if (a == b) {
                                        printf("ok!\n");
                                } else
                                        printf("wrong!\n");
				*/
                                int len = PACKET_HEADER_SIZE + packettoresend->length;
                                flood3((char *) packettoresend, len, 0, 0);
                                
                        }
                        printf("\n");
                        break;
                default:
                        //("it's nothing!====================\n");
                        break;
                }
        }
        /* THIS PACKET IS FOR SOMEONE ELSE */
        else {
                //              //("hopcount = %d\n", p->hopcount);
                //              //("MAXHOPS = %d\n", MAXHOPS);
                if (p->hopcount < MAXHOPS) { /* if not too many hops... */
                        ////("other's frame!\n");

                        //("piece for another node arrives\n");
                        length = p->length;
                        memcpy(rb[p->src], (char *) p->msg, length);
                        rb[p->src] = rb[p->src] + length;
			//packet_length[p->src] += length;

                        if (p->pieceEnd) {
                                //printf("last piece for another node arrives\n");
                                length = p->pieceNumber * (linkinfo[arrived_on_link].mtu
                                                - PACKET_HEADER_SIZE) + p->length;
				//length = packet_length[p->src];
				//packet_length[p->src] = 0;
                                NL_savehopcount(p->src, p->trans_time, arrived_on_link);

                                NL_PACKET wholePacket;

                                wholePacket.src = p->src;
                                wholePacket.dest = p->dest;
                                wholePacket.kind = p->kind;
                                wholePacket.seqno = p->seqno;
                                wholePacket.hopcount = p->hopcount;
				wholePacket.pieceNumber = 0;
                                wholePacket.pieceEnd = 0;
				wholePacket.length = length;
				wholePacket.src_packet_length = p->src_packet_length;
				wholePacket.checksum = p->checksum;
				wholePacket.trans_time = p->trans_time;
				
				memcpy(wholePacket.msg, receiveBuffer[p->src], length);

                                //                             //("contents of msg forwarding is \n %s\n", receiveBuffer[p->src]);
                                flood3((char *) &wholePacket, PACKET_SIZE(wholePacket), 0,
                                                arrived_on_link);
                                rb[p->src] = &receiveBuffer[p->src][0];

                        }

                } 
                else {  /* silently drop */;
		  //free(&p->msg);
		  //free(p);
		}
        }
        return (0);
}

/* ----------------------------------------------------------------------- */

EVENT_HANDLER( reboot_node) {
        if (nodeinfo.nlinks > 32) {
                //              f//(stderr, "flood3 flooding will not work here\n");
                exit(1);
        }

        for (int i = 0; i <= 255; i++) {
                rb[i] = receiveBuffer[i];
		//packet_length[i] = 0;
        }
	//memset(packet_length, 0, 256*sizeof(size_t));
        reboot_DLL();
        reboot_NL_table();

        CHECK(CNET_set_handler(EV_APPLICATIONREADY, down_to_network, 0));
        CHECK(CNET_enable_application(ALLNODES));
}
