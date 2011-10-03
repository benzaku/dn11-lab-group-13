#include <cnetsupport.h>
#include <limits.h>
#include "milestone3.h"

typedef struct {
	unsigned int id;
	size_t length;
	int flag;
	char msg[MAX_MESSAGE_SIZE];
} RB_BUF_ELEM;

typedef struct {
	int *pos;
	int size;

} START_POS;

size_t RB_ELEM_SIZE = sizeof(RB_BUF_ELEM);

/*
 unsigned int RB_get_id_link(NL_PACKET *p, int arrive_on_link){
 unsigned int hashPart = (unsigned int) (p->src * 13) + (unsigned int) (p->dest * 7) + (unsigned int) p->seqno;
 return (unsigned int) 100 * hashPart + (unsigned int) arrive_on_link;
 }
 */

unsigned int RB_get_id_link(NL_PACKET *p, int arrive_on_link) {
	return (unsigned int) p->src * 1000 + (unsigned int) p->dest;
}

void RB_init(VECTOR rb) {
	rb = vector_new();
}

//if success return 1, if full return 2
int RB_save_msg_link(VECTOR rb, NL_PACKET *p, int arrive_on_link) {

	RB_BUF_ELEM* temp;
	RB_BUF_ELEM bufelem;
	unsigned int id = RB_get_id_link(p, arrive_on_link);

	int i;
	int n = vector_nitems(rb);

	//fill pieces into receive buffer
	for (i = 0; i < n; i++) {
		temp = vector_peek(rb, i, &RB_ELEM_SIZE);
		if (temp->id == id) {
			//backup to new elem because of vector_replace

			if (temp->flag != p->pieceStartPosition){
				printf("flag = %d, startpos = %d\n", temp->flag, p->pieceStartPosition);
				return -1;

			}
			temp->flag = p->pieceStartPosition;
			memcpy(&temp->msg[p->pieceStartPosition], (char *) p->msg,
					p->length);
			temp->length += p->length;

			temp->flag +=  p->length;
			if (temp->length == p->src_packet_length)
				return 2; //whole msg filled
			else
				return 1;
		}
	}

	//id not found, created new entry in vector
	if (i == n) {

		bufelem.id = id;
		bufelem.length = p->length;
		bufelem.flag = p->length;

		memset(bufelem.msg, CHAR_MAX, MAX_MESSAGE_SIZE);
		memcpy(bufelem.msg, (char *) p->msg, p->length);

		vector_append(rb, &bufelem, RB_ELEM_SIZE);
		if (bufelem.length == p->src_packet_length)
			return 2;
		else
			return 1;
	}
	return 0;
}

void RB_copy_whole_msg_link(VECTOR rb, NL_PACKET *p, int arrive_on_link) {

	unsigned int id = RB_get_id_link(p, arrive_on_link);

	int i;
	int n = vector_nitems(rb);
	RB_BUF_ELEM *temp;

	for (i = 0; i < n; i++) {
		temp = vector_peek(rb, i, &RB_ELEM_SIZE);
		if (temp->id == id) {
			memcpy(p->msg, temp->msg, temp->length);
			size_t aaa;
			temp = vector_remove(rb, i, &aaa);
			free(temp);
			break;
		}
	}
}

// 1 indicates frame missed, 0 denotes no frame missed
unsigned short int is_frame_missed(VECTOR rb, NL_PACKET *p, int arrive_on_link) {
	unsigned int id = RB_get_id_link(p, arrive_on_link);
	int n = vector_nitems(rb);
	RB_BUF_ELEM *p_bufelem;
	int i;
	int pos;
	for (i = 0; i < n; i++) {
		p_bufelem = vector_peek(rb, i, &RB_ELEM_SIZE);
		if (p_bufelem->id == id) {
			for (pos = 0; pos < (int) p->pieceStartPosition; pos += p->min_mtu) {
				if (p_bufelem->msg[pos] == CHAR_MAX) {
					return 1;
				}
			}
			return 0;
		}
	}
	return 0;
}

//  find all missing frame position
void RB_find_missing_pieces(VECTOR rb, NL_PACKET *p, int arrive_on_link,
		START_POS *start_pos) {

	unsigned int id = RB_get_id_link(p, arrive_on_link);
	int n = vector_nitems(rb);
	RB_BUF_ELEM *p_bufelem;
	int i, pos;
	start_pos->size = 0;

	for (i = 0; i < n; i++) {
		p_bufelem = vector_peek(rb, i, &RB_ELEM_SIZE);
		if (p_bufelem->id == id) {
			for (pos = 0; pos < (int) p->pieceStartPosition; pos += p->min_mtu) {
				if (p_bufelem->msg[pos] == CHAR_MAX) {
					*(start_pos->pos + start_pos->size) = pos;
					++start_pos->size;
				}
			}
			break;
		}
	}
}

int RB_find_missing_start_pos(VECTOR rb, NL_PACKET *p, int arrive_on_link) {
	unsigned int id = RB_get_id_link(p, arrive_on_link);
	int n = vector_nitems(rb);
	RB_BUF_ELEM *p_bufelem;
	int min_mtu = (int) p->min_mtu;
	int start_pos;// start position of a piece
	int first_missing_pos = -1;
	int i;

	for (i = 0; i < n; i++) {
		p_bufelem = vector_peek(rb, i, &RB_ELEM_SIZE);
		if (p_bufelem->id == id) {
			for (start_pos = (int) p->pieceStartPosition - min_mtu; start_pos
					>= 0; start_pos -= min_mtu) {
				if (p_bufelem->msg[start_pos] == CHAR_MAX)
					first_missing_pos = start_pos;
				else
					return first_missing_pos;
			}
		}
	}
	return -1;
}

/*
 void RB_delete_missing_piece(START_POS *start_pos, int pos) {

 }
 */

/*
 unsigned int RB_find_missing_interval(VECTOR rb, NL_PACKET *p,
 int arrive_on_link, START_POS *start_pos) {
 unsigned int id = RB_get_id_link(p, arrive_on_link);
 int n = vector_nitems(rb);
 RB_BUF_ELEM *p_bufelem;
 int min_mtu = (int) p->min_mtu;
 int start_pos;// start position of a piece
 int i;
 int last_missing_pos = -1;
 int first_missing_pos = -1;

 for (i = 0; i < n; i++) {
 p_bufelem = vector_peek(rb, i, &RB_ELEM_SIZE);
 if (p_bufelem->id == id) {
 for (start_pos = 0; start_pos < (int) p->pieceStartPosition; start_pos
 += min_mtu) {
 if (p_bufelem->msg[start_pos] == CHAR_MAX && first_missing_pos
 == -1)
 first_missing_pos = start_pos;
 if (p_bufelem->msg[start_pos] != CHAR_MAX && first_missing_pos
 != -1)
 last_missing_pos = start_pos - 1;
 if (first_missing_pos != -1 && last_missing_pos != -1)
 return (last_missing_pos - first_missing_pos + 1);
 }
 return 0;
 }
 }
 }
 */

