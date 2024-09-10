#ifndef QUEUE_H
#define QUEUE_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>

#define MAX_QUEUE_SIZE 50000 // Define the maximum size of the queue

// Define the Queue structure
typedef struct Queue {
    struct Node* array[MAX_QUEUE_SIZE]; // Array to hold the nodes
    int front;                   // Index of the front of the queue
    int rear;                    // Index of the rear of the queue
    int size;                    // Number of elements in the queue
} Queue;

// Function to create and initialize a queue
Queue* createQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (q == NULL) {
        perror("Failed to create queue");
        exit(EXIT_FAILURE);
    }
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    return q;
}

// Function to enqueue a node
void enqueue(Queue* q, struct Node* newNode) {
    if (q == NULL || newNode == NULL) {
        fprintf(stderr, "Invalid queue or node\n");
        return;
    }

    if (q->size == MAX_QUEUE_SIZE) {
        fprintf(stderr, "Queue is full\n");
        return;
    }

    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->array[q->rear] = newNode;
    q->size++;
}

// Function to dequeue a node
struct Node* dequeue(Queue* q) {
    if (q == NULL || q->size == 0) {
        fprintf(stderr, "Queue is empty or invalid queue\n");
        return NULL;
    }

    struct Node* temp = q->array[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return temp;
}

// Function to check if the queue is empty
int isQueueEmpty(Queue* q) {
    return (q == NULL || q->size == 0);
}

// Function to free the queue
void freeQueue(Queue* q) {
    if (q == NULL) return;

    // Free all nodes
    for (int i = 0; i < q->size; i++) {
        struct Node* node = q->array[(q->front + i) % MAX_QUEUE_SIZE];
        free(node); // Assuming you want to free the nodes here
    }

    free(q);
}

#endif
