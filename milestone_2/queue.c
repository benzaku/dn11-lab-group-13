#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void initQueue(struct queueLK *hq) {
	hq->front = hq->rear = NULL;
	hq->size = 0;
	return;
}

void enQueue(struct queueLK *hq, MSG* msg) {
	struct sNode *newP;
	newP = malloc(sizeof(struct sNode));
	if (newP == NULL) {
		printf("Fail to allocate memory");
		exit(1);
	}
	newP->msg = msg;
	newP->next = NULL;
	if (hq->rear == NULL) {
		hq->front = hq->rear = newP;
	} else {
		hq->rear = hq->rear->next = newP;
	}
	hq->size++;
}

void outQueue(struct queueLK *hq) {
	struct sNode *p;
	if (hq->front == NULL) {
		printf("Empty queue!");
		exit(1);
	}
	p = hq->front;
	hq->front = p->next;
	if (hq->front == NULL) {
		hq->rear = NULL;
	}
	free(p->msg);
	free(p);
	hq->size--;
}

MSG* peekQueue(struct queueLK *hq) {
	if (hq->front == NULL) {
		printf("Empty queue!");
		exit(1);
	}
	return hq->front->msg;
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
		free(p->msg);
		free(p);
		p = hq->front;
	}
	hq->rear = NULL;
	hq->size = 0;
	return;
}
