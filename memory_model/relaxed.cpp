#include <atomic>
#include <thread>
#include <assert.h>
#include <iostream>

std::atomic<bool> x,y;
std::atomic<int> z;

void write_x_then_y()
{
    y.store(true, std::memory_order_relaxed);
    x.store(true, std::memory_order_relaxed);
}

void read_y_then_x()
{
    while(!y.load(std::memory_order_relaxed));
    if (x.load(std::memory_order_relaxed))
    {
        ++z;
    }
}

int main ()
{
    x = false;
    y = false;
    z = 0;
    std::thread b(read_y_then_x);
    std::thread a(write_x_then_y);
    a.join();
    b.join();
    assert(z.load() != 0);
    return 0;
//    std::cout << "The final value of z is " << z.load() << std::endl;
}
