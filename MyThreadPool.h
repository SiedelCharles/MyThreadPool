#pragma once

#include <queue>
#include <future>
#include <thread>
#include <vector>
#include <iostream>

class MyThreadPool {
public:
    // 这种方式是C++11之后单例化方法
    static MyThreadPool& get_instance() {
        static MyThreadPool inst{}; // 默认初始化
        return inst;
    }
    ~MyThreadPool() {
        stop();
    }

    template <class F, class... Args>
    auto commit(F&& f, Args&&... args) ->
        std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
        using RetType = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        if (_Stop.load())
            return std::future<RetType>{};
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<RetType> ret = task->get_future();
        {
            std::lock_guard<std::mutex> mtx(_mtx);
            _Tasks.emplace([task] {(*task)(); });
        }
        _cv.notify_one();
        return ret;
    }

private:
    using Task = std::packaged_task<void()>; // 任务类型,需要任务的多样性

    MyThreadPool(unsigned int init_num = std::thread::hardware_concurrency()/*C++11时被引入,返回实现支持的并发线程数量*/)
        :_Idle_Threads(init_num<2?2:init_num /*禁止初始线程数被设置为0或1, TODO: 如果init_num使用被默认初始化为0或者1怎么办,如果init_num被初始化为一个超过
        std::thread::hardware_concurrency()的值该怎么办, 如果init_num使用被默认初始化为std::thread::hardware_concurrency()而此时一些线程资源被其他对象占用该怎么办*/)
        , _Stop(false){
        start();
    }
    // 启动线程并且将线程放入vector中管理
    void start();
    void stop();
    // 删除拷贝构造和拷贝赋值,移动构造与移动赋值
    MyThreadPool(const MyThreadPool& other) = delete;
    MyThreadPool& operator=(const MyThreadPool& other) = delete;
    MyThreadPool(const MyThreadPool&& other) = delete;
    MyThreadPool& operator=(const MyThreadPool&& other) = delete;

    // 使用该原子变量表示空闲线程数量
    std::atomic<int> _Idle_Threads;
    // 使用该原子对象表示是否停止
    std::atomic<bool> _Stop;

    std::mutex _mtx;
    std::condition_variable _cv;

    std::vector<std::thread> _Pool;
    std::queue<Task> _Tasks; // TODO: 该queue是否需要线程安全,保证线程安全是否需要采用无锁设计

};

void MyThreadPool::start() {
    for (int i = 0; i < _Idle_Threads; ++i) {
        _Pool.emplace_back([this]() {
            while (!_Stop.load(std:: memory_order_seq_cst)) { // TODO:此处是否可改进?
                Task task;
                {
                    std::unique_lock<std::mutex> lk(_mtx);
                    _cv.wait(lk, [this](){
                        return !_Tasks.empty() || _Stop.load(std::memory_order_seq_cst);
                    });
                    if (_Stop.load(std::memory_order_seq_cst))
                        return;
                    task = std::move(_Tasks.front());
                    _Tasks.pop();
                }
                --_Idle_Threads;
                task();
                ++_Idle_Threads;
            }
            });
    }
}
void MyThreadPool::stop() {
    _Stop.store(true, std::memory_order_seq_cst);
    _cv.notify_all();
    for (auto& t : _Pool) {
        if (t.joinable()) {
            std::cout << "Thread " << t.get_id() << std::endl;
            t.join();
        }
    }
}