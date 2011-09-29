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

// here the second parameter is the length of msg of packet!
static CnetTimerID last_packet_timeout_timer = NULLTIMER;
VECTOR rb;

void down_pieces_to_datalink(char *packet, size_t length, int choose_link) {
	//printf("send packet pieces to data link at %s\n", nodeinfo.nodename);

	size_t maxPacketLength = linkinfo[choose_link].mtu - PACKET_HEADER_SIZE;

	NL_PACKET *tempPacket = (NL_PACKET *) packet;

	if(tempPacket->kind == NL_ACK || tempPacket->kind == NL_ERR_ACK || tempPacket->kind == NL_ERR_ACK_RESENT){
		//fprintf(stdout, "ack\n");
		CHECK(down_to_datalink(choose_link, (char *) tempPacket,
						PACKET_HEADER_SIZE));
	} else if(tempPacket->kind != NL_DATA){
		return;
	}
	//fprintf(stdout, "data! plength = %d length = %d srclength = %d \n", (int)tempPacket->length, (int) length, (int)tempPacket->src_packet_length);

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
	piecePacket.is_resent = tempPacket->is_resent;
	piecePacket.piece_checksum = tempPacket->piece_checksum;
	while (tempLength > maxPacketLength) {
		//printf("packet remains %d  bytes\n", tempLength);
		piecePacket.length = maxPacketLength;
		memcpy(piecePacket.msg, str, maxPacketLength);
		piecePacket.piece_checksum = CNET_crc32(
				(unsigned char *) (piecePacket.msg), piecePacket.length);

		CHECK(down_to_datalink(choose_link, (char *) &piecePacket,
				PACKET_SIZE(piecePacket)));
		//("piece %d down_to_datalink\n", piecePacket.pieceNumber);

		str = str + maxPacketLength;
		piecePacket.pieceNumber = piecePacket.pieceNumber + 1;
		tempLength = tempLength - maxPacketLength;
	}

	piecePacket.pieceEnd = 1;
	piecePacket.length = tempLength;
	piecePacket.piece_checksum = CNET_crc32(
			(unsigned char *) (piecePacket.msg), piecePacket.length);
	//printf("last piece contains %d  bytes, pieceNumber = %d\n\n", tempLength ,piecePacket.pieceNumber);

	memcpy(piecePacket.msg, str, tempLength);
	piecePacket.piece_checksum = CNET_crc32(
			(unsigned char *) (piecePacket.msg), piecePacket.length);

	////("Required link is provided link = %d\n", choose_link);
	CHECK(down_to_datalink(choose_link, (char *) &piecePacket,
			PACKET_SIZE(piecePacket)));
	//("last piece %d down_to_datalink\n", piecePacket.pieceNumber);
	////("Provided link = %d sent! \n", choose_link);
}

/* ----------------------------------------------------------------------- */

/*  flood() IS A BASIC ROUTING STRATEGY WHICH TRANSMITS THE OUTGOING PACKET
 ON EITHER THE SPECIFIED LINK, OR ALL BEST-KNOWN LINKS WHILE AVOIDING
 ANY OTHER SPECIFIED LINK.
 */
