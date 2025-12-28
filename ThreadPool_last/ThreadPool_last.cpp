#include <iostream>
#include "ThreadPool.h"
#include <future>
using namespace std;
int main()
{
    ThreadPool pool;

    pool.start(4);
    future<int> r1 = pool.submitTask([](int a, int b)->int {  return a + b; }, 10, 20);
    future<int> r2 = pool.submitTask([](int a, int b, int c)->int {  return a + b + c; }, 10, 20, 30);
    future<int> r3 = pool.submitTask([](int begin, int end)->int {
        int sum = 0;
        for (int i = begin; i <= end; i++)
            sum += i;
        return sum;
        }, 1, 100);
    cout << r1.get() << endl;
    cout << r2.get() << endl;
    cout << r3.get() << endl;
}
