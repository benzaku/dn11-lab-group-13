#ifndef _NL_PACKET_H_
#define _NL_PACKET_H_
#include <cnet.h>

typedef enum {
	NL_DATA, NL_ACK, NL_ERR_ACK, NL_ERR_ACK_RESENT
} NL_PACKETKIND;

typedef struct {
	CnetAddr src;
	CnetAddr dest;
	NL_PACKETKIND kind; /* only ever NL_DATA or NL_ACK */
	unsigned short int seqno; /* 0, 1, 2, ... */
	unsigned short int hopcount;
	size_t pieceStartPosition;
	unsigned short int pieceEnd;
	size_t length; /* the length of the msg portion only */
	size_t src_packet_length; /* dont change during routing */
	uint32_t checksum;
	uint32_t piece_checksum;
	unsigned int trans_time;
	unsigned short int is_resent;
	unsigned int mtu;

	char msg[MAX_MESSAGE_SIZE];
} NL_PACKET;

#endif