static void flood(char *packet, size_t length, int choose_link, int avoid_link) {
	//printf("flood\n\n");
	NL_PACKET *p = (NL_PACKET *) packet;

	/*  REQUIRED LINK IS PROVIDED - USE IT */
	if (choose_link != 0) {
		down_pieces_to_datalink(packet, p->length, choose_link);
	}

	/*  OTHERWISE, CHOOSE THE BEST KNOWN LINKS, AVOIDING ANY SPECIFIED ONE */
	else {
		int links_wanted = NL_linksofminhops(p->dest);
		int link;

		for (link = 1; link <= nodeinfo.nlinks; ++link) {
			if (link == avoid_link) /* possibly avoid this one */
				continue;
			if (links_wanted & (1 << link)) /* use this link if wanted */
				down_pieces_to_datalink(packet, p->length, link);
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
	//p.checksum = CNET_ccitt((unsigned char *) (p.msg), p.src_packet_length);
	p.checksum = CNET_crc32((unsigned char *) (p.msg), p.src_packet_length);
	p.piece_checksum = CNET_crc32((unsigned char *) (p.msg),
			p.src_packet_length);
	p.trans_time = 0;
	p.is_resent = 0;

	/*
	 //lastPacket = &p;
	 printf(
	 "packet generated, src = %d, des = %d, seqno = %d, send_length = %d, checksum = %d\n\n",
	 nodeinfo.address, p.dest, p.seqno, p.length, p.checksum);
	 */
	update_last_packet(&p);
	flood((char *) &p, PACKET_SIZE(p), 0, 0);

}

static void NL_update_lastackexpected(CnetAddr address) {
	NL_table[find_address(address)].last_ack_expected = NL_table[find_address(
			address)].ackexpected;
}

//static int err_count = 0;

/*  up_to_network() IS CALLED FROM THE DATA LINK LAYER (BELOW) TO ACCEPT
 A PACKET FOR THIS NODE, OR TO RE-ROUTE IT TO THE INTENDED DESTINATION.
 */
int up_to_network(char *packet, size_t length, int arrived_on_link) {
	//printf("up to network at hop %d\n", nodeinfo.address);
	NL_PACKET *p = (NL_PACKET *) packet;
	if (p->src == nodeinfo.address) {
		printf(
				"drop a packet at %d, src = %d, des = %d, seqno = %d, length = %d\n\n",
				nodeinfo.address, p->src, p->dest, p->seqno, p->length);
		return 0;
	}

	//printf("up to network at %d (from %d to %d)\n", nodeinfo.address, p->src, p->dest);
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
		printf("receive a packet from src %d\n");
		switch (p->kind) {
		case NL_DATA:
			printf("NL_DATA, seqno = %d, expected = %d", p->seqno,
					NL_packetexpected(p->src));
			if (p->seqno == NL_packetexpected(p->src)) {
				//length = p->length;
				//memcpy(rb[p->src], (char *) p->msg, length);
				//rb[p->src] = rb[p->src] + length;
				//packet_length[p->src] += length;
				if (RB_save_msg_link(rb, p, arrived_on_link) == 0)
					return 0;

				if (p->pieceEnd) {
					RB_copy_whole_msg_link(rb, p, arrived_on_link);
					up_to_application(p, arrived_on_link);
					return 0;
				}
			} else if (p->seqno < NL_packetexpected(p->src)) {
				//in this case means the ack send before has not been received
				printf("outdated frame come!\n");

				RB_save_msg_link(rb, p, arrived_on_link);

				if (p->pieceEnd) {
					RB_copy_whole_msg_link(rb, p, arrived_on_link);
					send_ack(p, arrived_on_link, 2);
					return 0;
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
					//NL_inc_resent_times(p->src); // for debug
					flood((char *) packettoresend, len, 0, 0);

				} else {
					printf(
							"this packet has already been resent, dont resent it again\n");
				}
			} else {
				printf(
						"this packet has already been correct received, dont resent it again\n");
			}
			//printf("\n");
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
				//printf("this packet has been resent %d times before this time\n", NL_get_resent_times(p->src)); //for debug
				//NL_inc_resent_times(p->src); // for debug
				int len = PACKET_HEADER_SIZE + packettoresend->length;
				packettoresend->is_resent = 1;
				flood((char *) packettoresend, len, 0, 0);

			} else {
				printf(
						"this packet has already been correct received, dont resent it again\n");
			}
			//("\n");
			break;
		default:
			printf("it's nothing!====================\n");
			break;
		}
	}
	/* THIS PACKET IS FOR SOMEONE ELSE */
	else {
		if (p->hopcount < MAXHOPS) { /* if not too many hops... */
			//length = p->length;
			//memcpy(rb[p->src], (char *) p->msg, length);
			//rb[p->src] = rb[p->src] + length;
			if (p->kind == NL_ACK || p->kind == NL_ERR_ACK || p->kind
					== NL_ERR_ACK_RESENT) {
				//fprintf(stdout,"route ack!\n");
				route_packet(p, arrived_on_link);
			} else {
				//fprintf(stdout,"route data!\n");


				RB_save_msg_link(rb, p, arrived_on_link);
				//("finish new rb save msg\n");
				if (p->pieceEnd) {

					RB_copy_whole_msg_link(rb, p, arrived_on_link);
					//printf("finish copy whole msg\n");
					//printf("p->length after copy = %d\n", p->length);
					route_packet(p, arrived_on_link);


				}
			}
		} else {/* silently drop */
			;

		}
	}
	return 0;
}

void up_to_application(NL_PACKET *p, int arrived_on_link) {
	//debug
	size_t length = p->pieceNumber * (linkinfo[arrived_on_link].mtu
			- PACKET_HEADER_SIZE) + p->length;
	//length = packet_length[p->src];
	//packet_length[p->src] = 0;
	if (length != p->src_packet_length)
		return;
	uint32_t p_checksum = p->checksum;

	//fprintf(stdout, "length = %d\n", (int) p->src_packet_length);
	uint32_t checksum = CNET_crc32((unsigned char *) (p->msg),
			p->src_packet_length);

	/*
	 if (p->is_resent) {
	 printf(
	 "%d received a resent packet, src = %d, des = %d, seqno = %d, send_length = %d, receive_length = %d \n",
	 nodeinfo.address, p->src, p->dest, p->seqno,
	 p->src_packet_length, length);
	 } else {
	 printf(
	 "%d received a packet, src = %d, des = %d, seqno = %d, send_length = %d, receive_length = %d \n",
	 nodeinfo.address, p->src, p->dest, p->seqno,
	 p->src_packet_length, length);
	 }
	 printf("last_piece_trans_time = %d, hopcount = %d\n", p->trans_time,
	 p->hopcount);
	 printf("src_checksum = %d calc_checksum = %d, ", p_checksum, checksum);
	 */
	if (p_checksum != checksum) {
		send_ack(p, arrived_on_link, 1);
	} else { // received a correct packet
		if ((CNET_write_application(p->msg, &length)) == -1) {
			if (p->msg) {
				/*
				 fprintf(stdout, "node: %d, p->msg pointer invalid\n",
				 nodeinfo.address);
				 */
			} else {
				//fprintf(stdout, "node: %d, length invalid, length = %d\n",
						//nodeinfo.address, (int) length);
			}
		}
		send_ack(p, arrived_on_link, 0);
	}
}

