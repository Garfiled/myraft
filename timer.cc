#define _CRT_SECURE_NO_WARNINGS
#include "timer.h"
#ifdef _MSC_VER
# include <sys/timeb.h>
#else
# include <sys/time.h>
#endif
 

#include "logutils.h"

// Timer
Timer::Timer(TimerManager& manager): manager_(manager), m_nHeapIndex(-1)
{}
 
Timer::~Timer()
{
	stop();
}
 
void Timer::stop()
{
	manager_.mu.lock();
	if (m_nHeapIndex != -1)
	{
		manager_.remove_timer(this);
		m_nHeapIndex = -1;
	}
	manager_.mu.unlock();
}

void Timer::on_timer(unsigned long long now)
{
	if (timerType_ == TimerType::CIRCLE)
	{
		m_nExpires = m_nInterval + now;
		manager_.add_timer(this);
	}
	else
	{
		m_nHeapIndex = -1;
	}
	m_timerfunc(arg);
}
 
// TimerManager
void TimerManager::add_timer(Timer* timer)
{
	this->mu.lock();
	//插到数组最后一个位置上，上浮
	timer->m_nHeapIndex = heap_.size();
	HeapEntry entry = { timer->m_nExpires, timer};
	heap_.push_back(entry);
	up_heap(heap_.size() - 1);
	this->mu.unlock();
	this->cv.notify_one();
}

void TimerManager::reset_timer(Timer* timer,int interval)
{
	this->mu.lock();
	if (timer->m_nHeapIndex != -1)
		remove_timer(timer);

	timer->m_nExpires = TimerManager::get_current_millisecs()+interval;
	timer->m_nInterval =interval;
	
	timer->m_nHeapIndex = heap_.size();
	HeapEntry entry = { timer->m_nExpires, timer};
	heap_.push_back(entry);
	up_heap(heap_.size() - 1);
	this->mu.unlock();
	this->cv.notify_one();
}

 
void TimerManager::remove_timer(Timer* timer)
{
	//头元素用数组未元素替换，然后下沉
	size_t index = timer->m_nHeapIndex;
	if (!heap_.empty() && index < heap_.size())
	{
		if (index == heap_.size() - 1) //only one timer
		{
			heap_.pop_back();
		}
		else  //more than one
		{
			swap_heap(index, heap_.size() - 1);
			heap_.pop_back();
 
			size_t parent = (index - 1) / 2;
			if (index > 0 && heap_[index].time < heap_[parent].time)
				up_heap(index);
			else
				down_heap(index);
		}
	}
}
 
void TimerManager::detect_timers()
{
	int delay = 0;
	while (1) 
	{
		Timer* timer;
		unsigned long long now;
		{
			std::unique_lock<std::mutex> lk(this->mu);
			if (this->heap_.empty())
			{
				this->cv.wait(lk);
				continue;
			}

			now = get_current_millisecs();

			delay = heap_[0].time-now; 

			if (delay>0) {
				this->cv.wait_for(lk,std::chrono::milliseconds(delay));
				continue;
			}

			timer = heap_[0].timer;
			remove_timer(timer);
		}

		this->cv.notify_one();

		// 应该交给worker处理
		timer->on_timer(now);
	}
}
 
void TimerManager::up_heap(size_t index)
{
	//下至上，和父节点比较。如果小于父节点上浮
	size_t parent = (index - 1) / 2;
	while (index > 0 && heap_[index].time < heap_[parent].time)
	{
		swap_heap(index, parent);
		index = parent;
		parent = (index - 1) / 2;
	}
}
 
void TimerManager::down_heap(size_t index)
{
	//从上到下，算出左右子节点，和最小的交换
	size_t lchild = index * 2 + 1;
	while (lchild < heap_.size())
	{
		size_t minChild = (lchild + 1 == heap_.size() || heap_[lchild].time < heap_[lchild + 1].time) ? lchild : lchild + 1;
		if (heap_[index].time < heap_[minChild].time)
			break;
		swap_heap(index, minChild);
		index = minChild;
		lchild = index * 2 + 1;
	}
}
 
void TimerManager::swap_heap(size_t index1, size_t index2)
{
	HeapEntry tmp = heap_[index1];
	heap_[index1] = heap_[index2];
	heap_[index2] = tmp;
	heap_[index1].timer->m_nHeapIndex = index1;
	heap_[index2].timer->m_nHeapIndex = index2;
}
 
unsigned long long TimerManager::get_current_millisecs()
{
#ifdef _MSC_VER
	_timeb timebuffer;
	_ftime(&timebuffer);
	unsigned long long ret = timebuffer.time;
	ret = ret * 1000 + timebuffer.millitm;
	return ret;
#else
	timeval tv;
	::gettimeofday(&tv, 0);
	unsigned long long ret = tv.tv_sec;
	return ret * 1000 + tv.tv_usec / 1000;
#endif
}
