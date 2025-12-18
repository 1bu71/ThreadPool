#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<condition_variable>
#include<mutex>
#include<memory>
#include<atomic>
#include<functional>

//线程池支持两种模式，杜绝枚举类型不同但是枚举项相同冲突的情况
enum class PoolMode {
	MODE_FIXED,
	MODE_CACHED,
};

//任务抽象基类,用户可自定义任意任务类型，从Task继承并实现run方法
class Task
{
public:
	virtual void run() = 0;
};

class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void()>;
	Thread(ThreadFunc func);
	~Thread();
	void start();
private:
	ThreadFunc func_;
};
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	void setMode(PoolMode mode);

	void setTaskQueMaxThreshhold(int threshhold);
	void start(int initthreadsize);
	void submitTask(std::shared_ptr<Task> task);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	//线程函数
	void threadFunc();


	std::vector<std::unique_ptr<Thread>> threads_;//线程列表
	size_t initThreadSize_;//初始线程数量
	//如果用户创建了一个临时的任务对象，并将其加入线程池，线程池销毁时需要释放该任务对象
	std::queue<std::shared_ptr<Task>> taskQue_;
	std::atomic_uint taskSize_;//任务数量
	int taskQueMaxThreshhold_;//任务队列数量的上限

	std::mutex taskQueMtx_;
	//定义两个条件变量，分别是不空和不满
	std::condition_variable notFull_;//表示任务队列不满
	std::condition_variable notEmpty_;//表示任务队列不空

	PoolMode poolMode_;
};

#endif
