#include "threadpool.h"
#include <iostream>
const int TASK_QUEMAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_IDLE_TIME = 10; //单位秒
ThreadPool::ThreadPool()
	: initThreadSize_(4),
	taskSize_(0),
	idleThreadSize_(0),
	curThreadSize_(0),
	threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
	taskQueMaxThreshhold_(TASK_QUEMAX_THRESHHOLD),
	poolMode_(PoolMode::MODE_FIXED),
	isPoolRunnig_(false)
{}

ThreadPool::~ThreadPool()
{
	isPoolRunnig_ = false;
	//等待线程池中所有的线程返回，  有两种状态：阻塞、正在执行任务
	notEmpty_.notify_all();
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	exitCond_.wait(lock, [&]()->bool { return threads_.size() == 0; });
}

void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())	return;
	poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxThreshhold(int threshhold)
{
	if (checkRunningState())	return;
	taskQueMaxThreshhold_ = threshhold;
}

void ThreadPool::setTheadSizeMaxThreshhold(int threshhold)
{
	if (checkRunningState())	return;
	if (poolMode_ == PoolMode::MODE_CACHED)
		taskQueMaxThreshhold_ = threshhold;
}

void ThreadPool::start(int initthreadthreshhold)
{
	initThreadSize_ = initthreadthreshhold;
	curThreadSize_ = initthreadthreshhold;
	isPoolRunnig_ = true;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		auto ptr = make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start();
		idleThreadSize_++;
	}
}

//给线程池提交任务，用户调用接口，传入任务，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
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
		return Result(sp, false);	//
	}
	//如果有空余，把任务加入到任务队列中
	taskQue_.emplace(sp);
	taskSize_++;
	//通知notEmpty_
	notEmpty_.notify_all();

	//根据任务数量和空闲线程的数量，判断是否需要创建新的线程
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_)
	{
		//创建新线程
		std::cout << "create new thread" << std::this_thread::get_id() <<std::endl;
		auto ptr = make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();
		curThreadSize_++;
		idleThreadSize_++;
	}

	//返回任务Result对象
	//return task->getResult();	task的生命周期从队列pop出来就会自己析构，线程执行玩task，task就被析构了
	return Result(sp);
}

//线程池的所有线程从任务队列中取出任务进行消费
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	while (isPoolRunnig_)
	{
		std::shared_ptr<Task> task;
		{
			//先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			//等待notEmpty_条件
			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务" << std::endl;

			//cached模式下，可能创建了很多线程，这些线程可能会因为长时间没有任务可做而退出(超过iniThreadSize_数量的线程要进行回收)
			
				//每一秒要返回一次， 区分超时返回还是有任务执行返回
			while (taskQue_.size() == 0)
			{
				if (poolMode_ == PoolMode::MODE_CACHED)
				{

					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							//回收线程
							//记录线程数量的相关变量的值进行修改，把线程对象从线程池列表容器中删除
							//threadFunc <=> thread对象 
							threads_.erase(threadid);
							std::cout << "tid: " << std::this_thread::get_id() << "线程退出" << std::endl;
							curThreadSize_--;
							idleThreadSize_--;
							return; // 线程退出，必须return
						}
					}
				}
				else
				{
					notEmpty_.wait(lock);
				}
				//线程池结束，线程退出
				if (!isPoolRunnig_)
				{
					threads_.erase(threadid);
					std::cout << "tid: " << std::this_thread::get_id() << "线程退出" << std::endl;
					exitCond_.notify_all();
					return;
				}
			}

			idleThreadSize_--;

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
			task->exec();
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); //更新一下最后执行任务的时间	
	}
	threads_.erase(threadid);
	std::cout << "tid: " << std::this_thread::get_id() << "线程退出" << std::endl;
	exitCond_.notify_all();
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunnig_;
}







/////////////////////Thread
int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func)   //线程函数运行结束，线程对象就会被销毁
	: func_(func)
	, threadId_(generateId_++)
{}

Thread::~Thread()
{} 

void Thread::start()
{
	std::thread t(func_, threadId_);//线程对象t 线程函数func_ 
	t.detach();//设置分离线程
}

int Thread::getId() const
{
	return threadId_;
}



//////////////////////Task
Task::Task()
	:result_(nullptr)
{ }

void Task::exec()
{
	if(result_ != nullptr)
		result_->setValue(run());
}

void Task::setResult(Result* result)
{
	result_ = result;
}


//////////////////////Result
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: task_(task),
	isValid_(isValid)
{
	task_->setResult(this);
}

void Result::setValue(Any any)
{
	this->any_ = std::move(any);
	sem_.post();
}

Any Result::get()
{
	if (!isValid_)
		return "";
	sem_.wait();
	return std::move(any_);
}