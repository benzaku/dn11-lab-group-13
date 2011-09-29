//#include <cnetsupport.h>
#include "nl_packet.h"

typedef struct {
	unsigned int id;
	size_t length;
	char msg[MAX_MESSAGE_SIZE];
} RB_DATA;

struct RB_NODE{
  RB_DATA data;
  struct RB_NODE *next;

};

struct RB_NODE *rb_head, *rb_tail;

unsigned int RB_get_id(NL_PACKET *p) {
	return ((unsigned int) (p->src) * 10000000 + (unsigned int) (p->dest)
			* 10000 + (unsigned int) p->seqno);
}

unsigned int RB_get_id_link(NL_PACKET *p, int arrive_on_link){
	unsigned int hashPart = (unsigned int) (p->src * 13) + (unsigned int) (p->dest * 7) + (unsigned int) p->seqno;
	return (unsigned int) 100 * hashPart + (unsigned int) arrive_on_link;
}

void RB_init(struct RB_NODE *rb_head, struct RB_NODE *rb_tail) {
    rb_head->next = rb_tail;
    rb_tail->next = rb_head;
}



void RB_save_msg_link(struct RB_NODE *rb_head, struct RB_NODE *rb_tail, NL_PACKET *p, int arrive_on_link) {
	printf("RB_save_msg\n");
	printf("packet to be saved: src = %d, des = %d, seqno = %d\n, current = %d ", p->src,
			p->dest, p->seqno, nodeinfo.address);
	if((int) p->length > linkinfo[arrive_on_link].mtu - PACKET_HEADER_SIZE || (int) p->length < 0){
	  fprintf(stdout, "RB_save_msg_link, p->length = %d", (int) p->length);
	  return;
	}
	unsigned int id = RB_get_id_link(p, arrive_on_link);
	unsigned int short is_saved = 0;
	struct RB_NODE *temp;
	temp = rb_head;
	while (temp->next != rb_tail){//search whole receive buffer except rb_head and rb_tail
	  if (temp->data.id == id) {
	    memcpy(temp->data.msg, (char *) p->msg, p->length);
	    temp->data.length += p->length;
	    is_saved = 1;
	    break;
	  }
	  temp = temp->next;
	}	
	if(is_saved == 0){//found target node before rb_tail
	  if (temp->data.id == id) {
	    memcpy(temp->data.msg, (char *) p->msg, p->length);
	    temp->data.length += p->length;
	  }else {//create new node before rb_tail
	    struct RB_NODE *node = malloc(sizeof(struct RB_NODE));
	    node->data.id = id;
	    memcpy(node->data.msg, (char *) p->msg, p->length);
	    node->data.length = p->length;
	    
	    node->next = rb_tail;
	    temp->next = node;
	  }
	}
	//printf("\n");
}

void RB_copy_whole_msg_link(struct RB_NODE *rb_head, struct RB_NODE *rb_tail, NL_PACKET *p, int arrive_on_link) {
	/*
	printf("RB_copy_whole_msg\n");
	printf("packet to be removed: src = %d, des = %d, seqno = %d\n, current = %d ", p->src,
			p->dest, p->seqno, nodeinfo.address);
	*/
	unsigned int id = RB_get_id_link(p,arrive_on_link);
	unsigned int hashPart = id / 100;
	//printf("calc_id = %d\n", id);
	struct RB_NODE *prev, *temp;
	prev = rb_head;
	temp = rb_tail->next;
	while(temp != rb_tail){
		if (temp->data.id == id) {
			memcpy(p->msg, temp->data.msg, temp->data.length);
			p->length = temp->data.length;
			
			prev->next = temp->next;
			free(temp);
			temp = prev->next;
			continue;
		}
		if(temp->data.id/100 == hashPart){
			prev->next = temp->next;
			free(temp);
			temp = prev->next;
			continue;
		}
		prev = prev->next;
		temp = temp->next;
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
