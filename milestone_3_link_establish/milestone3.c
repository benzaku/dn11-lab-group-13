#include <cnet.h>
#include <stdlib.h>
#include <limits.h>

#include "milestone3.h"
#include "dll_basic.c"
#include "queue.c"
#include "nl_table.c"

#define	MAXHOPS		100

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

void printmsg(char * msg, size_t length) {
	size_t i;
	printf("msg :");
	for (i = 0; i < length; i++) {
		printf("%d", (unsigned int) msg[i]);
	}
	printf("\n");
}

/* ----------------------------------------------------------------------- */

/*  flood3() IS A BASIC ROUTING STRATEGY WHICH TRANSMITS THE OUTGOING PACKET
 ON EITHER THE SPECIFIED LINK, OR ALL BEST-KNOWN LINKS WHILE AVOIDING
 ANY OTHER SPECIFIED LINK.
 */
static void flood3(char *packet, size_t length, int choose_link, int avoid_link) {
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

		for (link = 1; link <= nodeinfo.nlinks; ++link) {

			if (link == avoid_link) /* possibly avoid this one */{
				continue;
			}
			if (links_wanted & (1 << link)) /* use this link if wanted */
			{
				//printf("links wanted = %d\n", links_wanted);
				if (linkinfo[link].mtu < p->min_mtu)
					p->min_mtu = linkinfo[link].mtu;
				//printf("down to datalink! link = %d, length = %d\n", link,
				//	length);

				CHECK(down_to_datalink(link, packet, length));
			}
		}
	}
}

/*  down_to_network() RECEIVES NEW MESSAGES FROM THE APPLICATION LAYER AND
 PREPARES THEM FOR TRANSMISSION TO OTHER NODES.
 */
