#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nl_packet.h"

typedef struct {
  unsigned int id;
  size_t length;
  char msg[MAX_MESSAGE_SIZE];
} BUF_DATA;

struct BUF_NODE {
  BUF_DATA *data;
  struct BUF_NODE *next;
};

unsigned int get_id (NL_PACKET *p){
  return ((unsigned int) (p->src) * 10000000 + (unsigned int) (p->dest) * 10000 + (unsigned int) p->seqno);
}

void RB_save_msg (struct BUF_NODE *rb, NL_PACKET *p){
   printf("RB_save_msg\n");
   unsigned int id = get_id(p);
  /* UINT_MAX = +4,294,967,295 */

  if (!rb) {
    printf("if");
    rb = malloc(sizeof(struct BUF_NODE));
    rb->data->id = id;
    memcpy(&rb->data->msg[0], (char *) p->msg, p->length);
    rb->data->length = p->length;
    rb->next = NULL;
    printf("receive buffer initialized, node_id = %d\n\n", rb->data->id);
  } 
  else {
    printf("else");
    struct BUF_NODE *temp = rb;
    while (temp->next != NULL){
      if(temp->data->id == id){
	memcpy(&temp->data->msg[temp->data->length], (char *) p->msg, p->length);
	temp->data->length += p->length;
	printf("node existed in recevier buffer, node_id = %d\n\n", temp->data->id);
	break;
      }
      temp = temp->next;
    }
    //save at rear node or create a new node
    if(temp->next == NULL){
      if(temp->data->id == id){
	memcpy(&temp->data->msg[temp->data->length], (char *) p->msg, p->length);
	temp->data->length += p->length;
	printf("node existed buffer at the rear of receive buffer, node_id = %d\n\n", temp->data->id);
      } else {
	struct BUF_NODE *newNode = malloc(sizeof(struct BUF_NODE));
	newNode->data->id = id;
	memcpy(newNode->data->msg, (char *) p->msg, p->length);
	newNode->data->length = p->length;
	newNode->next = NULL;
	temp->next = newNode;
	printf("new node created in receive buffer, node_id = %d\n\n", newNode->data->id);
      }
    }
  }
}

void RB_copy_whole_msg(struct BUF_NODE *rb, NL_PACKET *p){
  printf("RB_copy_whole_msg\n");
  /* UINT_MAX = +4,294,967,295 */
  unsigned int id = get_id(p);
  struct BUF_NODE *temp = rb;
  if(temp->data->id == id){
    memcpy(p->msg, temp->data->msg, temp->data->length);
    rb = rb->next;
    printf("node to be deleted at the front of receive buffer\n\n");
    free(&temp->data->msg);
    free(temp);
    printf("node deleted in receive buffer\n\n");
  } else{
    struct BUF_NODE *prev = rb;
    temp = temp->next;
    while(temp != NULL){
      if(temp->data->id == id){
	memcpy(p->msg, temp->data->msg, temp->data->length);
	prev->next = temp->next;
	free(&temp->data->msg);
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

void main(){
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
 struct BUF_NODE rb;
 for(i=0; i<10;i++)
    RB_save_msg(&rb, p);
}