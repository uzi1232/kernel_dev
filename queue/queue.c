#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#define MAX_SIZE 100

int queue[MAX_SIZE];
atomic_int head;
atomic_int tail;

void arr_init()
{
    int i;
    for(i = 0; i < MAX_SIZE; i++)
    {
        queue[i] = 0;
    }
}

//create a simple queue (not using any syncronization) int push
int increment(int idx)
{
  return (idx + 1) % MAX_SIZE;
}

bool wasEmpty()
{
    return head == tail;
}

bool wasFull()
{
    int next_tail = increment(tail);
    return (next_tail == head);
}

bool push(int item)
{
  int current_tail = tail;
  int next_tail = increment(current_tail);
  if(next_tail != head)
  {
    queue[current_tail] = item;
    atomic_store(&tail, increment(current_tail));
    return true;
  }

  return false;  // full queue
}

bool pop(int* item)
{
    int current_head = head;
    if(current_head == tail)
    {
        return false;   // empty queue
    }

    *item = queue[current_head];
    atomic_store(&head, increment(current_head));
    //head.store(increment(current_head));
    return true;
}

void print_queue()
{
    int cp_head = head;
    int cp_tail = tail;
    if (cp_head == cp_tail)
    {
        printf("The queue is empty");
    }
    while(cp_head != cp_tail)
    {
        printf("%d,", queue[cp_head]);
        cp_head = increment(cp_head);
    }
}

int main()
{
    head = 0;
    tail = 0;
    arr_init();
    for (int i = 12; i < 20; i++)
    {
        push(i);
    }
    for (int i = 55; i < 200; i++)
    {
        push(i);
    }

    print_queue();
    return 0;
}
