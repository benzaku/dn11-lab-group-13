#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nl_packet.h"

struct BUF_NODE {
  unsigned int id;
  size_t length;
  char msg[MAX_MESSAGE_SIZE];
  struct BUF_NODE *next;
};

static struct BUF_NODE RB_BUF_END;

unsigned int get_id (NL_PACKET *p){
  return ((unsigned int) (p->src) * 10000000 + (unsigned int) (p->dest) * 10000 + (unsigned int) p->seqno);
}

void RB_init (struct BUF_NODE *rb){
  rb = malloc(sizeof(struct BUF_NODE));
  rb->length = 0;
  rb->next = &RB_BUF_END;
}

void RB_save_msg (struct BUF_NODE *rb, NL_PACKET *p){
   printf("RB_save_msg\n");
   unsigned int id = get_id(p);
  /* UINT_MAX = +4,294,967,295 */
    struct BUF_NODE *temp = rb;
    while (temp->next != &RB_BUF_END){
      if(temp->id == id || temp->length == 0){
	memcpy(&temp->msg[temp->length], (char *) p->msg, p->length);
	temp->length += p->length;
	printf("node existed in recevier buffer, node_id = %d\n\n", temp->id);
	break;
      }
      temp = temp->next;
    }
    //save at rear node or create a new node
    if(temp->next == &RB_BUF_END){
      if(temp->id == id || temp->length == 0){
	memcpy(&temp->msg[temp->length], (char *) p->msg, p->length);
	temp->length += p->length;
	printf("node existed buffer at the rear of receive buffer, node_id = %d\n\n", temp->id);
	struct BUF_NODE *newNode = malloc(sizeof(struct BUF_NODE));
	 newNode->length = 0;
	 newNode->next = &RB_BUF_END;
	 temp->next = newNode;
      }
    }
}

void RB_copy_whole_msg(struct BUF_NODE *rb, NL_PACKET *p){
  printf("RB_copy_whole_msg\n");
  /* UINT_MAX = +4,294,967,295 */
  unsigned int id = get_id(p);
  struct BUF_NODE *temp = rb;
  printf("test");
  if(temp->id == id){
    memcpy(p->msg, temp->msg, temp->length);
    rb = rb->next;
    printf("node to be deleted at the front of receive buffer\n\n");
    free(&temp->msg);
    free(temp);
    printf("node deleted in receive buffer\n\n");
  } else{
    struct BUF_NODE *prev = rb;
    temp = temp->next;
    while(temp != &RB_BUF_END){
      if(temp->id == id){
	memcpy(p->msg, temp->msg, temp->length);
	prev->next = temp->next;
	free(&temp->msg);
	free(temp);
	printf("node deleted in receive buffer\n\n");
	break;
      } else {
	prev = prev->next;
	temp = temp->next;
      }
    }
  }
}


int main(){
 NL_PACKET *p = malloc(sizeof(NL_PACKET));
 p->src = 134;
 p->dest = 96;
 p->kind = NL_ACK;
 p->seqno = 0;
 p->hopcount = 2;
 p->pieceNumber = 2;
 p->pieceEnd = 1;
 p->length = 10;
 p->src_packet_length = 10;
 p->checksum = 1043413;
 p->trans_time = 10;
 p->is_resent = 0;
 memcpy(&p->msg, "hello hao!", 10);
 int i;
 printf("msg: %s\n\n", p->msg);
 struct BUF_NODE *rb;
 rb = &RB_BUF_END;
 RB_init(rb);
 for(i=0; i<10; i++)
    RB_save_msg(rb, p);
 if(rb == &RB_BUF_END)
   printf("rb is null");
 else printf("rb is not null");
 printf("saved msg: %s", rb->msg);
 RB_copy_whole_msg(rb, p);
 printf("whole msg: %s", p->msg);
 
 return 0;
}
