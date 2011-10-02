#include <cnetsupport.h>
#include "nl_packet.h"

typedef struct {
	unsigned int id;
	size_t length;
	char msg[MAX_MESSAGE_SIZE];
} RB_BUF_ELEM;

size_t RB_ELEM_SIZE = sizeof(RB_BUF_ELEM);


unsigned int RB_get_id_link(NL_PACKET *p, int arrive_on_link){
	unsigned int hashPart = (unsigned int) (p->src * 13) + (unsigned int) (p->dest * 7) + (unsigned int) p->seqno;
	return (unsigned int) 100 * hashPart + (unsigned int) arrive_on_link;
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
        
	
        for (i = 0; i < n; i++) {
                temp = vector_peek(rb, i, &RB_ELEM_SIZE);
                if (temp->id == id) {
                    //backup to new elem because of vector_replace
                        
                    memcpy(&temp->msg[p->pieceStartPosition], (char *) p->msg, p->length);
                    temp->length += p->length;
                    if(temp->length == p->src_packet_length) return 2;
                    else return 1;
                
                
                }
        }
        
        //id not found, created new entry in vector
        if (i == n) {
                
            bufelem.id = id;
            bufelem.length = p->length;
            memcpy(bufelem.msg, (char *) p->msg, p->length);
            vector_append(rb, &bufelem, RB_ELEM_SIZE);
            return 1;
        }
	return 0;
}

void RB_copy_whole_msg_link(VECTOR rb, NL_PACKET *p, int arrive_on_link) {
	
	unsigned int id = RB_get_id_link(p, arrive_on_link);
	unsigned int hashPart = id / 100;
	
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

}

// int RB_find_missing_piece(VECTOR rb, NL_PACKET *p, int arrive_on_link){
// 
//     unsigned int id = RB_get_id_link(p, arrive_on_link);
//     int n = vector_nitems(rb);
//     RB_BUF_ELEM *bufelem;
//     int i, j;
//     
//     for(i = 0; i < n; i++){
//         
//         bufelem = vector_peek(rb, i, &RB_ELEM_SIZE);
//         if(bufelem->id == id){
//             
//             for(j = 0; j < p->src_packet_length; j = j + p->length){
//                 if(!bufelem->msg[j]) break;
//             
//             }
//             
//             break;
//         }
//     }
//     return j;
// 
// }
