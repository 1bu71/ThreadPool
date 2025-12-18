#include "threadpool.h"
#include <iostream>
const int TASK_QUEMAX_THRESHHOLD = 1024;

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
		std::cerr << "task queue is full, submit task fail" << std::endl;
	}
	//如果有空余，把任务加入到任务队列中
	taskQue_.emplace(task);
	taskSize_++;
	//通知notEmpty_
	notEmpty_.notify_all();
}

void ThreadPool::threadFunc()
{
	std::cout << "begin threadFunc" << std::this_thread::get_id() << std::endl;
	std::cout << "end threadFunc" << std::this_thread::get_id() << std::endl;
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