#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "queue.c"
#include "nl_table.c"
#include "dll_basic.c"

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

#define PACKET_HEADER_SIZE  (sizeof(NL_PACKET) - MAX_MESSAGE_SIZE)
#define PACKET_SIZE(p)	    (PACKET_HEADER_SIZE + p.length)

static char receiveBuffer[256][MAX_MESSAGE_SIZE];
static char *rb[256];
NL_PACKET * lastPacket;

static size_t  packet_length[256];
static int is_traveled_hop;
static int i;
static int mtu;

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


void printmsg(char * msg, size_t length) {
	size_t i;
	//printf("msg :");
	for (i = 0; i < length; i++) {
		printf("%d", (unsigned int) msg[i]);
	}
	//printf("\n");
}

// here the second parameter is the length of msg of packet!
void sendPacketPiecesToDatalink(char *packet, size_t length, int choose_link) {
	size_t maxPacketLength = linkinfo[choose_link].mtu - PACKET_HEADER_SIZE;

	NL_PACKET *tempPacket = (NL_PACKET *) packet;

	size_t tempLength = length;
	char *str = tempPacket->msg;

	NL_PACKET piecePacket;

	piecePacket.src = tempPacket->src;
	piecePacket.dest = tempPacket->dest;
	piecePacket.kind = tempPacket->kind;
	piecePacket.seqno = tempPacket->seqno;
	//piecePacket.hopcount = tempPacket->hopcount;
	piecePacket.trans_time = tempPacket->trans_time;
	piecePacket.traveled_hops_count = tempPacket->traveled_hops_count;
	memcpy(piecePacket.traveled_hops,tempPacket->traveled_hops, MAXHOPS);
	piecePacket.pieceEnd = tempPacket->pieceEnd;
	piecePacket.pieceNumber = tempPacket->pieceNumber;
	piecePacket.checksum = tempPacket->checksum;

	while (tempLength > maxPacketLength) {

		piecePacket.length = maxPacketLength;
		memcpy(piecePacket.msg, str, maxPacketLength);

		CHECK(down_to_datalink(choose_link, (char *) &piecePacket,
				PACKET_SIZE(piecePacket)));
		str = str + maxPacketLength;
		piecePacket.pieceNumber = piecePacket.pieceNumber + 1;
		tempLength = tempLength - maxPacketLength;
	}

	piecePacket.pieceEnd = 1;
	piecePacket.length = tempLength;
	memcpy(piecePacket.msg, str, tempLength);
	CHECK(down_to_datalink(choose_link, (char *) &piecePacket,
			PACKET_SIZE(piecePacket)));
	}

/* ----------------------------------------------------------------------- */

/*  flood3() IS A BASIC ROUTING STRATEGY WHICH TRANSMITS THE OUTGOING PACKET
 ON EITHER THE SPECIFIED LINK, OR ALL BEST-KNOWN LINKS WHILE AVOIDING
 ANY OTHER SPECIFIED LINK.
 */
