#include "threadpool.h"
#include <iostream>
const int TASK_QUEMAX_THRESHHOLD = 4;

ThreadPool::ThreadPool()
	: initThreadSize_(4),
	taskSize_(0),
	taskQueMaxThreshhold_(TASK_QUEMAX_THRESHHOLD),
	poolMode_(PoolMode::MODE_FIXED)
{}

ThreadPool::~ThreadPool()
{}

void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxThreshhold(int threshhold)
{
	taskQueMaxThreshhold_ = threshhold;
}

void ThreadPool::start(int initthreadthreshhold)
{
	initThreadSize_ = initthreadthreshhold;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		auto ptr = make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}
	for (int i = 0; i < initThreadSize_; ++i)
		threads_[i]->start();
}

//给线程池提交任务，用户调用接口，传入任务，生产任务
void ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	//获取锁
	std::unique_lock <std::mutex> lock(taskQueMtx_);
	//线程的通信，等待任务队列空余
	/*while (taskSize_ == taskQueMaxThreshhold_)
	{
		notFull_.wait(lock);
	}*/
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()-> bool { return taskSize_ < taskQueMaxThreshhold_; }))
	{
		//notFull_等待1s，条件依然不满足
		std::cerr << "task queue is full, submit task fail" << std::endl;
		return;
	}
	//如果有空余，把任务加入到任务队列中
	taskQue_.emplace(task);
	taskSize_++;
	//通知notEmpty_
	notEmpty_.notify_all();
}

//线程池的所有线程从任务队列中取出任务进行消费
void ThreadPool::threadFunc()
{
	while (1)
	{
		std::shared_ptr<Task> task;
		{
			//先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			//等待notEmpty_条件
			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务" << std::endl;
			notEmpty_.wait(lock, [&]()->bool {  return taskSize_ > 0; });
			std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功" << std::endl;
			//从任务队列中取出任务
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			if (taskQue_.size() > 0)
				notEmpty_.notify_all();
			//取出任务，进行通知
			notFull_.notify_all();
		}//出作用域自己释放锁，执行任务的时候不需要持有锁
		//当前线程负责执行这个任务
		if(task != nullptr)
			task->run();
		
	}
}













/////////////////////Thread
Thread::Thread(ThreadFunc func)
	: func_(func)
{}

Thread::~Thread()
{}

void Thread::start()
{
	std::thread t(func_);//线程对象t 线程函数func_ 
	t.detach();//设置分离线程
}