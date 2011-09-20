#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void initQueue(struct queueLK *hq) {
	hq->front = hq->rear = NULL;
	hq->size = 0;
	return;
}

void enQueue(struct queueLK *hq, struct elemType x) {
	struct sNode *newP;
	newP = malloc(sizeof(struct sNode));
	if (newP == NULL) {
		printf("Fail to allocate memory");
		exit(1);
	}
	newP->data = x;
	newP->next = NULL;
	if (hq->rear == NULL) {
		hq->front = hq->rear = newP;
	} else {
		hq->rear = hq->rear->next = newP;
	}
	hq->size++;

	return;
}

struct elemType outQueue(struct queueLK *hq) {
	struct sNode *p;
	struct elemType temp;
	if (hq->front == NULL) {
		printf("Empty queue!");
		exit(1);
	}
	temp = hq->front->data;
	p = hq->front;
	hq->front = p->next;
	if (hq->front == NULL) {
		hq->rear = NULL;
	}
	free(temp.packet);
	hq->size--;
	return temp;
}

struct elemType peekQueue(struct queueLK *hq) {
	if (hq->front == NULL) {
		printf("Empty queue!");
		exit(1);
	}
	return hq->front->data;
}


int emptyQueue(struct queueLK *hq) {

	if (hq->front == NULL) {
		return 1;
	} else {
		return 0;
	}
}


void clearQueue(struct queueLK *hq) {
	struct sNode *p = hq->front;

	while (p != NULL) {
		hq->front = hq->front->next;
		free(p);
		p = hq->front;
	}
	hq->rear = NULL;
	hq->size = 0;
	return;
}
