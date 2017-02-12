#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <future>
#include <functional>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <iterator>

int numElements = 1000000;
int numThreads = 8;
bool found = false;
int tasks = 0;
std::mutex mtx;



void QuickSort(std::vector<int> &, int, int);
int partition(std::vector<int> &, int, int);




template<typename T>
class TSQ
{
private:
        std::mutex m;
        std::queue<T> q;

public:
	void enqueue(T t)
	{
		std::lock_guard<std::mutex> l(m);
		q.push(t);
		mtx.lock();
		++tasks;
		mtx.unlock();
	}

	bool try_dequeue(T &f)
	{
		std::lock_guard<std::mutex> l(m);
		if (q.empty())
			return false;
		auto res = q.front();
		f = res;
		q.pop();
		mtx.lock();
		--tasks;
		mtx.unlock();
		return true;
	}

	bool empty()
    {
        if (q.empty())
        {
            return true;
        }
        return false;
    }


};

class ThreadPool2 {
    
    using func = std::function<void(void)>;
    
public:
    ThreadPool2(int);
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>;
    
    ~ThreadPool2();
    
private:
    
    std::vector< std::thread > pool;
    TSQ<func> queue;
    

    std::mutex itemMutex;
    std::condition_variable condition;

    std::atomic<bool> shouldContinue;

};

