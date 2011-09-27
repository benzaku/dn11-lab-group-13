#include <cnetsupport.h>
#include "nl_packet.h"
//#include "vector.c"

typedef struct {
  unsigned int id;
  size_t length;
  char msg[MAX_MESSAGE_SIZE];
} RB_BUF_ELEM;

size_t RB_ELEM_SIZE = sizeof(RB_BUF_ELEM);
VECTOR rb;//receive buffer

unsigned int RB_get_id (NL_PACKET *p){
  return ((unsigned int) (p->src) * 10000000 + (unsigned int) (p->dest) * 10000 + (unsigned int) p->seqno);
}

void RB_init(){
  rb = vector_new(); 
}

void RB_save_msg (NL_PACKET *p){
  RB_BUF_ELEM *temp;
  unsigned int id = RB_get_id(p);
  int n = vector_nitems(rb);
  while(n > 0){
      temp = vector_peek(rb, n, &RB_ELEM_SIZE);
      if(temp->id == id){
	memcpy(&temp->msg[temp->length], (char *) p->msg, p->length);
	temp->length += p->length;
	vector_replace(rb, n, temp, RB_ELEM_SIZE);
	break;
      }
      --n;
    }
  if(n == 0){
    temp = malloc(RB_ELEM_SIZE);
    temp->id = RB_get_id(p);
    temp->length = p->length;
    memcpy(&temp->msg[temp->length], (char *) p->msg, p->length);
    vector_append(rb, temp, RB_ELEM_SIZE);
  }
}

void RB_copy_whole_msg(NL_PACKET *p){
  RB_BUF_ELEM *temp;
  unsigned int id = RB_get_id(p);
  int n = vector_nitems(rb);
  while(n >0){
    temp = vector_peek(rb, n, &RB_ELEM_SIZE);
    if(temp->id == id){
      memcpy(p->msg, temp->msg, temp->length);
      vector_remove(rb, n, &RB_ELEM_SIZE);
      break;
    }
  }
}