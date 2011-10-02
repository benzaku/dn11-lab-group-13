#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cnetsupport.h>

#include "queue.c"
#include "nl_table.c"
#include "dll_basic.c"
#include "nl_packet.h"
#include "milestone3.h"
#include "receive_buffer.c"

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

static CnetTimerID last_packet_timeout_timer = NULLTIMER;
VECTOR rb;

/*  flood() IS A BASIC ROUTING STRATEGY WHICH TRANSMITS THE OUTGOING PACKET
 ON EITHER THE SPECIFIED LINK, OR ALL BEST-KNOWN LINKS WHILE AVOIDING
 ANY OTHER SPECIFIED LINK.
 */
static void flood(char *packet, size_t length, int choose_link, int avoid_link) {

	/*  REQUIRED LINK IS PROVIDED - USE IT */
	if (choose_link != 0) {
		CHECK(down_to_datalink(choose_link, packet, length));
	}

	/*  OTHERWISE, CHOOSE THE BEST KNOWN LINKS, AVOIDING ANY SPECIFIED ONE */
	else {
		NL_PACKET *p = (NL_PACKET *) packet;
		int links_wanted = NL_linksofminhops(p->dest);
		int link;

		for (link = 1; link <= nodeinfo.nlinks; ++link) {
			if (link == avoid_link) /* possibly avoid this one */
				continue;
			if (links_wanted & (1 << link)) /* use this link if wanted */
				CHECK(down_to_datalink(link, packet, length));
		}
	}
}

void piece_to_flood(char *packet, size_t mtu_from_src_to_dest) {

	NL_PACKET *tempPacket = (NL_PACKET *) packet;
	size_t maxPieceLength = mtu_from_src_to_dest - PACKET_HEADER_SIZE;
	size_t tempLength = tempPacket->length;
	char *str = tempPacket->msg;

	NL_PACKET piecePacket;
	piecePacket.src = tempPacket->src;
	piecePacket.dest = tempPacket->dest;
	piecePacket.kind = tempPacket->kind;
	piecePacket.seqno = tempPacket->seqno;
	piecePacket.hopcount = tempPacket->hopcount;
	piecePacket.pieceStartPosition = 0;
	piecePacket.pieceEnd = 0;
	piecePacket.src_packet_length = tempPacket->src_packet_length;
	piecePacket.checksum = tempPacket->checksum;
	piecePacket.trans_time = tempPacket->trans_time;
	piecePacket.is_resent = tempPacket->is_resent;
	while (tempLength > maxPieceLength) {
		piecePacket.length = maxPieceLength;
		memcpy(piecePacket.msg, str, maxPieceLength);
		piecePacket.piece_checksum = CNET_crc32(
				(unsigned char *) (piecePacket.msg), piecePacket.length);

		flood((char *) &piecePacket, PACKET_SIZE(piecePacket), 0, 0);

		str = str + maxPieceLength;
		piecePacket.pieceStartPosition = piecePacket.pieceStartPosition
				+ maxPieceLength;
		tempLength = tempLength - maxPieceLength;
	}

	piecePacket.pieceEnd = 1;
	piecePacket.length = tempLength;

	memcpy(piecePacket.msg, str, tempLength);
	piecePacket.piece_checksum = CNET_crc32(
			(unsigned char *) (piecePacket.msg), piecePacket.length);

	flood((char *) &piecePacket, PACKET_SIZE(piecePacket), 0, 0);

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
	p.pieceStartPosition = 0;
	p.pieceEnd = 0;
	p.src_packet_length = (int) p.length;
	p.checksum = CNET_crc32((unsigned char *) (p.msg), p.src_packet_length);
	p.piece_checksum = CNET_crc32((unsigned char *) (p.msg),
			p.src_packet_length);
	p.trans_time = 0;
	p.is_resent = 0;

	update_last_packet(&p);

	//TODO: please inform the mtu from src to dest by using pathfinder packet
	size_t mtu_from_src_to_dest = 96;
	piece_to_flood((char *) &p, mtu_from_src_to_dest);

}

static void NL_update_lastackexpected(CnetAddr address) {
	NL_table[find_address(address)].last_ack_expected = NL_table[find_address(
			address)].ackexpected;
}

/*  up_to_network() IS CALLED FROM THE DATA LINK LAYER (BELOW) TO ACCEPT
 A PACKET FOR THIS NODE, OR TO RE-ROUTE IT TO THE INTENDED DESTINATION.
 */