// the constructor just launches some amount of workers
inline ThreadPool2::ThreadPool2(int threads)
:   shouldContinue(false)
{
    for(int i = 0;i<threads;++i)
        pool.emplace_back(
                             [this]
                             {
                                 for(;;)
                                 {
                                     std::function<void(void)> task;
                                     
                                     {
                                         std::unique_lock<std::mutex> lock(this->itemMutex);
                                         this->condition.wait(lock,
                                                              [this]{ return this->shouldContinue || !this->queue.empty(); });
                                         if(this->shouldContinue && this->queue.empty())
                                             return;
                                        // task = std::move(this->tasks.front());
                                         this->queue.try_dequeue(task);
                                     }
                                     task();
                                 }
                             }
                             );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool2::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared< std::packaged_task<return_type()> >(
                                                                      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                                                                      );
    
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(itemMutex);
        
        // don't allow enqueueing after stopping the pool
        if(shouldContinue)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        
        queue.enqueue([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool2::~ThreadPool2()
{
    {
        std::unique_lock<std::mutex> lock(itemMutex);
        shouldContinue =true;
    }
    condition.notify_all();
    for(std::thread &worker: pool)
        worker.join();
}

//generic template to time a function
template<typename F>
auto timeFunc(F f)
{
	auto start = std::chrono::steady_clock::now();
	f();
	auto end = std::chrono::steady_clock::now();
	return end - start;
}

//calculates and outputs the average and standard deviation
void averageStdDev(std::vector<std::chrono::duration<double>> list)
{
	int n = list.size();
	double sum = 0.0;
	for (int i = 0; i < n; i++)
	{
		sum += list[i].count();
	}
	double average = sum / n;
	std::cout << "Average: " << (sum / n) * 1000 << " milliseconds." << std::endl;
	double stdDev = 0.0;
	for (int j = 0; j < n; j++)
	{
		stdDev += pow(list[j].count() - average, 2);
	}
	std::cout << "Standard Deviation: " << sqrt(stdDev / n) * 1000 << " milliseconds." << std::endl;
}


ThreadPool2 quickSortPool(numThreads);

    
void QuickSort(std::vector<int> &intVect, int low, int high)
{
	if (low <= high)
	{
		int p = partition(intVect, low, high);
		std::function<void(void)> task1 = std::bind(QuickSort,intVect, low, p - 1);
		std::function<void(void)> task2 = std::bind(QuickSort, intVect, p + 1, high);
		quickSortPool.enqueue(task1);
		//quickSortPool.enqueue(task2);
		//QuickSort(intVect, low, p-1);
		QuickSort(intVect, p+1, high);
		//std::cout<<"tasks: "<<tasks<<std::endl;
	}
	else
		return;
}

int partition(std::vector<int> &intVect, int low, int high)
{
	int pivotPosition = (high + low) / 2;
	int pivot = intVect[pivotPosition];
	//std::cout<<pivot<<std::endl;
	int temp = intVect[low];
	intVect[low] = intVect[pivotPosition];
	intVect[pivotPosition] = temp;
	int i = low + 1;
	int j = high;
	while (i <= j)
	{
		while (intVect[i] < pivot && i <= j && i <= high)
			i++;
		while (intVect[j] > pivot && i <= j)
			j--;
		if (i <= j && intVect[i] > pivot && intVect[j] < pivot && j >= 0 )
		{
			int temp = intVect[i];
			intVect[i] = intVect[j];
			intVect[j] = temp;
			//std::cout<<"hi"<<std::endl;
		}
		else
		{
			//int temp = arrayPtr[low];
			intVect[low] = intVect[j];
			intVect[j] = pivot;
			//std::cout<<"hi"<<std::endl;
			//return j;
		}
	}
	return j;
}

void useStandardSort(std::vector<int> & intVect)
{
	std::sort(intVect.begin(), intVect.end());	

}

void useStandardFind(std::vector<int> intVect, int userInt, int begin, int end)
{
	
	auto value = std::find(intVect.begin()+begin, intVect.begin()+end, userInt);
	if (value != intVect.begin()+end)
	{
		found = true;
	//	std::cout<<"found"<<std::endl;
	}
		//found = true;
}

void threadedFind(std::vector<int> intVect, int userInt)
{
	ThreadPool2 tpool(numThreads);
	for (int i = 0; i < numThreads; i++)
	{
		std::function<void(void)> task = std::bind(useStandardFind, intVect, userInt, (intVect.size()/8) * i, (intVect.size()/8)*(i+1));
		tpool.enqueue(task);
	} 	

}


int main()
{
	
	
	std::vector<int> intVector;
        for (int i = 0; i < numElements; i++)
        {
               intVector.push_back(std::rand());
		//intVector.push_back(i+1);
        }
	std::random_shuffle(intVector.begin(), intVector.end());
	std::cout<<"A Vector holding "<<numElements<<" ints has been created. Enter number to search for."<<std::endl;
	int userInt;
	std::cin>>userInt;
	std::cout<<"Searching..."<<std::endl;
//	useStandardFind(intVector, userInt, 0, numElements);

	auto timeElapsed = timeFunc(std::bind(threadedFind, intVector, userInt));
	//auto timeElapsed = timeFunc(std::bind(useStandardFind,intVector, userInt, 0, numElements));
	//threadedFind(intVector, userInt);
	std::cout << "Time for search : " << std::chrono::duration_cast<std::chrono::microseconds>(timeElapsed).count() << " microseconds" << std::endl;

	if (found == true)
	{
		std::cout<<userInt<<" was found!"<<std::endl;
	}
	else
	{
		std::cout<<userInt<<" was not found."<<std::endl;
	}
	

	//std::cout<<"before sort"<<std::endl;
	//for (size_t i = 0; i < intVector.size(); i++)
	//{
        //        std::cout<<intVector[i]<<std::endl;
        //}
	//std::cout<<std::endl<<std::endl;
	std::cout<<"Sorting..."<<std::endl;
	
	//timeElapsed = timeFunc(std::bind(useStandardSort,intVector));
	timeElapsed = timeFunc(std::bind(QuickSort,intVector,0, numElements));
	std::cout << "Time for sort : " << std::chrono::duration_cast<std::chrono::microseconds>(timeElapsed).count() << " microseconds" << std::endl;

	//useStandardSort(intVector);
	//std::cout<<"after sort"<<std::endl;
	//for (size_t i = 0; i < intVector.size(); i++)
	//{
	//	std::cout<<intVector[i]<<std::endl;
	//}
	


return 0;
}
