#ifndef MILESTONE3_H
#define MILESTONE3_H

typedef enum {
	NL_DATA, NL_ACK, NL_TEST, NL_TEST_ACK
} NL_PACKETKIND;

typedef struct {
	CnetAddr src;
	CnetAddr dest;
	NL_PACKETKIND kind; /* only ever NL_DATA or NL_ACK */
	int seqno; /* 0, 1, 2, ... */
	int hopcount;
	size_t length; /* the length of the msg portion only */
	int min_mtu;
	char msg[MAX_MESSAGE_SIZE];
} NL_PACKET;

#define PACKET_HEADER_SIZE  (sizeof(NL_PACKET) - MAX_MESSAGE_SIZE)
#define PACKET_SIZE(p)	    (PACKET_HEADER_SIZE + p.length)

#endif
