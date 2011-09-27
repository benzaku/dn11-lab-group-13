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

static struct BUF_NODE *rb = NULL;

unsigned int get_id (NL_PACKET *p){
  return ((unsigned int) (p->src) * 10000000 + (unsigned int) (p->dest) * 10000 + (unsigned int) p->seqno);
}

void RB_save_msg (NL_PACKET *p){
  /* UINT_MAX = +4,294,967,295 */
  unsigned int id = get_id(p);
  printf("RB_save_msg\n");
  if (rb == NULL) {
    rb = malloc(sizeof(struct BUF_NODE));
    rb->data->id = id;
    memcpy(&rb->data->msg[0], (char *) p->msg, p->length);
    rb->data->length = p->length;
    rb->next = NULL;
    printf("receive buffer initialized, node_id = %d\n\n", rb->data->id);
  } 
  else {
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

void RB_copy_whole_msg(NL_PACKET *p){
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