int up_to_network(char *packet, size_t length, int arrived_on_link) {

	NL_PACKET *p = (NL_PACKET *) packet;
	if (p->src == nodeinfo.address) {
		printf(
				"drop a packet at %d, src = %d, des = %d, seqno = %d, length = %d\n\n",
				nodeinfo.address, p->src, p->dest, p->seqno, p->length);
		return 0;
	}

	++p->hopcount; /* took 1 hop to get here */
	mtu = linkinfo[arrived_on_link].mtu;
	p->trans_time += ((CnetTime) 8000 * 1000 * mtu
			/ linkinfo[arrived_on_link].bandwidth
			+ linkinfo[arrived_on_link].propagationdelay) * 100 / mtu;

	size_t maxsize = mtu - PACKET_HEADER_SIZE;
	if (p->length > maxsize || p->length < (size_t) 0) {
		//fprintf(stdout, "p->length = %d, max size = %d\n", (int)(p->length), (int) maxsize);
		return 0;
	}

	/*  IS THIS PACKET IS FOR ME? */
	if (p->dest == nodeinfo.address) {

		switch (p->kind) {
		case NL_DATA:

			if (p->seqno == NL_packetexpected(p->src)) {
				
                                int isWholeMsg = RB_save_msg_link(rb, p, arrived_on_link);
                            
				if(isWholeMsg) {
                                    /*
                                        all pieces are arrived
                                        now get the whole msg from buffer and write it in applicaiton layer
                                    */
                                    
                                    CHECK(CNET_write_application((char*) p->msg, &p->src_packet_length));
                                    send_ack(p, arrived_on_link, 0);
                                    return 0;
                                
                                } else if(!isWholeMsg && p->pieceStartPosition + p->length == p->src_packet_length){
                                    /*
                                        last piece arrives, now check the missing frame position
                                    */
                                    
                                    // check which piece is missing and require to resend this piece
                                    
                                    START_POS allMissingFramePositions;
                                    RB_find_missing_piece(rb, p, arrived_on_link, &allMissingFramePositions);
                                    
                                    int *posTemp;
                                    posTemp = allMissingFramePositions.pos;
                                    
                                    int numberOfMissingFrame = allMissingFramePositions.size;
                                    int i;
                                    
                                    for(i = 0; i < numberOfMissingFrame; i++){
                                        
                                        p->pieceStartPosition = *posTemp;
                                        send_ack(p, arrived_on_link, 1);
                                        posTemp++;
                                    }
                                    
                                    
                                }
			}
			break;

		case NL_ACK:
			if (p->seqno == NL_ackexpected(p->src)) {
				printf("ACK come!\n");
				inc_NL_ackexpected(p->src);
				NL_savehopcount(p->src, p->trans_time, arrived_on_link);
				NL_set_has_resent(p->src, 0);
				NL_update_lastackexpected(p->src);
				CHECK(CNET_enable_application(p->src));
			} else if (p->seqno < NL_ackexpected(p->src)) {
				printf("outdated ACK come!\n");

			}
			break;
		case NL_ERR_ACK:
			printf("NL_ERR_ACK!\n");
			if (p->seqno == NL_ackexpected(p->src)) {
				if (NL_get_has_resent(p->src) == 0) {
					NL_savehopcount(p->src, p->trans_time, arrived_on_link);
					NL_PACKET * packettoresend = get_last_packet(p->src);
					printf(
							"src = %d, des = %d, seqno = %d, send_length = %d, checksum = %d\n",
							packettoresend->src, packettoresend->dest,
							packettoresend->seqno, packettoresend->length,
							packettoresend->checksum);
					printf("resend it\n");
					int len = PACKET_HEADER_SIZE + packettoresend->length;
					packettoresend->is_resent = 1;
					NL_set_has_resent(p->src, 1);
					flood((char *) packettoresend, len, 0, 0);

				} else {
					printf(
							"this packet has already been resent, dont resent it again\n");
				}
			} else {
				printf(
						"this packet has already been correct received, dont resent it again\n");
			}
			break;

		case NL_ERR_ACK_RESENT:
			printf("NL_ERR_ACK_RESENT!\n");
			if (p->seqno == NL_ackexpected(p->src)) {
				NL_savehopcount(p->src, p->trans_time, arrived_on_link);
				NL_PACKET * packettoresend = get_last_packet(p->src);
				printf(
						"resend a resent packet, src = %d, des = %d, seqno = %d, send_length = %d, checksum = %d\n",
						packettoresend->src, packettoresend->dest,
						packettoresend->seqno, packettoresend->length,
						packettoresend->checksum);

				int len = PACKET_HEADER_SIZE + packettoresend->length;
				packettoresend->is_resent = 1;
				flood((char *) packettoresend, len, 0, 0);

			} else {
				printf(
						"this packet has already been correct received, dont resent it again\n");
			}

			break;
		default:
			printf("it's nothing!====================\n");
			break;
		}
	}
	/* THIS PACKET IS FOR SOMEONE ELSE */
	else {
		if (p->hopcount < MAXHOPS) { /* if not too many hops... */

			NL_savehopcount(p->src, p->hopcount, arrived_on_link);//TODO: pay attention for this line, shall we need it?

			/* retransmit on best links *except* the one on which it arrived */
			flood(packet, length, 0, arrived_on_link);
		}
	}
	return 0;
}

