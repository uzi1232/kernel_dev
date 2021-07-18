#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/completion.h>

#define MAX_SIZE_QUEUE 1000

MODULE_LICENSE("GPL");
MODULE_AUTHOR("THE UZI");
MODULE_DESCRIPTION("Single consumer sigle producer queue");

//Thread varaibles
static DECLARE_COMPLETION(thread_done);
static struct task_struct* t1;
static struct task_struct* t2;
int val = 0;

//queue
atomic_t _tail = ATOMIC_INIT(0);
atomic_t _head = ATOMIC_INIT(0);
int _array[MAX_SIZE_QUEUE];

int increment(int idx)
{
  return (idx + 1) % MAX_SIZE_QUEUE;
}

bool push(int item)
{
    int current_tail = atomic_read_acquire(&_tail);
    int next_tail = increment(current_tail);
    if(next_tail != atomic_read_acquire(&_head))
    {
        _array[current_tail] = item;
        atomic_set_release(&_tail, next_tail);
        // ^^ statement -->_tail = next_tail;
        return true;
    }

    return false;  // full queue
}

bool pop(int *item)
{
    int current_head = atomic_read_acquire(&_head);
    if(current_head == atomic_read_acquire(&_tail))
    {
        //printk("The queue is empty");
        return false;   // empty queue
    }

    *item = _array[current_head];
    atomic_set_release(&_head, increment(current_head));
    // ^^ statement -->_head = increment(current_head);
    return true;
}

int consumer(void *ptr)
{
    //pop
    int count = 0;
    while(!kthread_should_stop())
    {
        int value = 0;
        if (pop(&value))
        {
            //printk("The received value is %d\n", value);
            if (count != value)
            {
                printk("ERROR The value expected did not match %d\n", value);
            }
            else
            {
                count++;
            }

            if (count == 9999)
            {
                printk("Done receiving 500 values\n");
            }
        }
        udelay(1000);
        //msleep(1);
    }
    return 0;
}

int producer(void *ptr)
{
    int i = 0;
    //msleep(2000);
    //push
//    while(!kthread_should_stop())
//    {
        for (i = 0; i < 10000; i++)
        {
        //msleep(1000);
            if (!push(i))
            {
                printk("*****Queue is full****");
            }
        }
        printk("Done sending all the values");

//    }
    complete_and_exit(&thread_done, 0);
    return 0;
}

static int __init queue_module_init(void)
{
    t1 = kthread_create(consumer, NULL, "consumer_t");
    if (t1 == NULL)
    {
        printk("Failed to create the consumer thread\n");
        return -1;
    }
    else
    {
        t2 = kthread_create(producer, NULL, "producer_t");
        if (t2 == NULL)
        {
            printk("Failed to create the producer thread\n");
            kthread_stop(t1);
            complete(&thread_done);
        }
        printk("Successfully created both threads\n");
        //run the threads
        wake_up_process(t1);
        wake_up_process(t2);
    }
    return 0;
}

static void __exit queue_module_exit(void)
{
    printk("Stopping the threads...\n");
    kthread_stop(t1);//consumer
    //producer
    printk("waiting for the producer thread to finish ...\n");
    wait_for_completion(&thread_done);
    printk("Stopped the threads\n");
    return;
}

module_init(queue_module_init);
module_exit(queue_module_exit);
