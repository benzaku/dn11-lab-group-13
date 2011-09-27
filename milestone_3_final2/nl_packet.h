#ifndef _NL_PACKET_H_
#define _NL_PACKET_H_
//#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_MESSAGE_SIZE 1000

typedef enum {
	NL_DATA, NL_ACK, NL_ERR_ACK, NL_ERR_ACK_RESENT
} NL_PACKETKIND;

typedef struct {
	//CnetAddr src;
	//CnetAddr dest;
	int src;
	int dest;
	NL_PACKETKIND kind; /* only ever NL_DATA or NL_ACK */
	unsigned short int seqno; /* 0, 1, 2, ... */
	unsigned short int hopcount;
	size_t pieceNumber;
	unsigned short int pieceEnd;
	size_t length; /* the length of the msg portion only */
	int src_packet_length; /* dont change during routing */
	int checksum;
	unsigned int trans_time;
	unsigned short int is_resent;

	char msg[MAX_MESSAGE_SIZE];
} NL_PACKET;

#endif
