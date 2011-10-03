#include <cnet.h>
#include <stdlib.h>
#include <limits.h>

#include "milestone3.h"
#include "dll_basic.c"
#include "queue.c"
#include "nl_table.c"
#include "receive_buffer.c"

#define	MAXHOPS		100

/*  This file implements a much better flooding algorithm than those in
 both flooding1.c and flooding2.c. As Network Layer packets are processed,
 the information in their headers is used to update the NL table.

 The minimum observed hopcount to each (potential) remote destination
 is remembered by the NL table, as is the link on which these packets
 arrive. This link is later used to route packets leaving for that node.

 The routine NL_savetranstime() is called for both NL_DATA and NL_ACK
 packets, and we even "steal" information from Network Layer packets
 that don't belong to us!

 I don't think it's a flooding algorithm any more Toto.

 This flooding algorithm exhibits an efficiency which improves over time
 (as the NL table "learns" more).  Over the 8 nodes in the AUSTRALIA.MAP
 file, the initial efficiency is the same as that of flooding1.c (about
 2%) but as the NL table improves, the efficiency rises to over 64%.
 */

CnetTimerID last_test_timeout_timer = NULLTIMER;
CnetTimerID last_packet_timeout_timer = NULLTIMER;
VECTOR rb;
void printmsg(char * msg, size_t length) {
	size_t i;
	printf("msg :");
	for (i = 0; i < length; i++) {
		printf("%d", (unsigned int) msg[i]);
	}
	printf("\n");
}

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

/* ----------------------------------------------------------------------- */

/*  flood3() IS A BASIC ROUTING STRATEGY WHICH TRANSMITS THE OUTGOING PACKET
 ON EITHER THE SPECIFIED LINK, OR ALL BEST-KNOWN LINKS WHILE AVOIDING
 ANY OTHER SPECIFIED LINK.
 */
