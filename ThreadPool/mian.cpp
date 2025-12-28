#include <iostream>
#include <chrono>
#include <thread>
#include "threadpool.h"
/*
有些场景是希望能够获取线程执行任务的返回值的
*/
using uLong = unsigned long long;
class MyTask :public Task
{
 public: 
	  MyTask(int begin, int end)
		  :begin_(begin),
		  end_(end)
	  { }
	 //怎么设计run函数的返回值，可以表示任意类型
	Any run() override
	{
		std::cout << "tid: " << std::this_thread::get_id() << "begin" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		uLong sum = 0;
		for (uLong i = begin_; i <= end_; ++i)
		{
			sum += i;
		}
		std::cout << "tid: " << std::this_thread::get_id() << "end" << std::endl;
		return sum;
	}
private:
	int begin_;
	int end_;
};
int main()
{
#if 0
    ThreadPool pool;
	pool.setMode(PoolMode::MODE_CACHED);
	pool.start(4);

	// 随着task被执行完，task对象没了，依赖于task对象的Result对象也没了。所以获取Result不能用task->getResult()的设计方式
	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	uLong sum1 = res1.get().cast_<uLong>();
	uLong sum2 = res2.get().cast_<uLong>();
	uLong sum3 = res3.get().cast_<uLong>();
	std::cout << sum1 + sum2 + sum3 << std::endl;

	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
	//pool.submitTask(std::make_shared<MyTask>());
#endif
	{
		ThreadPool pool;
		pool.start(4);
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		uLong sum1 = res1.get().cast_<uLong>();
		std::cout << sum1 << std::endl;	
	}
	getchar();
	std::cout << "main over" << std::endl;
	return 0;
}