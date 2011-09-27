#ifndef _MILESTONE3_H_
#define _MILESTONE3_H_

#include "nl_packet.h"

#define PACKET_HEADER_SIZE  (sizeof(NL_PACKET) - MAX_MESSAGE_SIZE)
#define PACKET_SIZE(p)      (PACKET_HEADER_SIZE + p.length)
#define MAXHOPS             100

static char receiveBuffer[256][MAX_MESSAGE_SIZE];
static char *rb[256];
//static size_t packet_length[256];
NL_PACKET * lastPacket;
static int mtu;

extern void send_ack(NL_PACKET *p, int arrived_on_link,
		unsigned short int is_err_ack);
extern void print_msg(char *msg, size_t length);
extern void update_last_packet(NL_PACKET *last);
extern NL_PACKET * get_last_packet(CnetAddr address);
extern void sdown_pieces_to_datalink(char *packet, size_t length,
		int choose_link);
extern int up_to_network(char *packet, size_t length, int arrived_on_link);
extern void up_to_application(NL_PACKET *p, int arrived_on_link);
extern void route_packet(NL_PACKET *p, int arrived_on_link);

#endif
