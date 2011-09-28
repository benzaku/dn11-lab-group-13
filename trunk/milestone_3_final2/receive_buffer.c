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

void new_RB_save_msg(NL_PACKET *p) {
	printf(";asldkfja;dkfj;asdlkfj;askdjf\n");
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

void RB_save_msg(NL_PACKET *p) {
	printf("RB_save_msg\n");
	printf("packet to be saved: src = %d, des = %d, seqno = %d\n", p->src,
			p->dest, p->seqno);
	RB_BUF_ELEM* temp;
	RB_BUF_ELEM bufelem;
	unsigned int id = RB_get_id(p);
	printf("calc_id = %d\n", id);
	int i;
	int n = vector_nitems(rb);
	if (n == 0) {
		//temp = malloc(RB_ELEM_SIZE);
		//temp->id = id;
		//temp->length = p->length;
		//memcpy(temp->msg, (char *) p->msg, p->length);
		bufelem.id = id;
		bufelem.length = p->length;
		memcpy((char *) (&bufelem.msg[0]), (char *) p->msg, p->length);
		vector_append(rb, &bufelem, RB_ELEM_SIZE);
		printf("ok1\n");
		//temp = vector_peek(rb, 0, &RB_ELEM_SIZE); // debug
		//printf("vector empty: msg saved at vector[%d], msg_id= %d, msg_length = %d\n", 0, temp->id, temp->length);
	} else {
		for (i = 0; i < n; i++) {
			temp = vector_peek(rb, i, &RB_ELEM_SIZE);
			if (temp == NULL)
				printf("temp = NULL\n");
			if (temp->id == id) {
				//backup to new elem because of vector_replace
				printf("ok1\n");
				RB_BUF_ELEM elem;
				elem.id = id;
				elem.length = temp->length;
				memcpy(&elem.msg, &temp->msg, elem.length);
				printf("1. p->length = %d\n", p->length);
				memcpy(&elem.msg[elem.length], (char *) p->msg, p->length);

				elem.length += p->length;
				printf("1.25. p->pieceno = %d\n", p->pieceNumber);
				printf("1.5. p->src_packet_length = %d\n", p->src_packet_length);
				printf("2. p->length = %d\n", p->length);
				vector_replace(rb, i, &elem, RB_ELEM_SIZE);
				temp = vector_peek(rb, i, &RB_ELEM_SIZE); // debug
				printf(
						"elem found: msg saved at vector[%d], msg_id= %d, msg_length = %d\n",
						i, temp->id, temp->length);
				printf("p->isend = %d", p->pieceEnd);
				break;
			}
		}
		if (i == n) {
			//printf("n = %d\n", n);
			RB_BUF_ELEM tempelem;
			tempelem.id = RB_get_id(p);
			tempelem.length = p->length;
			//temp = malloc(RB_ELEM_SIZE);
			//temp->id = RB_get_id(p);
			//temp->length = p->length;
			//memcpy(temp, (char *) p->msg, p->length);
			memcpy(&tempelem.msg, (char *) p->msg, p->length);
			vector_append(rb, &tempelem, RB_ELEM_SIZE);
			//temp = vector_peek(rb, i, &RB_ELEM_SIZE); // debug
			//printf("create new elem: msg saved at vector[%d], msg_id= %d, msg_length = %d\n", i, temp->id, temp->length);
		}
	}
	printf("\n");
}

void RB_save_msg_link(NL_PACKET *p, int arrive_on_link) {
	printf("RB_save_msg\n");
	printf("packet to be saved: src = %d, des = %d, seqno = %d\n", p->src,
			p->dest, p->seqno);
	RB_BUF_ELEM* temp;
	RB_BUF_ELEM bufelem;
	unsigned int id = RB_get_id_link(p, arrive_on_link);
	printf("calc_id = %d\n", id);
	int i;
	int n = vector_nitems(rb);
	if (n == 0) {
		//temp = malloc(RB_ELEM_SIZE);
		//temp->id = id;
		//temp->length = p->length;
		//memcpy(temp->msg, (char *) p->msg, p->length);
		bufelem.id = id;
		bufelem.length = p->length;
		memcpy((char *) (&bufelem.msg[0]), (char *) p->msg, p->length);
		vector_append(rb, &bufelem, RB_ELEM_SIZE);
		printf("ok1\n");
		//temp = vector_peek(rb, 0, &RB_ELEM_SIZE); // debug
		//printf("vector empty: msg saved at vector[%d], msg_id= %d, msg_length = %d\n", 0, temp->id, temp->length);
	} else {
		for (i = 0; i < n; i++) {
			temp = vector_peek(rb, i, &RB_ELEM_SIZE);
			if (temp == NULL)
				printf("temp = NULL\n");
			if (temp->id == id) {
				//backup to new elem because of vector_replace
				printf("ok1\n");
				RB_BUF_ELEM elem;
				elem.id = id;
				elem.length = temp->length;
				memcpy(&elem.msg, &temp->msg, elem.length);
				printf("1. p->length = %d\n", p->length);
				memcpy(&elem.msg[elem.length], (char *) p->msg, p->length);

				elem.length += p->length;
				printf("1.25. p->pieceno = %d\n", p->pieceNumber);
				printf("1.5. p->src_packet_length = %d\n", p->src_packet_length);
				printf("2. p->length = %d\n", p->length);
				vector_replace(rb, i, &elem, RB_ELEM_SIZE);
				temp = vector_peek(rb, i, &RB_ELEM_SIZE); // debug
				printf(
						"elem found: msg saved at vector[%d], msg_id= %d, msg_length = %d\n",
						i, temp->id, temp->length);
				printf("p->isend = %d", p->pieceEnd);
				break;
			}
		}
		if (i == n) {
			//printf("n = %d\n", n);
			RB_BUF_ELEM tempelem;
			tempelem.id = RB_get_id_link(p, arrive_on_link);
			tempelem.length = p->length;
			//temp = malloc(RB_ELEM_SIZE);
			//temp->id = RB_get_id(p);
			//temp->length = p->length;
			//memcpy(temp, (char *) p->msg, p->length);
			memcpy(&tempelem.msg, (char *) p->msg, p->length);
			vector_append(rb, &tempelem, RB_ELEM_SIZE);
			//temp = vector_peek(rb, i, &RB_ELEM_SIZE); // debug
			//printf("create new elem: msg saved at vector[%d], msg_id= %d, msg_length = %d\n", i, temp->id, temp->length);
		}
	}
	printf("\n");
}

int isCorrupted(NL_PACKET * p) {
	int p_checksum = p->checksum;
	int checksum = CNET_ccitt((unsigned char *) p->msg,
			p->src_packet_length);
	if(p_checksum == checksum){
		return 1;
	}
	return 0;
}

void RB_copy_whole_msg_link(NL_PACKET *p, int arrive_on_link) {
	printf("RB_copy_whole_msg\n");
	printf("packet to be removed: src = %d, des = %d, seqno = %d\n", p->src,
			p->dest, p->seqno);

	unsigned int id = RB_get_id_link(p,arrive_on_link);
	unsigned int hashPart = id / 100;
	printf("calc_id = %d\n", id);
	int i;
	int n = vector_nitems(rb);
	for (i = 0; i < n; i++) {
		RB_BUF_ELEM *temp;
		temp = vector_peek(rb, i, &RB_ELEM_SIZE);
		if (temp->id == id) {
			memcpy(p->msg, temp->msg, temp->length);
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
	//n = vector_nitems(rb);

	printf("\n");
}

/*
void RB_copy_whole_msg(NL_PACKET *p) {
	printf("RB_copy_whole_msg\n");
	printf("packet to be removed: src = %d, des = %d, seqno = %d\n", p->src,
			p->dest, p->seqno);

	unsigned int id = RB_get_id(p);
	printf("calc_id = %d\n", id);
	int i;
	int n = vector_nitems(rb);
	for (i = 0; i < n; i++) {
		printf("hi1\n");
		RB_BUF_ELEM *temp;
		printf("hi2\n");
		temp = vector_peek(rb, i, &RB_ELEM_SIZE);
		printf("hi3\n");
		if (temp->id == id) {
			printf("hi4\n");
			memcpy(p->msg, temp->msg, temp->length);
			//p->length = temp->length;
			printf("hi5\n");
			printf("p->length = %d\n", p->length);
			printf("hi6\n");
			printf("temp->length = %d\n", temp->length);
			printf("hi7\n");

			size_t aaa;
			for(i = 0; i< n; i++){
			  temp = vector_peek(rb, i, &RB_ELEM_SIZE);
			  if(temp->id / 100 == hashPart)
			    temp = vector_remove(rb, i, &aaa);
			}
			printf("hi8\n");
			printf(
					"msg removed from vector[%d], msg_id= %d, msg_length = %d\n",
					i, temp->id, temp->length);
			printf("hi9\n");
			free(temp);
			printf("hi10\n");
			return;
		} else {
			printf("NONONONO\n");
		}
	}
	printf("\n");
}
*/
