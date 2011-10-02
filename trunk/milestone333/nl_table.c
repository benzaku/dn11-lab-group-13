#include <cnet.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "nl_table.h"
#include "nl_packet.h"
#include "milestone3.h"

// ---- A SIMPLE NETWORK LAYER SEQUENCE TABLE AS AN ABSTRACT DATA TYPE ----

typedef struct {
	CnetAddr address; // ... of remote node
	int ackexpected; // packet sequence numbers to/from node
	int nextpackettosend;
	int packetexpected;
	int minhops; // minimum known hops to remote node
	int minhop_trans_time;
	int minhop_link; // link via which minhops path observed
	NL_PACKET lastpacket;
	int last_ack_expected;
	unsigned short int has_resent;
	NL_PACKET lasttestpacket;
	unsigned int min_mtu;
	int test_has_come;
//unsigned short int resent_times; //for debug
} NLTABLE;

static NLTABLE *NL_table = NULL;
static int NL_table_size = 0;
static int NL_table_capacity = 0;
static int NL_table_increment = 10;

// -----------------------------------------------------------------

//  GIVEN AN ADDRESS, LOCATE OR CREATE ITS ENTRY IN THE NL_table
static int find_address(CnetAddr address) {
	//  ATTEMPT TO LOCATE A KNOWN ADDRESS
	for (int t = 0; t < NL_table_size; ++t)
		if (NL_table[t].address == address)
			return t;

	//  UNNOWN ADDRESS, SO WE MUST CREATE AND INITIALIZE A NEW ENTRY
	if (NL_table_size == NL_table_capacity) {
		NL_table = realloc(NL_table, (NL_table_size + NL_table_increment)
				* sizeof(NLTABLE));
		memset(&NL_table[NL_table_size], 0, NL_table_increment
				* sizeof(NLTABLE));
		NL_table_capacity += NL_table_increment;
	}
	NL_table[NL_table_size].address = address;
	//NL_table[NL_table_size].minhops = INT_MAX;
	NL_table[NL_table_size].minhop_trans_time = INT_MAX;
	NL_table[NL_table_size].has_resent = 0;
	NL_table[NL_table_size].test_has_come = 0;
	//NL_table[NL_table_size].resent_times = 0;
	return NL_table_size++;
}

int NL_updateminmtu(CnetAddr address, int min_mtu, int link) {
	int id = find_address(address);
	if (NL_table[id].minhop_link == link) {
		if (NL_table[id].min_mtu < min_mtu) {
			NL_table[id].min_mtu = min_mtu;
			return 1;
		}
	}
	return 0;
}

void NL_updatelastsendtest(NL_PACKET *last) {
	int index = find_address(last->dest);

	memcpy(&(NL_table[index].lasttestpacket), last, PACKET_HEADER_SIZE);
}

int NL_gettesthascome(CnetAddr address){
	return NL_table[find_address(address)].test_has_come;
}

void NL_inctesthascome(CnetAddr address){
	(NL_table[find_address(address)].test_has_come) ++;
}

int NL_minmtu(CnetAddr address){
	int id = find_address(address);
	return NL_table[id].min_mtu;
}

int NL_gethopcount(CnetAddr address){
	int id = find_address(address);
	return NL_table[id].minhops;
}

int	NL_gettablesize(){
	return NL_table_size;
}

int NL_getminmtubyid(int id){
	return NL_table[id].min_mtu;
}

NL_PACKET * NL_getlastsendtest(CnetAddr address){
	return &(NL_table[find_address(address)].lasttestpacket);
}
int NL_getdestbyid(int id){
	return NL_table[id].address;
}

/*--------
 *
 */

int NL_ackexpected(CnetAddr address) {
	int t = find_address(address);
	return NL_table[t].ackexpected;
}

void inc_NL_ackexpected(CnetAddr address) {
	int t = find_address(address);
	NL_table[t].ackexpected++;
}

int NL_nextpackettosend(CnetAddr address) {
	int t = find_address(address);
	return NL_table[t].nextpackettosend++;
}

int NL_packetexpected(CnetAddr address) {
	int t = find_address(address);
	return NL_table[t].packetexpected;
}

void inc_NL_packetexpected(CnetAddr address) {
	int t = find_address(address);
	NL_table[t].packetexpected++;
}

// -----------------------------------------------------------------

//  FIND THE LINK ON WHICH PACKETS OF MINIMUM HOP COUNT WERE OBSERVED.
//  IF THE BEST LINK IS UNKNOWN, WE RETURN ALL_LINKS.

int NL_linksofminhops(CnetAddr address) {
	int t = find_address(address);
	int link = NL_table[t].minhop_link;
	return (link == 0) ? ALL_LINKS : (1 << link);
}

static bool given_stats = false;

void NL_savehopcount(CnetAddr address, int trans_time, int link) {
	int t = find_address(address);
	if (NL_table[t].minhop_trans_time > trans_time) {
		NL_table[t].minhop_trans_time = trans_time;
		NL_table[t].minhop_link = link;
		given_stats = true;
	}
	/*
	 if (NL_table[t].minhops > hops) {
	 NL_table[t].minhops = hops;
	 NL_table[t].minhop_link = link;
	 given_stats = true;
	 }*/
}

int NL_get_has_resent(CnetAddr address) {
	int t = find_address(address);
	return NL_table[t].has_resent;
}

void NL_set_has_resent(CnetAddr address, unsigned short int value) {
	int t = find_address(address);
	NL_table[t].has_resent = value;
}

/* for debug */
/*
 void NL_inc_resent_times(CnetAddr address){
 int t = find_address(address);
 NL_table[t].resent_times++;
 }

 unsigned short int NL_get_resent_times(CnetAddr address){
 int t = find_address(address);
 return NL_table[t].resent_times;
 }
 */
/* end debug */
// -----------------------------------------------------------------

static EVENT_HANDLER( show_NL_table) {
	CNET_clear();
	printf("\n%12s %12s %12s %12s", "destination", "ackexpected",
			"nextpkttosend", "pktexpected");
	if (given_stats)
		//printf(" %8s %8s", "minhops", "bestlink");
		printf(" %8s %8s", "min_mtu", "bestlink");
	printf("\n");
	int no_init_count = 0;
	for (int t = 0; t < NL_table_size; ++t)
		if (NL_table[t].address != nodeinfo.address) {
			printf("%12d %12d %12d %12d %12d", (int) NL_table[t].address,
					NL_table[t].ackexpected, NL_table[t].nextpackettosend,
					NL_table[t].packetexpected, NL_table[t].min_mtu);

			if (NL_table[t].minhop_link != 0)
				printf(" %8d %8d", NL_table[t].minhops, NL_table[t].minhop_link);
			else
				++no_init_count;
			printf("\n");
		}
	printf("no_init_count = %d\n", no_init_count);

}

void reboot_NL_table(void) {
	CHECK(CNET_set_handler(EV_DEBUG0, show_NL_table, 0));
	CHECK(CNET_set_debug_string(EV_DEBUG0, "NL info"));

	NL_table = calloc(NL_table_increment, sizeof(NLTABLE));
	NL_table_capacity = NL_table_increment;
	NL_table_size = 0;
}
