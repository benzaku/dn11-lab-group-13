#include <cnetsupport.h>
#include "nl_packet.h"
//#include "vector.c"

typedef struct {
	unsigned int id;
	size_t length;
	char msg[MAX_MESSAGE_SIZE];
} RB_BUF_ELEM;

size_t RB_ELEM_SIZE = sizeof(RB_BUF_ELEM);
static VECTOR rb;//receive buffer

unsigned int RB_get_id(NL_PACKET *p) {
	return ((unsigned int) (p->src) * 10000000 + (unsigned int) (p->dest)
			* 10000 + (unsigned int) p->seqno);
}

unsigned int RB_get_id_link(NL_PACKET *p, int arrive_on_link){
	unsigned int hashPart = (unsigned int) (p->src * 13) + (unsigned int) (p->dest * 7) + (unsigned int) p->seqno;
	return (unsigned int) 100 * hashPart + (unsigned int) arrive_on_link;
}



void RB_init() {
	rb = vector_new();
}

void RB_save_msg_link(NL_PACKET *p, int arrive_on_link) {
	/*
	printf("RB_save_msg\n");
	printf("packet to be saved: src = %d, des = %d, seqno = %d\n, current = %d ", p->src,
			p->dest, p->seqno, nodeinfo.address);
	*/
	if((int) p->length > linkinfo[arrive_on_link].mtu - PACKET_HEADER_SIZE || (int) p->length < 0){
	  fprintf(stdout, "RB_save_msg_link, p->length = %d", (int) p->length);
	  return;
	}
	RB_BUF_ELEM* temp;
	RB_BUF_ELEM bufelem;
	unsigned int id = RB_get_id_link(p, arrive_on_link);
	//printf("calc_id = %d\n", id);
	int i;
	int n = vector_nitems(rb);
	if (n == 0) {
		//temp = malloc(RB_ELEM_SIZE);
		//temp->id = id;
		//temp->length = p->length;
		//memcpy(temp->msg, (char *) p->msg, p->length);
		bufelem.id = id;
		bufelem.length = p->length;
		memset(bufelem.msg, '0', MAX_MESSAGE_SIZE);
		memcpy(bufelem.msg, (char *) p->msg, p->length);
		vector_append(rb, &bufelem, RB_ELEM_SIZE);
	} else {
		for (i = 0; i < n; i++) {
			temp = vector_peek(rb, i, &RB_ELEM_SIZE);
			if (temp == NULL)
				printf("temp = NULL\n");
			if (temp->id == id) {
				//backup to new elem because of vector_replace
				/*
				memcpy(&temp->msg[temp->length], (char *) p->msg, p->length);
				temp->length += p->length;
				*/
				
				RB_BUF_ELEM elem;
				elem.id = id;
				elem.length = temp->length;
				memcpy(&elem.msg, &temp->msg, elem.length);
				memcpy(&elem.msg[elem.length], (char *) p->msg, p->length);

				elem.length += p->length;
				vector_replace(rb, i, &elem, RB_ELEM_SIZE);
				
				/*
				printf(
						"elem found: msg saved at vector[%d], msg_id= %d, msg_length = %d\n",
						i, temp->id, temp->length);
				printf("p->isend = %d", p->pieceEnd);
				*/
				break;
			}
		}
		if (i == n) {
			RB_BUF_ELEM tempelem;
			tempelem.id = id;
			tempelem.length = p->length;
			memset(bufelem.msg, '0', MAX_MESSAGE_SIZE);
			memcpy(tempelem.msg, (char *) p->msg, p->length);
			vector_append(rb, &tempelem, RB_ELEM_SIZE);
		}
	}
	//printf("\n");
}

void RB_copy_whole_msg_link(NL_PACKET *p, int arrive_on_link) {
	/*
	printf("RB_copy_whole_msg\n");
	printf("packet to be removed: src = %d, des = %d, seqno = %d\n, current = %d ", p->src,
			p->dest, p->seqno, nodeinfo.address);
	*/
	unsigned int id = RB_get_id_link(p,arrive_on_link);
	unsigned int hashPart = id / 100;
	//printf("calc_id = %d\n", id);
	int i;
	int n = vector_nitems(rb);
	for (i = 0; i < n; i++) {
		RB_BUF_ELEM *temp;
		temp = vector_peek(rb, i, &RB_ELEM_SIZE);
		if (temp->id == id) {
			memcpy(p->msg, temp->msg, temp->length);
			p->length = temp->length;
			size_t aaa;
			temp = vector_remove(rb, i, &aaa);
			free(temp);
			--i;
			--n;
			continue;
		}
		if(temp->id/100 == hashPart){
			size_t aa;
			temp = vector_remove(rb, i, &aa);
			free(temp);
			--i;
			--n;
			continue;
		}

	}

	//printf("\n");
}

/*
int isCorrupted(NL_PACKET * p) {
	int p_checksum = p->checksum;
	int checksum = CNET_ccitt((unsigned char *) p->msg,
			p->src_packet_length);
	if(p_checksum == checksum){
		return 1;
	}
	return 0;
}
*/

/*
void new_RB_save_msg(NL_PACKET *p) {
	RB_BUF_ELEM * buf_elem_p;
	RB_BUF_ELEM buf_elem;
	size_t size = sizeof(RB_BUF_ELEM);
	int n = vector_nitems(rb);
	int i = 0;
	unsigned int id = RB_get_id(p);

	if (n == 0) {
		//no element
		buf_elem.id = id;
		buf_elem.length = p->length;
		memcpy(&(buf_elem.msg), (char *) &(p->msg), p->length);
		vector_append(rb, &buf_elem, sizeof(buf_elem));
	} else {
		for (i = 0; i < n; i++) {
			buf_elem_p = vector_peek(rb, i, &size);
			if (buf_elem_p->id == id) {
				memcpy(&(buf_elem_p->msg[buf_elem_p->length]), &(p->msg),
						p->length);
				buf_elem_p->length += p->length;
				return;
			}
		}
		//there is no node in vector.
		buf_elem.id = id;
		buf_elem.length = p->length;
		memcpy(&(buf_elem.msg), (char *) &(p->msg), p->length);
		vector_append(rb, &buf_elem, sizeof(buf_elem));
	}
}
*/
