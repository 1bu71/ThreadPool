#include <iostream>
#include <chrono>
#include <thread>
#include "threadpool.h"
int main()
{
    ThreadPool pool;
	pool.start(4);

	std::this_thread::sleep_for(std::chrono::seconds(5));
	return 0;
}