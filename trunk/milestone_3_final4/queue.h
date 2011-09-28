#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <cnet.h>

struct elemType {
	int link;
	size_t length;
	char *packet;
};
struct sNode {
	struct elemType data;
	struct sNode *next;
};
struct queueLK {
	struct sNode *front;
	struct sNode *rear;
	int size;
};

extern void initQueue(struct queueLK *hq);

extern void enQueue(struct queueLK *hq, struct elemType x);

extern struct elemType outQueue(struct queueLK *hq);

extern struct elemType peekQueue(struct queueLK *hq);

extern int emptyQueue(struct queueLK *hq);

extern void clearQueue(struct queueLK *hq);

#endif
