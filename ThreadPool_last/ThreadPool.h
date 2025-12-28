#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<condition_variable>
#include<mutex>
#include<memory>
#include<atomic>
#include<functional>
#include<chrono>
#include<unordered_map>
#include<future>
#include<thread>
#include<iostream>

const int TASK_QUEMAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_IDLE_TIME = 10; //单位秒
//线程池支持两种模式，杜绝枚举类型不同但是枚举项相同冲突的情况
enum class PoolMode {
	MODE_FIXED,
	MODE_CACHED,
};

class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func)
		: func_(func)
		, threadId_(generateId_++)
	{}
	~Thread() = default;
	void start()
	{
		std::thread t(func_, threadId_);//线程对象t 线程函数func_ 
		t.detach();//设置分离线程
	}
	int getId() const
	{
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;
};
int Thread::generateId_ = 0;
class ThreadPool
{
public:
	ThreadPool()
		: initThreadSize_(4),
		taskSize_(0),
		idleThreadSize_(0),
		curThreadSize_(0),
		threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
		taskQueMaxThreshhold_(TASK_QUEMAX_THRESHHOLD),
		poolMode_(PoolMode::MODE_FIXED),
		isPoolRunnig_(false)
	{}

	~ThreadPool()
	{
		isPoolRunnig_ = false;
		//等待线程池中所有的线程返回，  有两种状态：阻塞、正在执行任务

		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool { return threads_.size() == 0; });
	}

	void setMode(PoolMode mode)
	{
		if (checkRunningState())	return;
		poolMode_ = mode;
	}

	void setTaskQueMaxThreshhold(int threshhold)
	{
		if (checkRunningState())	return;
		taskQueMaxThreshhold_ = threshhold;
	}

	//设置线程池cached模式下的线程上限阈值
	void setTheadSizeMaxThreshhold(int threshhold)
	{
		if (checkRunningState())	return;
		if (poolMode_ == PoolMode::MODE_CACHED)
			taskQueMaxThreshhold_ = threshhold;
	}

	void start(int initthreadthreshhold)
	{
		initThreadSize_ = initthreadthreshhold;
		curThreadSize_ = initthreadthreshhold;
		isPoolRunnig_ = true;
		for (int i = 0; i < initThreadSize_; ++i)
		{
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
		}
		for (int i = 0; i < initThreadSize_; ++i)
		{
			threads_[i]->start();
			idleThreadSize_++;
		}
	}

	//给线程提交任务
	//使用可变参模板变成，让submiTask可以接受任意任务函数和任意数量的参数
	//pool.submitTask(sum1, 10, 20);
	//返回值future<>

	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		//打包任务放入任务队列
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();
		//获取锁
		std::unique_lock <std::mutex> lock(taskQueMtx_);
		//线程的通信，等待任务队列空余
		if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()-> bool { return taskSize_ < taskQueMaxThreshhold_; }))
		{
			//notFull_等待1s，条件依然不满足
			std::cerr << "task queue is full, submit task fail" << std::endl;
			auto emptyTask = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {  return RType(); });
			(*emptyTask)();
			return emptyTask->get_future();	
		}
		//如果有空余，把任务加入到任务队列中
		//taskQue_.emplace(sp);
		//using Task = std::function<void()>;
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;
		//通知notEmpty_
		notEmpty_.notify_all();

		//根据任务数量和空闲线程的数量，判断是否需要创建新的线程
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshHold_)
		{
			//创建新线程
			std::cout << "create new thread" << std::this_thread::get_id() << std::endl;
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			threads_[threadId]->start();
			curThreadSize_++;
			idleThreadSize_++;
		}

		//返回任务Result对象
		//return task->getResult();	task的生命周期从队列pop出来就会自己析构，线程执行玩task，task就被析构了
		return result;
	}
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	//线程函数
	void threadFunc(int threadid)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();
		//任务执行完 
		while (1)
		{
			Task task;
			{
				//先获取锁
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				//等待notEmpty_条件
				std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务" << std::endl;

				while (taskQue_.size() == 0)
				{
					if (!isPoolRunnig_)
					{
						threads_.erase(threadid);
						std::cout << "tid: " << std::this_thread::get_id() << "exit" << std::endl;
						exitCond_.notify_all();
						return; //线程函数结束，线程结束
					}
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						//cached模式下，可能创建了很多线程，这些线程可能会因为长时间没有任务可做而退出(超过iniThreadSize_数量的线程要进行回收)
						//每一秒要返回一次， 区分超时返回还是有任务执行返回
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
				}
				//线程池结束，线程退出

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
			if (task != nullptr)
				//task->exec();
				task();
			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); //更新一下最后执行任务的时间	
		}

	}
	//检查pool的运行状态
	bool checkRunningState() const
	{
		return isPoolRunnig_;
	}


	//std::vector<std::unique_ptr<Thread>> threads_;//线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	size_t initThreadSize_;//初始线程数量
	std::atomic_uint idleThreadSize_;//空闲线程数量
	size_t threadSizeThreshHold_;//线程数量上限
	std::atomic_int curThreadSize_;//记录当前线程池里面线程的总数量

	//Task任务就是函数对象
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;
	std::atomic_uint taskSize_;//任务数量
	size_t taskQueMaxThreshhold_;//任务队列数量的上限

	std::mutex taskQueMtx_;
	//定义两个条件变量，分别是不空和不满
	std::condition_variable notFull_;//表示任务队列不满
	std::condition_variable notEmpty_;//表示任务队列不空
	std::condition_variable exitCond_;//表示线程池退出时，等待所有线程退出的条件变量

	PoolMode poolMode_;
	std::atomic_bool isPoolRunnig_; //表示当前线程池的启动状态
};

#endif
