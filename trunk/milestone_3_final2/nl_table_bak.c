#include <cnet.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "nl_table.h"
#include "nl_packet.h"

//  GIVEN AN ADDRESS, LOCATE OR CREATE ITS ENTRY IN THE NL_table
static int find_index(CnetAddr src, CnetAddr dest) {
	//  ATTEMPT TO LOCATE A KNOWN ADDRESS
	for (int t = 0; t < NL_table_size; ++t)
		if (NL_table[t].src == src && NL_table[NL_table_size].dest == dest)
			return t;

	//  UNNOWN ADDRESS, SO WE MUST CREATE AND INITIALIZE A NEW ENTRY
	if (NL_table_size == NL_table_capacity) {
		NL_table = realloc(NL_table, (NL_table_size + NL_table_increment)
				* sizeof(NLTABLE));
		memset(&NL_table[NL_table_size], 0, NL_table_increment
				* sizeof(NLTABLE));
		NL_table_capacity += NL_table_increment;
	}
	NL_table[NL_table_size].src = src;
	NL_table[NL_table_size].dest = dest;
	//NL_table[NL_table_size].minhops = INT_MAX;
	NL_table[NL_table_size].minhop_trans_time = INT_MAX;
	NL_table[NL_table_size].has_resent = 0;
	//NL_table[NL_table_size].resent_times = 0;
	return NL_table_size++;
}

unsigned short int NL_ackexpected(CnetAddr src, CnetAddr dest) {
	int t = find_index(src, dest);
	return NL_table[t].ackexpected;
}

void inc_NL_ackexpected(CnetAddr src, CnetAddr dest) {
	int t = find_index(src, dest);
	NL_table[t].ackexpected++;
}

unsigned short int NL_nextpackettosend(CnetAddr src, CnetAddr dest) {
	int t = find_index(src, dest);
	int nextpackettosend = NL_table[t].nextpackettosend;
	if(nextpackettosend == 9999)
	  NL_table[t].nextpackettosend = 0;
	else 
	  NL_table[t].nextpackettosend++;
	return nextpackettosend;
}

unsigned short int NL_packetexpected(CnetAddr src, CnetAddr dest) {
	int t = find_index(src, dest);
	return NL_table[t].packetexpected;
}

void inc_NL_packetexpected(CnetAddr src, CnetAddr dest) {
	int t = find_index(src, dest);
	if (NL_table[t].packetexpected == 9999)
	  NL_table[t].packetexpected = 0;
	else 
	  NL_table[t].packetexpected++;
}

// -----------------------------------------------------------------

//  FIND THE LINK ON WHICH PACKETS OF MINIMUM HOP COUNT WERE OBSERVED.
//  IF THE BEST LINK IS UNKNOWN, WE RETURN ALL_LINKS.

int NL_linksofminhops(CnetAddr src, CnetAddr dest) {
	int t = find_index(src, dest);
	int link = NL_table[t].minhop_link;
	return (link == 0) ? ALL_LINKS : (1 << link);
}

static bool given_stats = false;

void NL_savehopcount(CnetAddr src, CnetAddr dest, int trans_time, int link) {
	int t = find_index(src, dest);
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

int NL_get_has_resent(CnetAddr src, CnetAddr dest) {
	int t = find_index(src, dest);
	return NL_table[t].has_resent;
}

void NL_set_has_resent(CnetAddr src, CnetAddr dest, unsigned short int value) {
	int t = find_index(src, dest);
	NL_table[t].has_resent = value;
}

/* for debug */
/*
 void NL_inc_resent_times(CnetAddr src){
 int t = find_src(src);
 NL_table[t].resent_times++;
 }

 unsigned short int NL_get_resent_times(CnetAddr src){
 int t = find_src(src);
 return NL_table[t].resent_times;
 }
 */
/* end debug */
// -----------------------------------------------------------------

/*
static EVENT_HANDLER( show_NL_table) {
	CNET_clear();
	printf("\n%12s %12s %12s %12s", "destination", "ackexpected",
			"nextpkttosend", "pktexpected");
	if (given_stats)
		//printf(" %8s %8s", "minhops", "bestlink");
		printf(" %8s %8s", "minhop_trans_time", "bestlink");
	printf("\n");

	for (int t = 0; t < NL_table_size; ++t)
		if (NL_table[t].src != nodeinfo.src) {
			printf("%12d %12d %12d %12d", (int) NL_table[t].src,
					NL_table[t].ackexpected, NL_table[t].nextpackettosend,
					NL_table[t].packetexpected);
			if (NL_table[t].minhop_link != 0)
				//printf(" %8d %8d", NL_table[t].minhops, NL_table[t].minhop_link);
				printf(" %8d %8d", NL_table[t].minhop_trans_time,
						NL_table[t].minhop_link);
			printf("\n");
			//if(NL_table[t].lastpacket){
			//printf("last packet sent = %d\n", NL_table[t].lastpacket->seqno);
			//}
		}
}
*/

void reboot_NL_table(void) {
	CHECK(CNET_set_handler(EV_DEBUG0, show_NL_table, 0));
	CHECK(CNET_set_debug_string(EV_DEBUG0, "NL info"));

	NL_table = calloc(NL_table_increment, sizeof(NLTABLE));
	NL_table_capacity = NL_table_increment;
	NL_table_size = 0;
}
