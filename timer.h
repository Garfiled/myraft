#ifndef	TIMER_H
#define	TIMER_H
#include <iostream>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable> 

class TimerManager;
 
class Timer
{
public:
	enum class TimerType{ONCE=0,CIRCLE=1};
 
    Timer (TimerManager& manager);
    ~Timer ();
	//启动一个定时器
	template<typename Func>
    void   start (Func func, unsigned int ms, TimerType type,void* _arg);
    template<typename Func>
    void   create (Func func, unsigned int ms, TimerType type,void* _arg);
    //终止一个定时器
	void   stop ();
private:
	//执行
	void on_timer(unsigned long long now);
private:
	friend class TimerManager;
	TimerManager& manager_;
	//调用函数，包括仿函数
	std::function<void(void*)> m_timerfunc;
	void* arg;
	TimerType timerType_;
	//间隔
	unsigned int m_nInterval;
	//过期
	unsigned long long  m_nExpires;
	int  m_nHeapIndex;
};
 
class TimerManager
{
public:
	//获取当前毫秒数
	static unsigned long long get_current_millisecs();
	//探测执行
	void detect_timers();
	void reset_timer(Timer*,int);

	std::mutex mu;
	std::condition_variable cv;
 
private:
	friend class Timer;
	//添加一个定时器
	void add_timer(Timer* timer);

	//移除一个定时器
	void remove_timer(Timer* timer);
	//定时上浮
	void up_heap(size_t index);
	//定时下沉
	void down_heap(size_t index);
	//交换两个timer索引
	void swap_heap(size_t index1, size_t index2);
private:
	struct HeapEntry
	{
		unsigned long long time;
		Timer* timer;
	};
	std::vector<HeapEntry> heap_;
};
 
template <typename Func>
inline void  Timer::start(Func fun, unsigned int interval, TimerType timetpe,void* _arg)
{
	m_nInterval = interval;
	m_timerfunc = fun;
	arg = _arg;
	m_nExpires = interval + TimerManager::get_current_millisecs();
	manager_.add_timer(this);
	timerType_= timetpe;
}

template <typename Func>
inline void  Timer::create(Func fun, unsigned int interval, TimerType timetpe,void* _arg)
{
	m_nInterval = interval;
	m_timerfunc = fun;
	arg = _arg;
	m_nExpires = interval + TimerManager::get_current_millisecs();
	timerType_= timetpe;
}
#endif
