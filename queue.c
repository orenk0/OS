#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <threads.h>
#include <stdbool.h>


typedef struct Node {//normal node for the queue
    void *data;
    struct Node *next;
} Node;

typedef struct Queue {//struct for the queue
    Node *head;
    Node *tail;
    atomic_size_t size;
    atomic_size_t waiting_threads;//currently that is
    atomic_size_t visited_items;//entered and left items
} Queue;

// Node for the linked list of cnd vars
typedef struct CondNode {
    cnd_t cond;
    thrd_t id;
    struct CondNode *next;
} CondNode;

// Queue structure
typedef struct {
    CondNode *head;
    CondNode *tail;
} CondQueue;

CondQueue condQueue;
Queue queue;
mtx_t lock;//global lock for everyone.

void initQueue(void) {//init all parameters
    queue.head = NULL;
    queue.tail = NULL;
    queue.size = 0;
    queue.waiting_threads = 0;
    queue.visited_items = 0;
    mtx_init(&lock, mtx_plain);
}

void destroyQueue(void) {
    //1 atomic block
    mtx_lock(&lock);
    while (queue.head != NULL) {//freeing queue
        Node *temp = queue.head;
        queue.head = queue.head->next;
        free(temp);
    }
    queue.head = NULL;
    queue.tail = NULL;
    queue.size = 0;
    queue.waiting_threads = 0;
    queue.visited_items = 0;


    while (condQueue.head != NULL) {//freeing condqueue
        CondNode *temp = condQueue.head;
        condQueue.head = condQueue.head->next;
        cnd_destroy(&temp->cond);
        free(temp);
    }
    condQueue.head = NULL;
    condQueue.tail = NULL;
    //now head points to null and the lists are freed
    mtx_unlock(&lock);// lock remained until now
    mtx_destroy(&lock);//destroying it as well.
}

void enqueue(void *item) {
    //1 atomic block
    mtx_lock(&lock);
    //create new node to the queue:
    Node *new_node = malloc(sizeof(Node));
    new_node->data = item;
    new_node->next = NULL;
    //edge case:
    if (queue.size == 0) {
        queue.head = new_node;
        queue.tail = new_node;
    } else {
        queue.tail->next = new_node;
        queue.tail = new_node;
    }
    queue.size++;//changing size accordingly now in atomic block the order doesnt matter
    if (queue.waiting_threads > 0) {
        cnd_signal(&(condQueue.head->cond));//wake up first in line. (i.e the head in condQueue)
    }
    mtx_unlock(&lock);
}

void *dequeue(void) {
    //1 atomic block:
    mtx_lock(&lock);    
    //add this id to condQueue:
    CondNode *new_node = malloc(sizeof(CondNode));
    new_node->id = thrd_current();
    new_node->next = NULL;

    cnd_init(&new_node->cond);//unique cnd variable for this thread

    if (condQueue.head == NULL) {//edge case
        condQueue.head = new_node;
        condQueue.tail = new_node;
    } else {
        condQueue.tail->next = new_node;
        condQueue.tail = new_node;
    }

    queue.visited_items++;
    while (queue.size == 0 || new_node->id!=condQueue.head->id) {// handle spurious wake-up for cnd_wait. also makes sure were in order.
        queue.waiting_threads++;//can't enqueue on this line because the lock.
        cnd_wait(&new_node->cond, &lock);//only now the lock is freed
        queue.waiting_threads--;//spurious wakeup maybe
    }
    

    //wer'e now locked again and for sure we have an element to remove intended for us (FIFO-wise)
    //remove it then:
    Node *temp = queue.head;
    queue.head = queue.head->next;
    if (queue.head == NULL) {//this was the last in the queue.
        queue.tail = NULL;
    }
    void *data = temp->data;
    free(temp);
    queue.size--;

    //now remove the current id from the requests queue:
    condQueue.head = condQueue.head->next;
    if (condQueue.head == NULL) {//this was the last in the queue.
        condQueue.tail = NULL;
    }
    //before finishing wake up the next dequeue if exists (could be that there are no more enqueues afterwards):
    else {
        cnd_signal(&(condQueue.head->cond));
    }
    mtx_unlock(&lock);

    return data;//probably meeant to return this.
}

bool tryDequeue(void **item) {
    mtx_lock(&lock);
    if (queue.size == 0) {
        mtx_unlock(&lock);
        return false;//as described.
    }//else:
    Node *temp = queue.head;
    //remove head (exists)
    queue.head = queue.head->next;
    if (queue.size == 0) {
        queue.tail = NULL;
    }
    *item = temp->data;
    free(temp);
    queue.size--;//removed indeed.
    queue.visited_items++;
    mtx_unlock(&lock);
    return true;
}
//the rest is very simple because we saved all this data: 
size_t size(void) {
    return queue.size;
}

size_t waiting(void) {
    return queue.waiting_threads;
}

size_t visited(void) {
    return queue.visited_items;
}
