#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <cnet.h>

typedef struct
{
    char        data[MAX_MESSAGE_SIZE];
} MSG;

struct sNode{
    MSG* msg;
    struct sNode *next;
};
struct queueLK{
    struct sNode *front;
    struct sNode *rear;
    int size;
};


extern void initQueue(struct queueLK *hq);


extern void enQueue(struct queueLK *hq, MSG x);


extern struct elemType outQueue(struct queueLK *hq);


extern struct elemType peekQueue(struct queueLK *hq);


extern int emptyQueue(struct queueLK *hq);


extern void clearQueue(struct queueLK *hq);

#endif