void up_to_application(NL_PACKET *p, int arrived_on_link) {
	size_t length = p->pieceStartPosition + p->length;

	if (length != p->src_packet_length)
		return;
	uint32_t p_checksum = p->checksum;

	uint32_t checksum = CNET_crc32((unsigned char *) (p->msg),
			p->src_packet_length);

	if (p_checksum != checksum) {
		send_ack(p, arrived_on_link, 1);
	} else { // received a correct packet
		CHECK(CNET_write_application(p->msg, &length));
		send_ack(p, arrived_on_link, 0);
	}
}

void send_ack(NL_PACKET *p, int arrived_on_link, unsigned short int mode_code) {
	CnetAddr tmpaddr;
	if (mode_code == 1) {
		printf("error packet! src = %d, dest = %d, seqno = %d\n", p->src,
				p->dest, p->seqno);
		NL_savehopcount(p->src, p->trans_time, arrived_on_link);
		if (p->is_resent == 1)
			p->kind = NL_ERR_ACK_RESENT;
		else
			p->kind = NL_ERR_ACK;

	} else if (mode_code == 0) {
		printf("correct packet src = %d, dest = %d, seqno = %d\n", p->src,
				p->dest, p->seqno);
		inc_NL_packetexpected(p->src);
		NL_savehopcount(p->src, p->trans_time, arrived_on_link);
		p->kind = NL_ACK;
                p->piece_checksum = 0;
                p->pieceStartPosition = 0;

// 		flood((char *) p, PACKET_HEADER_SIZE, arrived_on_link, 0);
	} else if (mode_code == 2) {
		printf("ack for outdated frame\n");
		p->kind = NL_ACK;
	}


	/* actually we just need to set p->length to 0 */
	p->hopcount = 0;
	p->pieceEnd = 1;
	p->length = 0;
	p->src_packet_length = 0;
	p->checksum = 0;
	p->trans_time = 0;
	p->is_resent = 0;
	memset(&p->msg, 0, MAX_MESSAGE_SIZE * sizeof(char));

	tmpaddr = p->src; /* swap src and dest addresses */
	p->src = p->dest;
	p->dest = tmpaddr;
	flood((char *) p, PACKET_HEADER_SIZE, arrived_on_link, 0);
}

void update_last_packet(NL_PACKET *last) {
	int index = find_address(last->dest);
	//NL_PACKET * lastsend = &(NL_table[index].lastpacket);

	memcpy(&(NL_table[index].lastpacket), last, PACKET_SIZE((*last)));
	//NL_table[index].last_ack_expected = NL_ackexpected(last->dest);
	//free(temp);
}

NL_PACKET * get_last_packet(CnetAddr address) {
	int index = find_address(address);
	return (NL_PACKET *) &(NL_table[index].lastpacket);
}

// static void resend_last_packet(int nl_table_id) {
// 	NL_PACKET packettoresend = NL_table[nl_table_id].lastpacket;
// 
// 	int len = PACKET_HEADER_SIZE + packettoresend.length;
// 	packettoresend.is_resent = 1;
// 	NL_set_has_resent(NL_table[nl_table_id].address, 1);
// 	//NL_inc_resent_times(p->src); // for debug
// 	flood((char *) &packettoresend, len, 0, 0);
// 
// }

// static void timeout_check(CnetEvent ev, CnetTimerID timer, CnetData data) {
// 
// 	CnetTime packet_timeout;
// 	printf("======================\n");
// 	printf("timeout_check!\n");
// 	printf("======================\n");
// 	int i;
// 	for (i = 0; i < NL_table_size; i++) {
// 		//printf("no: %d, ", i);
// 		if (NL_table[i].last_ack_expected == NL_table[i].ackexpected
// 				&& NL_table[i].nextpackettosend > NL_table[i].ackexpected) {
// 			printf("packet no %d loss! resend...\n",
// 					NL_table[i].last_ack_expected);
// 			resend_last_packet(i);
// 
// 		} else if (NL_table[i].last_ack_expected < NL_table[i].ackexpected) {
// 			printf("packet no %d not loss!\n", NL_table[i].last_ack_expected);
// 			NL_table[i].last_ack_expected = NL_table[i].ackexpected;
// 		} else {
// 			printf("Unknow condition!\n");
// 			printf("lastackexpected : %d\n", NL_table[i].last_ack_expected);
// 		}
// 	}
// 	printf("\n");
// 	packet_timeout = 5000000;
// 	last_packet_timeout_timer = CNET_start_timer(EV_TIMER2, packet_timeout, 0);
// }

/* ----------------------------------------------------------------------- */
EVENT_HANDLER( reboot_node) {
	if (nodeinfo.nlinks > 32) {
		exit(1);
	}
	
	rb = vector_new();
//         RB_init(rb);
	
	reboot_DLL();
	reboot_NL_table();

	CHECK(CNET_set_handler(EV_APPLICATIONREADY, down_to_network, 0));

// 	CHECK(CNET_set_handler(EV_TIMER2, timeout_check, 0));

	CnetTime packet_timeout;

	packet_timeout = 5000000;
	last_packet_timeout_timer = CNET_start_timer(EV_TIMER2, packet_timeout, 0);

	CHECK(CNET_enable_application(ALLNODES));
}
