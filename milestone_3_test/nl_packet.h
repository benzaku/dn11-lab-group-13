#ifndef _NL_PACKET_H_
#define _NL_PACKET_H_
#include <cnet.h>

typedef enum {
	NL_DATA, NL_ACK, NL_ERR_ACK
} NL_PACKETKIND;

typedef struct {
	CnetAddr src;
	CnetAddr dest;
	NL_PACKETKIND kind; /* only ever NL_DATA or NL_ACK */
	int seqno; /* 0, 1, 2, ... */
	//int hopcount;
	int checksum;
	int traveled_hops[MAXHOPS];
	int traveled_hops_count;
	int trans_time;
	size_t pieceNumber;
	int pieceEnd;
	size_t length; /* the length of the msg portion only */
	char msg[MAX_MESSAGE_SIZE];
} NL_PACKET;

#endif
