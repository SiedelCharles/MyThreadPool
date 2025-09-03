#include "MyThreadPool.h"
#include "ThreadSafeQueue.h"
#include <list>
#include <iostream>

template<typename T>
std::list<T>pool_thread_quick_sort(std::list<T> input) {
    if (input.empty())
    {
        return input;
    }
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());
    T const& partition_val = *result.begin();
    typename std::list<T>::iterator divide_point =
        std::partition(input.begin(), input.end(),
            [&](T const& val) {return val < partition_val; });
    std::list<T> new_lower_chunk;
    new_lower_chunk.splice(new_lower_chunk.end(),
        input, input.begin(),
        divide_point);
    std::future<std::list<T> > new_lower = MyThreadPool::get_instance().commit(pool_thread_quick_sort<T>, new_lower_chunk);
    std::list<T> new_higher(pool_thread_quick_sort(input));
    result.splice(result.end(), new_higher);
    result.splice(result.begin(), new_lower.get());
    return result;
}

void TestThreadPoolSort() {
    std::list<int> nlist = { 6,1,0,5,2,9,11 };
    auto sortlist = pool_thread_quick_sort<int>(nlist);
    for (auto& value : sortlist) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
}

//void TestDeque() {
//    BasicDeque<double> dq;
//    for (int i = 0; i < 12; i+=2) {
//        dq.push_front(i+0.2);
//    }
//    for (int i = 1; i < 12; i += 2) {
//        dq.push_back(i+0.1);
//    }
//    for (int i = 0; i < 12; ++i) {
//        std::cout << dq.front() << std::endl;
//        dq.pop_front();
//    }
//}

class TestClass {
public:
    TestClass(int x):_data(x){}
    ~TestClass(){}
private:
    int  _data;
};
//void TestThreadSafeQueLk() {
//    CircularQueLk<TestClass, 8> cq;
//    TestClass tc1(1);
//    TestClass tc2(2);
//    cq.push(tc1);
//    cq.push(std::move(tc2));
//
//    for (int i = 3; i <= 8; ++i) {
//        TestClass tc(i);
//        cq.emplace(tc);
//    }
//    cq.push(tc2);
//    for (size_t i = 0; i < 10; i++)
//    {
//        TestClass tc(0);
//        cq.pop(tc);
//    }
//}
//void TestThreadSafeQueLk1() {
//    BasicNolockQueue<TestClass, 8> cq;
//    TestClass tc1(1);
//    TestClass tc2(2);
//    cq.push(tc1);
//    cq.push(std::move(tc2));
//
//    for (int i = 3; i <= 8; ++i) {
//        TestClass tc(i);
//        cq.emplace(tc);
//    }
//    cq.push(tc2);
//    for (size_t i = 0; i < 10; i++)
//    {
//        TestClass tc(0);
//        cq.pop(tc);
//    }
//}
int main() {
    // TestThreadPoolSort();
    //TestDeque();
    //TestThreadSafeQueLk1();
    return 0;
}