void route_packet(NL_PACKET *p, int arrived_on_link) {
	//size_t length = p->pieceNumber * (linkinfo[arrived_on_link].mtu
	//		- PACKET_HEADER_SIZE) + p->length;
	//length = packet_length[p->src];
	//packet_length[p->src] = 0;

	if(p->src_packet_length <= 0 || p->src_packet_length > linkinfo[arrived_on_link].mtu - PACKET_HEADER_SIZE )
		return;
	NL_savehopcount(p->src, p->trans_time, arrived_on_link);
	p->pieceNumber = 0;
	p->pieceEnd = 1;
	p->length = p->src_packet_length;
	//memcpy(wholePacket.msg, receiveBuffer[p->src], length);
	//rb[p->src] = &receiveBuffer[p->src][0];

	flood((char *) p, p->length + PACKET_HEADER_SIZE, 0, arrived_on_link);
}

//mode_code = 1: NL_ERR_ACK, 0:NL_ACK, 2:outdated frame ack

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
		//flood((char *) p, PACKET_HEADER_SIZE, arrived_on_link, 0);
	} else if (mode_code == 2) {
		printf("ack for outdated frame\n");
		p->kind = NL_ACK;
	}
	//rb[p->src] = &receiveBuffer[p->src][0];


	/* actually we just need to set p->length to 0 */
	p->hopcount = 0;
	p->pieceNumber = 0;
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

	printf("\n");
}

void print_msg(char * msg, size_t length) {
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
	//NL_table[index].last_ack_expected = NL_ackexpected(last->dest);
	//free(temp);
}

NL_PACKET * get_last_packet(CnetAddr address) {
	int index = find_address(address);
	return (NL_PACKET *) &(NL_table[index].lastpacket);
}

static void resend_last_packet(int nl_table_id) {
	NL_PACKET packettoresend = NL_table[nl_table_id].lastpacket;

	int len = PACKET_HEADER_SIZE + packettoresend.length;
	packettoresend.is_resent = 1;
	NL_set_has_resent(NL_table[nl_table_id].address, 1);
	//NL_inc_resent_times(p->src); // for debug
	flood((char *) &packettoresend, len, 0, 0);

}

static void timeout_check(CnetEvent ev, CnetTimerID timer, CnetData data) {

	CnetTime packet_timeout;
	printf("======================\n");
	printf("timeout_check!\n");
	printf("======================\n");
	int i;
	for (i = 0; i < NL_table_size; i++) {
		//printf("no: %d, ", i);
		if (NL_table[i].last_ack_expected == NL_table[i].ackexpected
				&& NL_table[i].nextpackettosend > NL_table[i].ackexpected) {
			printf("packet no %d loss! resend...\n",
					NL_table[i].last_ack_expected);
			resend_last_packet(i);

		} else if (NL_table[i].last_ack_expected < NL_table[i].ackexpected) {
			printf("packet no %d not loss!\n", NL_table[i].last_ack_expected);
			NL_table[i].last_ack_expected = NL_table[i].ackexpected;
		} else {
			printf("Unknow condition!\n");
			printf("lastackexpected : %d\n", NL_table[i].last_ack_expected);
		}
	}
	printf("\n");
	packet_timeout = 5000000;
	last_packet_timeout_timer = CNET_start_timer(EV_TIMER2, packet_timeout, 0);
}

/* ----------------------------------------------------------------------- */
EVENT_HANDLER( reboot_node) {
	if (nodeinfo.nlinks > 32) {
		exit(1);
	}
	/*
	 for (int i = 0; i <= 255; i++) {
	 rb[i] = receiveBuffer[i];
	 //packet_length[i] = 0;
	 }*/
	//RB_init(rb);
	rb = vector_new();
	//memset(packet_length, 0, 256*sizeof(size_t));
	reboot_DLL();
	reboot_NL_table();

	CHECK(CNET_set_handler(EV_APPLICATIONREADY, down_to_network, 0));

	CHECK(CNET_set_handler(EV_TIMER2, timeout_check, 0));

	CnetTime packet_timeout;

	packet_timeout = 5000000;
	last_packet_timeout_timer = CNET_start_timer(EV_TIMER2, packet_timeout, 0);

	CHECK(CNET_enable_application(ALLNODES));
}