static EVENT_HANDLER( down_to_network) {
	NL_PACKET p;
	NL_PACKET test;

	p.length = sizeof(p.msg);
	//printf("p.length = %d, MAX_MESSAGE_SIZE = %d", p.length, MAX_MESSAGE_SIZE);
	CHECK(CNET_read_application(&p.dest, p.msg, &p.length));
	CNET_disable_application(p.dest);

	NL_setminmtu(p.dest, 0);

	p.src = nodeinfo.address;
	p.kind = NL_DATA;
	p.hopcount = 0;
	p.seqno = NL_nextpackettosend(p.dest);

	test.src = nodeinfo.address;
	test.dest = p.dest;
	test.kind = NL_TEST;
	test.hopcount = 0;
	test.seqno = 0;
	test.min_mtu = INT_MAX;
	test.length = 0;

	//printmsg((char *) &p, PACKET_SIZE(p));
	NL_updatelastsendtest(&test);
	flood3((char *) &test, PACKET_HEADER_SIZE, 0, 0);

	//flood3((char *) &p, PACKET_SIZE(p), 0, 0);
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
	//printf("me = %d, dest = %d =======\n", nodeinfo.address, p->dest);
	/*  IS THIS PACKET IS FOR ME? */
	if (p->dest == nodeinfo.address) {
		//printf("it's for me!\n========");
		switch (p->kind) {

		case NL_TEST_ACK:

			//if (p->min_mtu <= NL_minmtu(p->src))
			//break;

			printf(
					"NL_TEST_ACK come! src = %d, dest = %d, cur = %d, min mtu = %d\n",
					p->src, p->dest, nodeinfo.address, p->min_mtu);

			NL_savehopcount(p->src, p->hopcount, arrived_on_link);
			NL_updateminmtu(p->src, p->min_mtu, arrived_on_link);
			break;

		case NL_TEST:
			//			if (p->min_mtu != 0 && p->min_mtu <= NL_minmtu(p->src))
			//			break;
			/*
			 if (NL_gettesthascome(p->src))
			 break;
			 */
			printf(
					"NL_TEST come! src = %d, dest = %d, cur = %d, min mtu = %d\n",
					p->src, p->dest, nodeinfo.address, p->min_mtu);

			NL_savehopcount(p->src, p->hopcount, arrived_on_link);
			NL_updateminmtu(p->src, p->min_mtu, arrived_on_link);
			NL_inctesthascome(p->src);
			tempaddr = p->src;
			p->src = p->dest;
			p->dest = tempaddr;

			p->kind = NL_TEST_ACK;
			p->hopcount = 0;
			p->length = 0;
			flood3(packet, PACKET_HEADER_SIZE, arrived_on_link, 0);
			break;

		case NL_DATA:
			if (p->seqno == NL_packetexpected(p->src)) {
				CnetAddr tmpaddr;

				length = p->length;
				CHECK(CNET_write_application(p->msg, &length));
				printf("application written!\n");
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

		CnetAddr tmp;
		NL_PACKET temp_p;
		//printf("hopcount = %d\n", p->hopcount);
		//printf("MAXHOPS = %d\n", MAXHOPS);
		if (p->hopcount < MAXHOPS) { /* if not too many hops... */
			//printf("other's frame!\n");


			NL_savehopcount(p->src, p->hopcount, arrived_on_link);
			NL_updateminmtu(p->src, p->min_mtu, arrived_on_link);

			temp_p.src = p->src;
			temp_p.dest = p->dest;
			temp_p.min_mtu = p->min_mtu;
			temp_p.hopcount = p->hopcount;

			/* retransmit on best links *except* the one on which it arrived */
			//if (CNET_carrier_sense(arrived_on_link) == 0)

			if (NL_minmtu(temp_p.dest) != 0 && NL_minmtu(temp_p.dest) != -1) {

				if (temp_p.min_mtu > NL_minmtu(temp_p.dest)) {
					temp_p.min_mtu = NL_minmtu(temp_p.dest);
				}
				//temp_p.hopcount = NL_gethopcount(temp_p.src) + NL_gethopcount(
				//	temp_p.dest);
				temp_p.hopcount = NL_gethopcount(temp_p.dest);
				tmp = temp_p.dest;
				temp_p.dest = temp_p.src;

				temp_p.src = tmp;

				temp_p.kind = NL_TEST_ACK;
				temp_p.length = 0;
				printf("send query result of %d to %d, min_mtu = %d\n",
						temp_p.src, temp_p.dest, temp_p.min_mtu);
				flood3((char *) &temp_p, PACKET_HEADER_SIZE, arrived_on_link, 0);
			}

			//printf("forwarding src = %d to dest = %d\n", p->src, p->dest);
			//flood3(packet, length, 0, arrived_on_link);

		} else

			printf("drop\n");
		/* silently drop */;
	}
	return (0);
}

static void timeout_check(CnetEvent ev, CnetTimerID timer, CnetData data) {

	CnetTime packet_timeout;
	//printf("======================\n");
	//printf("timeout_check!\n");
	//printf("======================\n");
	int i;
	if (NL_gettablesize() != 0) {
		for (i = 0; i < NL_gettablesize(); i++) {
			if (NL_getminmtubyid(i) == 0) {
				flood3((char *) NL_getlastsendtest(NL_getdestbyid(i)),
						PACKET_HEADER_SIZE, 0, 0);
				printf("resend last test dest = %d\n", NL_getdestbyid(i));
			}
		}
	}
	//printf("\n");
	packet_timeout = 75000000;
	last_packet_timeout_timer = CNET_start_timer(EV_TIMER2, packet_timeout, 0);
}

/* ----------------------------------------------------------------------- */

EVENT_HANDLER( reboot_node) {
	if (nodeinfo.nlinks > 32) {
		fprintf(stderr, "flood3 flooding will not work here\n");
		exit(1);
	}

	reboot_DLL();
	reboot_NL_table();

	CHECK(CNET_set_handler(EV_APPLICATIONREADY, down_to_network, 0));
	CHECK(CNET_set_handler(EV_TIMER2, timeout_check, 0));
	CHECK(CNET_enable_application(ALLNODES));

	CnetTime packet_timeout;

	packet_timeout = 50000000;
	last_packet_timeout_timer = CNET_start_timer(EV_TIMER2, packet_timeout, 0);
}
