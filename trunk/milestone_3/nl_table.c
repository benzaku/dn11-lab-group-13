#include <cnet.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "nl_table.h"
#include "nl_packet.h"

// ---- A SIMPLE NETWORK LAYER SEQUENCE TABLE AS AN ABSTRACT DATA TYPE ----

typedef struct {
	CnetAddr address; // ... of remote node
	int ackexpected; // packet sequence numbers to/from node
	int nextpackettosend;
	int packetexpected;
	int minhops; // minimum known hops to remote node
	int minhop_link; // link via which minhops path observed
	NL_PACKET lastpacket;
} NLTABLE;

static NLTABLE *NL_table = NULL;
static int NL_table_size = 0;
static int NL_table_capacity = 0;
static int NL_table_increment = 5;




// -----------------------------------------------------------------

//  GIVEN AN ADDRESS, LOCATE OR CREATE ITS ENTRY IN THE NL_table
static int find_address(CnetAddr address) {
	//  ATTEMPT TO LOCATE A KNOWN ADDRESS
	for (int t = 0; t < NL_table_size; ++t)
		if (NL_table[t].address == address)
			return t;

	//  UNNOWN ADDRESS, SO WE MUST CREATE AND INITIALIZE A NEW ENTRY
	if(NL_table_size == NL_table_capacity){
	  NL_table = realloc(NL_table, (NL_table_size + NL_table_increment) * sizeof(NLTABLE));
	  memset(&NL_table[NL_table_size], 0, NL_table_increment*sizeof(NLTABLE));
	  NL_table_capacity += NL_table_increment;
	}
	NL_table[NL_table_size].address = address;
	NL_table[NL_table_size].minhops = INT_MAX;
	return NL_table_size++;
}

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

void NL_savehopcount(CnetAddr address, int hops, int link) {
	int t = find_address(address);

	if (NL_table[t].minhops > hops) {
		NL_table[t].minhops = hops;
		NL_table[t].minhop_link = link;
		given_stats = true;
	}
}

// -----------------------------------------------------------------

static EVENT_HANDLER( show_NL_table) {
	CNET_clear();
	printf("\n%12s %12s %12s %12s", "destination", "ackexpected",
			"nextpkttosend", "pktexpected");
	if (given_stats)
		printf(" %8s %8s", "minhops", "bestlink");
	printf("\n");

	for (int t = 0; t < NL_table_size; ++t)
		if (NL_table[t].address != nodeinfo.address) {
			printf("%12d %12d %12d %12d", (int) NL_table[t].address,
					NL_table[t].ackexpected, NL_table[t].nextpackettosend,
					NL_table[t].packetexpected);
			if (NL_table[t].minhop_link != 0)
				printf(" %8d %8d", NL_table[t].minhops, NL_table[t].minhop_link);
			printf("\n");
			//if(NL_table[t].lastpacket){
				//printf("last packet sent = %d\n", NL_table[t].lastpacket->seqno);
			//}
		}
}

void reboot_NL_table(void) {
	CHECK(CNET_set_handler(EV_DEBUG0, show_NL_table, 0));
	CHECK(CNET_set_debug_string(EV_DEBUG0, "NL info"));

	NL_table = calloc(NL_table_increment, sizeof(NLTABLE));
	NL_table_capacity = NL_table_increment;
	NL_table_size = 0;
}