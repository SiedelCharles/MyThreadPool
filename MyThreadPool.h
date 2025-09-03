#pragma once

#include <queue>
#include <future>
#include <thread>
#include <vector>
#include <iostream>

class MyThreadPool {
public:
    // ���ַ�ʽ��C++11֮����������
    static MyThreadPool& get_instance() {
        static MyThreadPool inst{}; // Ĭ�ϳ�ʼ��
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
    using Task = std::packaged_task<void()>; // ��������,��Ҫ����Ķ�����

    MyThreadPool(unsigned int init_num = std::thread::hardware_concurrency()/*C++11ʱ������,����ʵ��֧�ֵĲ����߳�����*/)
        :_Idle_Threads(init_num<2?2:init_num /*��ֹ��ʼ�߳���������Ϊ0��1, TODO: ���init_numʹ�ñ�Ĭ�ϳ�ʼ��Ϊ0����1��ô��,���init_num����ʼ��Ϊһ������
        std::thread::hardware_concurrency()��ֵ����ô��, ���init_numʹ�ñ�Ĭ�ϳ�ʼ��Ϊstd::thread::hardware_concurrency()����ʱһЩ�߳���Դ����������ռ�ø���ô��*/)
        , _Stop(false){
        start();
    }
    // �����̲߳��ҽ��̷߳���vector�й���
    void start();
    void stop();
    // ɾ����������Ϳ�����ֵ,�ƶ��������ƶ���ֵ
    MyThreadPool(const MyThreadPool& other) = delete;
    MyThreadPool& operator=(const MyThreadPool& other) = delete;
    MyThreadPool(const MyThreadPool&& other) = delete;
    MyThreadPool& operator=(const MyThreadPool&& other) = delete;

    // ʹ�ø�ԭ�ӱ�����ʾ�����߳�����
    std::atomic<int> _Idle_Threads;
    // ʹ�ø�ԭ�Ӷ����ʾ�Ƿ�ֹͣ
    std::atomic<bool> _Stop;

    std::mutex _mtx;
    std::condition_variable _cv;

    std::vector<std::thread> _Pool;
    std::queue<Task> _Tasks; // TODO: ��queue�Ƿ���Ҫ�̰߳�ȫ,��֤�̰߳�ȫ�Ƿ���Ҫ�����������

};

void MyThreadPool::start() {
    for (int i = 0; i < _Idle_Threads; ++i) {
        _Pool.emplace_back([this]() {
            while (!_Stop.load(std:: memory_order_seq_cst)) { // TODO:�˴��Ƿ�ɸĽ�?
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