static void flood_test(char *packet, size_t length, int choose_link,
		int avoid_link) {
	NL_PACKET *p = (NL_PACKET *) packet;
	/*  REQUIRED LINK IS PROVIDED - USE IT */
	if (choose_link != 0) {
		if (linkinfo[choose_link].mtu < p->min_mtu)
			p->min_mtu = linkinfo[choose_link].mtu;
		CHECK(down_to_datalink(choose_link, packet, length));
	}

	/*  OTHERWISE, CHOOSE THE BEST KNOWN LINKS, AVOIDING ANY SPECIFIED ONE */
	else {

		int links_wanted = NL_linksofminhops(p->dest);

		int link;
		int mtu_ori = p->min_mtu;

		for (link = 1; link <= nodeinfo.nlinks; ++link) {

			if (link == avoid_link) /* possibly avoid this one */{
				continue;
			}
			if (links_wanted & (1 << link)) /* use this link if wanted */
			{
				//printf("links wanted = %d\n", links_wanted);
				if (linkinfo[link].mtu < mtu_ori)
					p->min_mtu = linkinfo[link].mtu;
				//printf("down to datalink! link = %d, length = %d\n", link,
				//	length);

				CHECK(down_to_datalink(link, packet, length));
			}
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

void update_last_packet(NL_PACKET *last) {
	int index = find_address(last->dest);
	memcpy(&(NL_table[index].lastpacket), last, PACKET_SIZE((*last)));
}
/*  down_to_network() RECEIVES NEW MESSAGES FROM THE APPLICATION LAYER AND
 PREPARES THEM FOR TRANSMISSION TO OTHER NODES.
 */
static EVENT_HANDLER( down_to_network) {
	NL_PACKET p;

	p.length = sizeof(p.msg);
	//printf("p.length = %d, MAX_MESSAGE_SIZE = %d", p.length, MAX_MESSAGE_SIZE);
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

	if (NL_minmtu(p.dest) <= 0) {
		NL_PACKET test;
		test.src = nodeinfo.address;
		test.dest = p.dest;
		test.kind = NL_TEST;
		test.hopcount = 0;
		test.trans_time = 0;
		test.seqno = 0;
		test.min_mtu = INT_MAX;
		test.length = 0;

		//printmsg((char *) &p, PACKET_SIZE(p));
		NL_updatelastsendtest(&test);
		flood_test((char *) &test, PACKET_HEADER_SIZE, 0, 0);
	} else {
		if (NL_getnoinitcount() == 0) {
			size_t mtu = NL_minmtu(p.dest);
			piece_to_flood((char *) &p, mtu);
		}
	}

}

int check_valid_address(CnetAddr addr) {
	if (addr < 0 || addr > 255)
		return 0;
	else
		return 1;
}

void send_ack(NL_PACKET *p, int arrived_on_link, unsigned short int mode_code) {
	CnetAddr tmpaddr;
	if (mode_code == 1) {
		printf("error packet! src = %d, dest = %d, seqno = %d\n", p->src,
				p->dest, p->seqno);
//		NL_savehopcount(p->src, p->hopcount, arrived_on_link);
		NL_savetranstime(p->src, p->trans_time, arrived_on_link);
		if (p->is_resent == 1)
			p->kind = NL_ERR_ACK_RESENT;
		else
			p->kind = NL_ERR_ACK;

	} else if (mode_code == 0) {
		printf("correct packet src = %d, dest = %d, seqno = %d\n", p->src,
				p->dest, p->seqno);
		inc_NL_packetexpected(p->src);
//		NL_savehopcount(p->src, p->hopcount, arrived_on_link);
		NL_savetranstime(p->src, p->trans_time, arrived_on_link);
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

/*  up_to_network() IS CALLED FROM THE DATA LINK LAYER (BELOW) TO ACCEPT
 A PACKET FOR THIS NODE, OR TO RE-ROUTE IT TO THE INTENDED DESTINATION.
 */
int up_to_network(char *packet, size_t length, int arrived_on_link) {

	NL_PACKET *p = (NL_PACKET *) packet;

	if (p->src == nodeinfo.address)
		return 0;

	CnetAddr tempaddr;
	//printf("up to network\n");
	++p->hopcount; /* took 1 hop to get here */
	int mtu = linkinfo[arrived_on_link].mtu;
		p->trans_time += ((CnetTime) 8000 * 1000 * mtu
				/ linkinfo[arrived_on_link].bandwidth
				+ linkinfo[arrived_on_link].propagationdelay) * 100 / mtu;
	//printf("me = %d, dest = %d =======\n", nodeinfo.address, p->dest);
	/*  IS THIS PACKET IS FOR ME? */
	if (p->dest == nodeinfo.address) {
		//printf("it's for me!\n========");
		switch (p->kind) {

		case NL_TEST_ACK:

			if ((!check_valid_address(p->src))
					|| (!check_valid_address(p->dest)))
				break;
			//if (p->min_mtu <= NL_minmtu(p->src))
			//break;

			printf(
					"NL_TEST_ACK come! src = %d, dest = %d, cur = %d, min mtu = %d\n",
					p->src, p->dest, nodeinfo.address, p->min_mtu);

//			NL_savehopcount(p->src, p->hopcount, arrived_on_link);
			NL_savetranstime(p->src, p->trans_time, arrived_on_link);
			NL_updateminmtu(p->src, p->min_mtu, arrived_on_link);
			break;

		case NL_TEST:
			//			if (p->min_mtu != 0 && p->min_mtu <= NL_minmtu(p->src))
			//			break;
			/*
			 if (NL_gettesthascome(p->src))
			 break;
			 */
			if ((!check_valid_address(p->src))
					|| (!check_valid_address(p->dest)))
				break;
			printf(
					"NL_TEST come! src = %d, dest = %d, cur = %d, min mtu = %d\n",
					p->src, p->dest, nodeinfo.address, p->min_mtu);

//			NL_savehopcount(p->src, p->hopcount, arrived_on_link);
			NL_savetranstime(p->src, p->trans_time, arrived_on_link);
			NL_updateminmtu(p->src, p->min_mtu, arrived_on_link);
			NL_inctesthascome(p->src);
			tempaddr = p->src;
			p->src = p->dest;
			p->dest = tempaddr;

			p->kind = NL_TEST_ACK;
			p->hopcount = 0;
			p->trans_time = 0;
			p->length = 0;
			flood_test(packet, PACKET_HEADER_SIZE, arrived_on_link, 0);
			break;

		case NL_DATA:
			if (p->seqno == NL_packetexpected(p->src)) {
				if (RB_save_msg_link(rb, p, arrived_on_link) == 2) {
					/*
					 all pieces are arrived
					 now get the whole msg from buffer and write it in applicaiton layer
					 */
					RB_copy_whole_msg_link(rb, p, arrived_on_link);
					CHECK(CNET_write_application((char*) p->msg,
							&p->src_packet_length));
					send_ack(p, arrived_on_link, 0);
					return 0;

				} else if (p->pieceEnd || p->is_resent) {
					/*
					 last piece arrives, now check the missing frame position
					 */

					// check which piece is missing and require to resend this piece

					//TODO: check which piece is missing
					//TODO: and require to resend this piece

				}
			}
			break;

		case NL_ACK:
			if (p->seqno == NL_ackexpected(p->src)) {

				printf("here %d ACK come! from %d \n", p->dest, p->src);
				inc_NL_ackexpected(p->src);
//				NL_savehopcount(p->src, p->hopcount, arrived_on_link);
				NL_savetranstime(p->src, p->trans_time, arrived_on_link);
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

		if ((!check_valid_address(p->src)) || (!check_valid_address(p->dest)))
			return (0);

		CnetAddr tmp;
		NL_PACKET temp_p;
		//printf("hopcount = %d\n", p->hopcount);
		//printf("MAXHOPS = %d\n", MAXHOPS);
		if (p->hopcount < MAXHOPS) { /* if not too many hops... */
			//printf("other's frame!\n");
			if (p->kind == NL_TEST_ACK) {
				return 0;
			}
			if (p->kind == NL_TEST) {
//				NL_savehopcount(p->src, p->hopcount, arrived_on_link);
				NL_savetranstime(p->src, p->trans_time, arrived_on_link);
				NL_updateminmtu(p->src, p->min_mtu, arrived_on_link);

				temp_p.src = p->src;
				temp_p.dest = p->dest;
				temp_p.min_mtu = p->min_mtu;
				temp_p.hopcount = p->hopcount;
				temp_p.trans_time = p->trans_time;

				/* retransmit on best links *except* the one on which it arrived */
				//if (CNET_carrier_sense(arrived_on_link) == 0)

				if (NL_minmtu(temp_p.dest) != 0 && NL_minmtu(temp_p.dest) != -1) {

					if (temp_p.min_mtu > NL_minmtu(temp_p.dest)) {
						temp_p.min_mtu = NL_minmtu(temp_p.dest);
					}
					//temp_p.hopcount = NL_gethopcount(temp_p.src) + NL_gethopcount(
					//	temp_p.dest);
					temp_p.hopcount = NL_gethopcount(temp_p.dest);
					temp_p.trans_time = NL_gettranstime(temp_p.dest);
					tmp = temp_p.dest;
					temp_p.dest = temp_p.src;

					temp_p.src = tmp;

					temp_p.kind = NL_TEST_ACK;
					temp_p.length = 0;
					printf("send query result of %d to %d, min_mtu = %d\n",
							temp_p.src, temp_p.dest, temp_p.min_mtu);
					flood_test((char *) &temp_p, PACKET_HEADER_SIZE,
							arrived_on_link, 0);
				}
			} else {
//				NL_savehopcount(p->src, p->hopcount, arrived_on_link);
				flood(packet, length, 0, arrived_on_link);
			}

			//printf("forwarding src = %d to dest = %d\n", p->src, p->dest);
			//flood3(packet, length, 0, arrived_on_link);

		} else

			printf("drop\n");
		/* silently drop */;
	}
	return (0);
}

void resend_last_packet(int nl_table_id) {
	NL_PACKET packettoresend = NL_table[nl_table_id].lastpacket;

	//int len = PACKET_HEADER_SIZE + packettoresend.length;
	packettoresend.is_resent = 1;
	NL_set_has_resent(NL_table[nl_table_id].address, 1);
	//NL_inc_resent_times(p->src); // for debug
	size_t mtu = NL_table[nl_table_id].min_mtu;
	piece_to_flood((char *) &packettoresend, mtu);
}

void send_all_saved_packet() {
	int i = 0;
	for (i = 0; i < NL_gettablesize(); i++) {
		NL_PACKET packettoresend = NL_table[i].lastpacket;
		size_t mtu = NL_table[i].min_mtu;
		piece_to_flood((char *) &packettoresend, mtu);
	}
}

static void timeout_check_test(CnetEvent ev, CnetTimerID timer, CnetData data) {

	CnetTime test_timeout;
	//printf("======================\n");
	//printf("timeout_check!\n");
	//printf("======================\n");
	int i;
	int count = 0;
	if (NL_gettablesize() != 0) {
		printf("======================\n");
		for (i = 0; i < NL_gettablesize(); i++) {
			if (NL_getminmtubyid(i) == 0) {

				printf("nl_getdestbyid = %d, id = %d\n", NL_getdestbyid(i), i);
				NL_PACKET * temp = NL_getlastsendtest(NL_getdestbyid(i));
				printf("src = %d, dest = %d \n", temp->src, temp->dest);

				flood_test((char *) NL_getlastsendtest(NL_getdestbyid(i)),
						PACKET_HEADER_SIZE, 0, 0);
				//printf("resend last test dest = %d\n", NL_getdestbyid(i));
			} else {
				count++;
				//if (count == NL_gettablesize())
				//resend_last_packet(i);
			}
		}

	}

	/*
	 if(NL_getnoinitcount() == 0){
	 printf("no init count! table size = %d, count = %d\n", NL_gettablesize(), count);
	 for (i = 0; i < NL_table_size; i++) {
	 //printf("no: %d, ", i);
	 if (NL_table[i].last_ack_expected == NL_table[i].ackexpected
	 && NL_table[i].nextpackettosend > NL_table[i].ackexpected) {
	 printf("packet no %d loss! resend...\n",
	 NL_table[i].last_ack_expected);
	 resend_last_packet(i);

	 } else if (NL_table[i].last_ack_expected < NL_table[i].ackexpected) {
	 printf("packet no %d not loss!\n",
	 NL_table[i].last_ack_expected);
	 NL_table[i].last_ack_expected = NL_table[i].ackexpected;
	 } else {
	 printf("Unknow condition!\n");
	 printf("lastackexpected : %d\n", NL_table[i].last_ack_expected);
	 }
	 }
	 }
	 */

	if (count < NL_gettablesize()) {
		//printf("\n");
		//test_timeout = (NL_gettablesize() - count) * 5000000;
		test_timeout = NL_gettablesize() * 1250000;
		//test_timeout = 45000000;
		last_test_timeout_timer = CNET_start_timer(EV_TIMER2, test_timeout, 0);
	} else {
		printf("link establish finished!\n====================\n");
		send_all_saved_packet();

	}

	/*else{
	 for(i = 0; i < NL_gettablesize(); i ++){
	 resend_last_packet(i);
	 }
	 test_timeout = 75000000;
	 last_test_timeout_timer = CNET_start_timer(EV_TIMER2, test_timeout, 0);

	 }
	 */
}

static void packet_timeout(CnetEvent ev, CnetTimerID timer, CnetData data) {

	CnetTime test_timeout;
	//printf("======================\n");
	//printf("timeout_check!\n");
	//printf("======================\n");
	int i;
	if (NL_getnoinitcount() == 0) {
		for (i = 0; i < NL_table_size; i++) {
			//printf("no: %d, ", i);
			if (NL_table[i].last_ack_expected == NL_table[i].ackexpected
					&& NL_table[i].nextpackettosend > NL_table[i].ackexpected) {
				printf("packet no %d loss! resend...\n",
						NL_table[i].last_ack_expected);
				//resend_last_packet(i);

			} else if (NL_table[i].last_ack_expected < NL_table[i].ackexpected) {
				printf("packet no %d not loss!\n",
						NL_table[i].last_ack_expected);
				NL_table[i].last_ack_expected = NL_table[i].ackexpected;
			} else {
				printf("Unknow condition!\n");
				printf("lastackexpected : %d\n", NL_table[i].last_ack_expected);
			}
		}
	}
	//printf("\n");
	//test_timeout = 75000000;
	test_timeout = NL_gettablesize() * 5000000;
	last_test_timeout_timer = CNET_start_timer(EV_TIMER3, test_timeout, 0);
}

/* ----------------------------------------------------------------------- */

EVENT_HANDLER( reboot_node) {
	if (nodeinfo.nlinks > 32) {
		fprintf(stderr, "flood3 flooding will not work here\n");
		exit(1);
	}
	rb = vector_new();
	reboot_DLL();
	reboot_NL_table();

	CHECK(CNET_set_handler(EV_APPLICATIONREADY, down_to_network, 0));
	CHECK(CNET_set_handler(EV_TIMER2, timeout_check_test, 0));

	CHECK(CNET_set_handler(EV_TIMER3, packet_timeout, 0));

	CHECK(CNET_enable_application(ALLNODES));

	CnetTime test_timeout;

	test_timeout = 20000000;
	last_test_timeout_timer = CNET_start_timer(EV_TIMER2, test_timeout, 0);

	CnetTime packet_time;
	packet_time = 5000000;
	//last_packet_timeout_timer = CNET_start_timer(EV_TIMER3, packet_time, 0);
}
