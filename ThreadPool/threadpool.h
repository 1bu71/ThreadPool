#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<condition_variable>
#include<mutex>
#include<memory>
#include<atomic>
#include<functional>

//Any类型，可以接受任意类型的数据

class Any
{
public:
	Any() = default;
	~Any() = default;
	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data)) {}

	template<typename T>
	T cast_()
	{
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private: 
	class Base
	{
		virtual ~Base() = default;
	};
	template<typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data_(data) {}
		T data_;
	};
	//定义一个基类指针,可以指向任何派生类对象
	std::unique_ptr<Base> base_;
};

class Semphore
{
public:
	Semphore(int limit = 0):resLimit_(limit)
	{}
	~Semphore() = default;

	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cv_.wait(lock, [&]()->bool {  return resLimit_ > 0; });
		resLimit_--;
	}
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cv_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cv_;

};
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
/*
example:
ThreadPool pool;
pool.start(4);
class MyTask :public Task
{
 public:
	void run() override
	{
		//线程代码
	}
}
pool.submitTask(std::make_shared<MyTask>());
*/


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
	size_t taskQueMaxThreshhold_;//任务队列数量的上限

	std::mutex taskQueMtx_;
	//定义两个条件变量，分别是不空和不满
	std::condition_variable notFull_;//表示任务队列不满
	std::condition_variable notEmpty_;//表示任务队列不空

	PoolMode poolMode_;
};

#endif
