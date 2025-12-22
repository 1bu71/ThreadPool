#include <iostream>
#include <chrono>
#include <thread>
#include "threadpool.h"
/*
有些场景是希望能够获取线程执行任务的返回值的
*/
class MyTask :public Task
{
 public:
	 //怎么设计run函数的返回值，可以表示任意类型
	void run() override
	{
		std::cout << "tid: " << std::this_thread::get_id() << "begin" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::cout << "tid: " << std::this_thread::get_id() << "end" << std::endl;
	}
};
int main()
{
    ThreadPool pool;
	pool.start(4);
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());

	getchar();
	return 0;
}