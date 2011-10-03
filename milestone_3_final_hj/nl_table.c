#include <cnet.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "nl_table.h"

// ---- A SIMPLE NETWORK LAYER SEQUENCE TABLE AS AN ABSTRACT DATA TYPE ----

typedef struct {
	CnetAddr address; // ... of remote node
	int ackexpected; // packet sequence numbers to/from node
	int nextpackettosend;
	int packetexpected;
	int minhops; // minimum known hops to remote node
	unsigned int minhop_trans_time;
	int minhop_link; // link via which minhops path observed
	NL_PACKET lastpacket;
	int last_ack_expected;
	unsigned short int has_resent;
	NL_PACKET lasttestpacket;
	unsigned int min_mtu;
	int test_has_come;
} NLTABLE;

static NLTABLE *NL_table = NULL;
static int NL_table_size = 0;

// -----------------------------------------------------------------

//  GIVEN AN ADDRESS, LOCATE OR CREATE ITS ENTRY IN THE NL_table
static int find_address(CnetAddr address) {
	//  ATTEMPT TO LOCATE A KNOWN ADDRESS
	for (int t = 0; t < NL_table_size; ++t)
		if (NL_table[t].address == address)
			return t;

	//  UNNOWN ADDRESS, SO WE MUST CREATE AND INITIALIZE A NEW ENTRY
	NL_table = realloc(NL_table, (NL_table_size + 1) * sizeof(NLTABLE));
	memset(&NL_table[NL_table_size], 0, sizeof(NLTABLE));
	NL_table[NL_table_size].address = address;
	NL_table[NL_table_size].minhops = INT_MAX;
	NL_table[NL_table_size].minhop_trans_time = UINT_MAX;
	NL_table[NL_table_size].min_mtu = 0;
	NL_table[NL_table_size].test_has_come = 0;
	//NL_table[NL_table_size].minhop_link = 0;
	return NL_table_size++;
}

void NL_set_has_resent(CnetAddr address, unsigned short int value) {
	int t = find_address(address);
	NL_table[t].has_resent = value;
}

NL_PACKET * NL_getlastsendtest(CnetAddr address) {
	return &(NL_table[find_address(address)].lasttestpacket);
}

NL_PACKET * NL_getlastsendtestbyid(int id) {
	return &(NL_table[id].lasttestpacket);
}

void NL_inctesthascome(CnetAddr address) {
	(NL_table[find_address(address)].test_has_come)++;
}

int NL_gettesthascome(CnetAddr address) {
	return NL_table[find_address(address)].test_has_come;
}

void NL_updatelastsendtest(NL_PACKET *last) {
	if (last->dest == 0)
		fprintf(stdout, "HERE WE HAVE A ZERO!! src = %d, dest = %d\n",
				last->src, last->dest);
	int index = find_address(last->dest);
	//NL_PACKET * lastsend = &(NL_table[index].lastpacket);

	memcpy(&(NL_table[index].lasttestpacket), last, PACKET_HEADER_SIZE);
	//NL_table[index].last_ack_expected = NL_ackexpected(last->dest);
	//free(temp);
}

int NL_getdestbyid(int id) {
	return NL_table[id].address;
}

int NL_gettablesize() {
	return NL_table_size;
}

int NL_getminmtubyid(int id) {
	return NL_table[id].min_mtu;
}

void NL_setminmtu(CnetAddr address, int value) {
	int id = find_address(address);
	NL_table[id].min_mtu = value;
}

int NL_gethopcount(CnetAddr address) {
	int id = find_address(address);
	return NL_table[id].minhops;
}

int NL_minmtu(CnetAddr address) {
	int id = find_address(address);
	return NL_table[id].min_mtu;
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

void NL_savetranstime(CnetAddr address, int trans_time, int link) {
	int t = find_address(address);
	if (NL_table[t].minhop_trans_time > trans_time) {
		NL_table[t].minhop_trans_time = trans_time;
		NL_table[t].minhop_link = link;
		given_stats = true;
	}
}

int NL_getnoinitcount(){
	int ret = 0;
	for(int t = 0; t < NL_table_size; ++t){
		if(NL_table[t].min_mtu == 0)
			ret++;
	}
	return ret;
}

// -----------------------------------------------------------------

static EVENT_HANDLER( show_NL_table) {
	CNET_clear();
	printf("\n%12s %12s %12s %12s %12s", "destination", "ackexpected",
			"nextpkttosend", "pktexpected", "minmtu");
	if (given_stats)
		printf(" %8s %8s", "minhops", "bestlink");
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

	NL_table = calloc(1, sizeof(NLTABLE));
	NL_table_size = 0;
}
