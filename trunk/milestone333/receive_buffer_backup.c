#include <cnetsupport.h>
#include "nl_packet.h"


size_t RB_ELEM_SIZE = sizeof(RB_BUF_ELEM);


unsigned int rbTable_get_id(NL_PACKET *p, int arrive_on_link){
	unsigned int hashPart = (unsigned int) (p->src * 13) + (unsigned int) (p->dest * 7) + (unsigned int) p->seqno;
	return (unsigned int) 100 * hashPart + (unsigned int) arrive_on_link;
}



void RB_init(VECTOR rb) {
	rb = vector_new();
}

int rbTable_save_piece_msg(HASHTABLE rbTable, NL_PACKET *p, int arrive_on_link) {
	
        char *p_msg;
        size_t len;
	char *rbTableKey = (char*)(int) rbTable_get_id(p, arrive_on_link);
        
        p_msg = hashtable_find(rbTable, rbTableKey, &len);
        
        if(p_rbTable){
            p_msg
        
        
        }
        else{
            memcpy(rbTableContent, (char*) p->msg, p->length);
            hashtable_add(rbTable, rbTableKey, rbTableContent, p->length);
        
        }
        
        
        
        for (i = 0; i < n; i++) {
                temp = vector_peek(rb, i, &RB_ELEM_SIZE);
                if (temp->id == id) {
                    //backup to new elem because of vector_replace
                        
                    memcpy(&temp->msg[p->pieceStartPosition], (char *) p->msg, p->length);
                    temp->length += p->length;
                    return 1;
                
                
                }
        }
        
        //key not found, created new entry in verctor
        if (i == n) {
                
            bufelem.id = id;
            bufelem.length = p->length;
            memcpy(bufelem.msg, (char *) p->msg, p->length);
            vector_append(rb, &bufelem, RB_ELEM_SIZE);
            return 1;
        }
	
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
