#ifndef _NL_TABLE_H_
#define _NL_TABLE_H_

#include <cnet.h>

#define	ALL_LINKS	(-1)

// ---- A SIMPLE NETWORK LAYER SEQUENCE TABLE AS AN ABSTRACT DATA TYPE ----
typedef struct {
	CnetAddr src; // ... of remote node
	CnetAddr dest;
	unsigned short int ackexpected; // packet sequence numbers to/from node
	unsigned short int nextpackettosend;
	unsigned short int packetexpected;
	//int minhops; // minimum known hops to remote node
	int minhop_trans_time;
	int minhop_link; // link via which minhops path observed
	NL_PACKET lastpacket;
	unsigned short int has_resent;
//unsigned short int resent_times; //for debug
} NLTABLE;

static NLTABLE *NL_table = NULL;
static int NL_table_size = 0;
static int NL_table_capacity = 0;
static int NL_table_increment = 10;

extern void reboot_NL_table(void);

extern int NL_ackexpected(CnetAddr src, CnetAddr dest);
extern int NL_nextpackettosend(CnetAddr src, CnetAddr dest);
extern int NL_packetexpected(CnetAddr src, CnetAddr dest);

extern void inc_NL_packetexpected(CnetAddr src, CnetAddr dest);
extern void inc_NL_ackexpected(CnetAddr src, CnetAddr dest);

extern int NL_linksofminhops(CnetAddr src, CnetAddr dest);
extern void NL_savehopcount(CnetAddr src, CnetAddr dest, int trans_time, int link);

extern int NL_get_has_resent(CnetAddr src, CnetAddr dest);
extern void NL_set_has_resent(CnetAddr src, CnetAddr dest, unsigned short int value);
#endif
