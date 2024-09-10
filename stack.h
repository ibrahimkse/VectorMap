#ifndef STACK_H
#define STACK_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "rtree.h"

#define STACK_SIZE 10000

// Define the stack structure
typedef struct {
    struct Node* items[STACK_SIZE];
    int size;
    int top;
} Stack;

// Function prototypes
void initStack(Stack* s);
bool isEmpty(const Stack* s);
bool isFull(const Stack* s);
bool push(Stack* s, struct Node* item);
struct Node* pop(Stack* s);
struct Node* peek(const Stack* s);

// Initialize the stack
void initStack(Stack* s) {
    s->top = -1;
    s->size = 0;
}

// Check if the stack is empty
bool isEmpty(const Stack* s) {
    return s->top == -1;
}

// Check if the stack is full
bool isFull(const Stack* s) {
    return s->top == STACK_SIZE - 1;
}

// Push an item onto the stack
bool push(Stack* s, struct Node* item) {
    if (isFull(s)) {
        return false; // Stack is full
    }
    s->items[++s->top] = item;
    s->size++;
    return true;
}

// Pop an item from the stack
struct Node* pop(Stack* s) {
    if (isEmpty(s)) {
        return false; // Stack is empty
    }
    s->size--;
    return s->items[s->top--];
}

// Peek at the top item of the stack
struct Node* peek(const Stack* s) {
    if (isEmpty(s)) {
        return false; // Stack is empty
    }
    return s->items[s->top];
}

#endif // !STACK_H