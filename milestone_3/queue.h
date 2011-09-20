#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <cnet.h>


/************************************************************************/
/*                    以下是关于队列链接存储操作的6种算法               */
/************************************************************************/
struct elemType
{
	int link;
	size_t length;
	char *packet;
};
struct sNode{
    struct elemType data;            /* 值域 */
    struct sNode *next;        /* 链接指针 */
};
struct queueLK{
    struct sNode *front;    /* 队首指针 */
    struct sNode *rear;        /* 队尾指针 */
    int size;
};

/* 1.初始化链队 */
extern void initQueue(struct queueLK *hq);

/* 2.向链队中插入一个元素x */
extern void enQueue(struct queueLK *hq, struct elemType x);

/* 3.从队列中删除一个元素 */
extern struct elemType outQueue(struct queueLK *hq);

/* 4.读取队首元素 */
extern struct elemType peekQueue(struct queueLK *hq);

/* 5.检查链队是否为空，若为空则返回1, 否则返回0 */
extern int emptyQueue(struct queueLK *hq);

/* 6.清除链队中的所有元素 */
extern void clearQueue(struct queueLK *hq);

#endif