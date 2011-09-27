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
  printf("RB_save_msg\n");
  printf("packet to be saved: src = %d, des = %d, seqno = %d\n", p->src, p->dest, p->seqno);
  RB_BUF_ELEM *temp;
  unsigned int id = RB_get_id(p);
  printf("calc_id = %d\n", id);
  int i;
  int n = vector_nitems(rb);
  if(n == 0){
    temp = malloc(RB_ELEM_SIZE);
    temp->id = id;
    temp->length = p->length;
    memcpy(temp->msg, (char *) p->msg, p->length);
    vector_append(rb, temp, RB_ELEM_SIZE);
    temp = vector_peek(rb, 0, &RB_ELEM_SIZE); // debug
    printf("vector empty: msg saved at vector[%d], msg_id= %d, msg_length = %d\n", 0, temp->id, temp->length);
  } else{
    for(i=0;i<n; i++){
      temp = vector_peek(rb, i, &RB_ELEM_SIZE);
      if(temp->id == id){
	//backup to new elem because of vector_replace
	RB_BUF_ELEM elem;
	elem.id = id;
	elem.length = temp->length;
	memcpy(&elem.msg, &temp->msg, elem.length);
	
	memcpy(&elem.msg[elem.length], (char *) p->msg, p->length);
	elem.length += p->length;
	vector_replace(rb, i, &elem, RB_ELEM_SIZE);
	temp = vector_peek(rb, i, &RB_ELEM_SIZE); // debug
	printf("elem found: msg saved at vector[%d], msg_id= %d, msg_length = %d\n", i, temp->id, temp->length);
	break;
      }
    }
    if(i == n){
	temp = malloc(RB_ELEM_SIZE);
	temp->id = RB_get_id(p);
	temp->length = p->length;
	memcpy(temp, (char *) p->msg, p->length);
	vector_append(rb, temp, RB_ELEM_SIZE);
	temp = vector_peek(rb, i, &RB_ELEM_SIZE); // debug
	printf("create new elem: msg saved at vector[%d], msg_id= %d, msg_length = %d\n", i, temp->id, temp->length);
     }
  }
  printf("\n");
}

void RB_copy_whole_msg(NL_PACKET *p){
  printf("RB_copy_whole_msg\n");
  printf("packet to be removed: src = %d, des = %d, seqno = %d\n", p->src, p->dest, p->seqno);
  RB_BUF_ELEM *temp;
  unsigned int id = RB_get_id(p);
  printf("calc_id = %d\n", id);
  int i;
  int n = vector_nitems(rb);
  for(i=0; i<n;i++){
    temp = vector_peek(rb, i, &RB_ELEM_SIZE);
    if(temp->id == id){
      memcpy(p->msg, temp->msg, temp->length);
      temp = vector_remove(rb, i, &RB_ELEM_SIZE);
      printf("msg removed from vector[%d], msg_id= %d, msg_length = %d\n", i, temp->id, temp->length);
      //free(temp->msg);
      //free(temp);
      //temp = NULL;
      //printf("memory free\n");
      break;
    }
  }
  printf("\n");
}