static void flood3(char *packet, size_t length, int choose_link, int avoid_link) {
	//printf("flood3\n");
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
static EVENT_HANDLER(down_to_network) {

	//printf("down_to_network\n");
	NL_PACKET p;

	p.length = sizeof(p.msg);
	//printf("p.length = %d, MAX_MESSAGE_SIZE = %d", p.length, MAX_MESSAGE_SIZE);
	CHECK(CNET_read_application(&p.dest, p.msg, &p.length));
	CNET_disable_application(p.dest);

	p.src = nodeinfo.address;
	p.kind = NL_DATA;
	//p.hopcount = 0;
	memset(p.traveled_hops, -1, MAXHOPS*sizeof(int));
	p.traveled_hops[0] = nodeinfo.nodenumber;
	p.traveled_hops_count = 1;
	p.trans_time = 0;
	p.seqno = NL_nextpackettosend(p.dest);
	p.pieceNumber = 0;
	p.pieceEnd = 0;

	p.checksum = CNET_ccitt((unsigned char *) (p.msg), p.length);
	lastPacket = &p;
	update_last_packet(&p);
	
	printf("Packet generated at host %d, des %d\n\n", nodeinfo.address, p.dest);
	flood3((char *) &p, PACKET_SIZE(p), 0, 0);

}

/*  up_to_network() IS CALLED FROM THE DATA LINK LAYER (BELOW) TO ACCEPT
 A PACKET FOR THIS NODE, OR TO RE-ROUTE IT TO THE INTENDED DESTINATION.
 */
int up_to_network(char *packet, size_t length, int arrived_on_link) {
	printf("up to network at hop %s \n", nodeinfo.nodename);
	NL_PACKET *p = (NL_PACKET *) packet;
	
	//if(p->traveled_hops_count < 1 || p->traveled_hops_count > MAXHOPS)
	  //return 0;
	
	//++p->hopcount; /* took 1 hop to get here */
	if(p->traveled_hops_count > 0 && p->traveled_hops_count<= MAXHOPS){
	  is_traveled_hop = 0;
	  for(i=0; i<p->traveled_hops_count; ++i){
	    if(p->traveled_hops[i] == nodeinfo.nodenumber){
	      is_traveled_hop = 1;
	      break;
	    }
	  }
	  if(is_traveled_hop != 1){
	    printf("seqno %d    pieceNumber %d   src %d    des %d    current_hop %d\n", p->seqno, p->pieceNumber, p->src, p->dest, nodeinfo.address);
	    printf("p->traveled_hops_count %d\n", p->traveled_hops_count);
	    p->traveled_hops[p->traveled_hops_count++] = nodeinfo.nodenumber;
	    mtu = linkinfo[arrived_on_link].mtu;
	    p->trans_time += ((CnetTime)8000000 * mtu / linkinfo[arrived_on_link].bandwidth + linkinfo[arrived_on_link].propagationdelay)/mtu;
	    //p->trans_time += linkinfo[arrived_on_link].costperframe;
	  } else{ 
	      printf("seqno %d    pieceNumber %d    src %d    des %d    current_hop %d. This hop has been traveled\n",  p->seqno, p->pieceNumber, p->src, p->dest, nodeinfo.address);
	      printf("\n");
	      /*free(&p->traveled_hops);
	      free(&p->msg);
	      free(p);
	      */
	      return 0;
	  }
	}
					
	/*  IS THIS PACKET IS FOR ME? */
	if (p->dest == nodeinfo.address) {
		switch (p->kind) {
		case NL_DATA:
			if (p->seqno == NL_packetexpected(p->src)) {
				length = p->length;
				memcpy(rb[p->src], (char *) p->msg, length);
				rb[p->src] = rb[p->src] + length;
				packet_length[p->src] += length;
			
				printf("This piece traveled %d hops\n", p->traveled_hops_count);

				if (p->pieceEnd) {
					CnetAddr tmpaddr;
					//length = p->pieceNumber * (linkinfo[arrived_on_link].mtu
					//		- PACKET_HEADER_SIZE) + p->length;
					length = packet_length[p->src];
					
					int p_checksum = p->checksum;
                                        int checksum = CNET_ccitt(
                                                        (unsigned char *) (receiveBuffer[p->src]),
                                                        (int) length);
                                        if (p_checksum != checksum) {
                                                /***************************send back error ack**************/
						//NL_savehopcount(p->src, p->hopcount, arrived_on_link);
                                                NL_savehopcount(p->src, p->trans_time, arrived_on_link);

                                                tmpaddr = p->src; /* swap src and dest addresses */
                                                p->src = p->dest;
                                                p->dest = tmpaddr;

                                                p->kind = NL_ERR_ACK;
                                                //p->hopcount = 0;
						p->traveled_hops_count = 0;
						memset(p->traveled_hops, -1, MAXHOPS*sizeof(int));
						p->trans_time = 0;
                                                p->length = 0;
                                                flood3(packet, PACKET_HEADER_SIZE, arrived_on_link, 0);
                                                return 0;
                                                /***************************end******************************/
                                        }
					
					if (CNET_write_application(receiveBuffer[p->src], &length) == -1) {
						//source node should send this msg again
						if (cnet_errno == ER_CORRUPTFRAME) {
							printf("Warning: host %d received a corrupt packet (seqno = %d) from %d\n", nodeinfo.address, p->seqno, p->src);
						} else if (cnet_errno == ER_MISSINGMSG) {
							printf("Warning: host %d received a unordered packet (seqno = %d) from %d\n", nodeinfo.address, p->seqno, p->src);
						}else if (cnet_errno == ER_BADSIZE){
							printf("Warning: host %d received a loss packet (seqno = %d) from %d\n", nodeinfo.address, p->seqno, p->src);
						}
					} else {
					    printf("host %d received a correct packet (seqno = %d) from %d\n", nodeinfo.address, p->seqno, p->src);
					}
					  
					rb[p->src] = &receiveBuffer[p->src][0];
					packet_length[p->src] = 0;

					inc_NL_packetexpected(p->src);

					//NL_savehopcount(p->src, p->hopcount, arrived_on_link);
					NL_savehopcount(p->src, p->trans_time, arrived_on_link);
					
					//send a NL_ACK back to source host
					tmpaddr = p->src; /* swap src and dest addresses */
					p->src = p->dest;
					p->dest = tmpaddr;
					p->kind = NL_ACK;
					//p->hopcount = 0;
					p->traveled_hops_count = 0;
					memset(p->traveled_hops, -1, MAXHOPS*sizeof(int));
					p->trans_time = 0;
					p->length = 0;
					packet_length[p->src] = 0;
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
				//NL_savehopcount(p->src, p->hopcount, arrived_on_link);
				NL_savehopcount(p->src, p->trans_time, arrived_on_link);
				CHECK(CNET_enable_application(p->src));
			}
			break;
		 case NL_ERR_ACK:
                        printf("ERROR ACK!\n");
                        if (p->seqno == NL_ackexpected(p->src)) {
                                //NL_savehopcount(p->src, p->hopcount, arrived_on_link);
                                NL_savehopcount(p->src, p->trans_time, arrived_on_link);
                                printf("packet %d (to %d) error!\n", p->seqno, p->src);
                                NL_PACKET * packettoresend = get_last_packet(p->src);
                                printf("length = %d seq = %d\n", packettoresend->length,
                                                packettoresend->seqno);

                                int a = packettoresend->checksum;
                                int b = CNET_ccitt((unsigned char *) (packettoresend->msg),
                                                (int) (packettoresend->length));
                                if (a == b) {
                                        printf("ok!\n");
                                } else
                                        printf("wrong!\n");
                                int len = PACKET_HEADER_SIZE + packettoresend->length;
                                flood3((char *) packettoresend, len, 0, 0);

                        }
                        break;
		default: ;
			//printf("it's nothing!====================\n");
		}
	}
	/* THIS PACKET IS FOR SOMEONE ELSE */
	else {
		// 		printf("hopcount = %d\n", p->hopcount);
		// 		printf("MAXHOPS = %d\n", MAXHOPS);
		//if (p->hopcount < MAXHOPS) { /* if not too many hops... */
			//printf("other's frame!\n");
			//printf("piece for another node arrives\n");	
			length = p->length;
			memcpy(rb[p->src], (char *) p->msg, length);
			rb[p->src] = rb[p->src] + length;
			packet_length[p->src] += length;

			if (p->pieceEnd) {
				//printf("last piece for another node arrives\n");
				//length = p->pieceNumber * (linkinfo[arrived_on_link].mtu
				//		- PACKET_HEADER_SIZE) + p->length;
				length = packet_length[p->src];
				//NL_savehopcount(p->src, p->hopcount, arrived_on_link);
				NL_savehopcount(p->src, p->trans_time, arrived_on_link);
				NL_PACKET wholePacket;

				wholePacket.src = p->src;
				wholePacket.dest = p->dest;
				wholePacket.kind = p->kind;
				wholePacket.seqno = p->seqno;
				//wholePacket.hopcount = p->hopcount;
				wholePacket.trans_time = p->trans_time;
				wholePacket.traveled_hops_count = p->traveled_hops_count;
				memcpy(wholePacket.traveled_hops,p->traveled_hops, MAXHOPS);
				wholePacket.pieceEnd = 0;
				wholePacket.pieceNumber = 0;
				wholePacket.length = length;
				packet_length[p->src] = 0;

				memcpy(wholePacket.msg, receiveBuffer[p->src], length);
				flood3((char *) &wholePacket, PACKET_SIZE(wholePacket), 0,
						arrived_on_link);
				rb[p->src] = &receiveBuffer[p->src][0];

			}

		//} else
		  //  printf("drop\n");
		/* silently drop */;
	}
	printf("\n");
	return (0);
}

/* ----------------------------------------------------------------------- */

EVENT_HANDLER( reboot_node) {
	if (nodeinfo.nlinks > 32) {
		// 		fprintf(stderr, "flood3 flooding will not work here\n");
		exit(1);
	}

	for (int i = 0; i <= 255; i++) {
		rb[i] = receiveBuffer[i];
		packet_length[i] = 0;
	}

	reboot_DLL();
	reboot_NL_table();

	CHECK(CNET_set_handler(EV_APPLICATIONREADY, down_to_network, 0));
	CHECK(CNET_enable_application(ALLNODES));
}
