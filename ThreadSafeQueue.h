#pragma once
#include <stdexcept>
#include <iostream>
#include <memory>
#include <mutex>

//template<typename T>
//class BasicDeque {// 增长方向为顺时针
//public:
//    BasicDeque(size_t init_capacity = 12/*注:init_capacity=0是合理的*/) :_capacity(init_capacity), _count(0), _front_idx(0), _back_idx(0) {
//        _buffer = new T[_capacity]();
//    }
//    ~BasicDeque() {
//        delete[] _buffer;
//    }
//    bool empty() {
//        return _count==0;
//    }
//    T& front() {
//        if (empty()) {
//            throw std::out_of_range("Out of range");
//        }
//        return _buffer[(_front_idx+1)%_capacity];
//    }
//    T& back() {
//        if (empty()) {
//            throw std::out_of_range("Out of range");
//        }
//        return _buffer[_back_idx];
//    }
//    void push_front(const T& value) {
//        if (_count == _capacity) {
//            resize(2 * _capacity);
//        }
//        // _front_idx指向一个可插入的节点
//        _buffer[_front_idx] = value;
//        _front_idx = _front_idx > 0 ? _front_idx - 1 : _front_idx + _capacity - 1;
//        ++_count;
//    }
//    void push_back(const T& value) {
//        if (_count == _capacity) {
//            resize(2 * _capacity);
//        }
//        // _back_front指向一个有数据的节点
//        _back_idx = (_back_idx + 1) % _capacity;
//        _buffer[_back_idx] = value;
//        ++_count;
//    }
//    void pop_front() {
//        if (empty()) {
//            throw std::out_of_range("Out of range");
//        }
//        _front_idx = (_front_idx + 1) % _capacity;
//    }
//    void pop_back() {
//        if (empty()) {
//            throw std::out_of_range("Out of range");
//        }
//        _back_idx = _back_idx > 0 ? _back_idx - 1 : _back_idx + _capacity - 1;
//    }
//
//    
//private:
//    size_t _count;
//    size_t _capacity;
//    size_t _front_idx;
//    size_t _back_idx;
//    T* _buffer;
//
//    void resize(size_t new_capacity) {
//        auto new_buffer = new T[new_capacity]();
//        for (size_t i =0 ; i<_count  ;++i) {
//            new_buffer[i] = _buffer[(_front_idx + i) % _capacity];
//        }
//        delete _buffer;
//        _capacity = new_capacity;
//        _front_idx = 0;
//        _back_idx = _count;
//        _buffer = new_buffer;
//    }
//};

template<typename T, size_t Cap>
class CircularQueLk : private std::allocator<T> {
public:
    CircularQueLk(): _capacity(Cap+1), _data(std::allocator<T>::allocate(Cap)), _head(0), _tail(0){}
    CircularQueLk(const CircularQueLk& other) = delete;
    CircularQueLk& operator = (const CircularQueLk & other) = delete;
    CircularQueLk& operator = (const CircularQueLk & other) volatile= delete; // TODO:


    ~CircularQueLk() {
        std::lock_guard<std::mutex> lk(_mtx);
        while (_head!=_tail)
        {
            std::allocator<T>::destroy(_data + _head - 1);
            ++_head;
        }
        std::allocator<T>::deallocate(_data, _capacity);
    }
    template <typename... Args>
    bool emplace(Args&&...args) {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_head==(_tail+1)%_capacity) {
            // 此处应该记录一调信息
            std::cout << "Queue is fulled." << std::endl;
            return false;
        }
        std::allocator<T>::construct(_data + _tail, std::forward<Args>(args)...);
        _tail = (_tail + 1) % _capacity;
        return true;
    }

    bool push(const T& value) {// 有可能插入一个const类型的对象
        return emplace(value);
    }
    bool push(T&& value) {
        return emplace(std::move(value));
    }
    bool pop(T& value) {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_head == _tail) {
            std::cout << "Queue s empty." << std::endl;
            return false;
        }
        _head = (_head + 1) % _capacity;
        value = std::move(_data[_head]);
        return true;
    }
private:
    size_t _capacity;
    size_t _head;
    size_t _tail;
    T* _data;
    std::mutex _mtx;
};

template<typename T, size_t Cap>
class BasicNolockQueue :private std::allocator <T>{ // 自旋锁
public:
    BasicNolockQueue() : _capacity(Cap + 1), _data(std::allocator<T>::allocate(Cap))
        , _atomic(false), _head(0), _tail(0) {}
    BasicNolockQueue(const BasicNolockQueue& other) = delete;
    BasicNolockQueue& operator = (const BasicNolockQueue& other) = delete;
    BasicNolockQueue& operator = (const BasicNolockQueue& other) volatile = delete; // TODO:
    ~BasicNolockQueue() {
        while (_head != _tail)
        {
            std::allocator<T>::destroy(_data + _head - 1);
            ++_head;
        }
        std::allocator<T>::deallocate(_data, _capacity);
    }

    template<typename... Args>
    bool emplace(Args&&... args) {
        // 即_atomic只允许由false变为true
        bool use_expected = false;
        bool use_desired = true;
        do
        {
            use_expected = false;
            use_desired = true;
        } while (!_atomic.compare_exchange_strong(use_expected, use_desired));


        if (_head == (_tail + 1) % _capacity) {
            // 此处应该记录一调信息
            std::cout << "Queue is fulled." << std::endl;
            do
            {
                use_expected = true;
                use_desired = false;
            } while (!_atomic.compare_exchange_strong(use_expected, use_desired));
            return false;
        }
        std::allocator<T>::construct(_data + _tail, std::forward<Args>(args)...);
        _tail = (_tail + 1) % _capacity;
        do
        {
            use_expected = true;
            use_desired = false;
        } while (!_atomic.compare_exchange_strong(use_expected, use_desired));
        return true;
    }
    bool push(const T& value) {// 有可能插入一个const类型的对象
        return emplace(value);
    }
    bool push(T&& value) {
        return emplace(std::move(value));
    }

    bool pop(T& value) {
        bool use_expected = false;
        bool use_desired = true;
        do
        {
            use_expected = false;
            use_desired = true;
        } while (!_atomic.compare_exchange_strong(use_expected, use_desired));
        if (_head == _tail) {
            std::cout << "Queue s empty." << std::endl;
            do
            {
                use_expected = true;
                use_desired = false;
            } while (!_atomic.compare_exchange_strong(use_expected, use_desired));
            return false;
        }
        _head = (_head + 1) % _capacity;
        value = std::move(_data[_head]);
        do
        {
            use_expected = true;
            use_desired = false;
        } while (!_atomic.compare_exchange_strong(use_expected, use_desired));
        return true;
    }


private:
    size_t _capacity;
    size_t _head;
    size_t _tail;
    T* _data;
    std::atomic<bool> _atomic;
};

template<typename T, size_t Cap>
class NolockQueue :private std::allocator <T> { // 自旋锁
public:
    NolockQueue() : _capacity(Cap + 1), _data(std::allocator<T>::allocate(Cap))
        , _atomic(false), _head(0), _tail(0) {
    }
    NolockQueue(const NolockQueue& other) = delete;
    NolockQueue& operator = (const NolockQueue& other) = delete;
    NolockQueue& operator = (const NolockQueue& other) volatile = delete; // TODO:
    ~NolockQueue() {
        while (_head != _tail)
        {
            std::allocator<T>::destroy(_data + _head - 1);
            ++_head;
        }
        std::allocator<T>::deallocate(_data, _capacity);
    }

    template<typename... Args>
    bool push(const T& value) {// 有可能插入一个const类型的对象
        size_t t;
        do
        {
            t = _tail.load();
            if (_head.load() == (t + 1) % _apacity) {
                return false;
                //_data[t] = value;//1
            }
        } while (!_tail.compare_exchange_strong(t, (t+1)%_capacity);// 有危险,先写入且交换成功的线程写入的数据很有可能会被覆盖
        _data[t] = value;//2
        size_t tailup;
        do
        {
            tailup = t;
        } while (!_tail_updata.compare_exchange_strong(tailup, (tailup + 1) % _capacity);
        return true;
    }
    bool push(T&& value) {
        return emplace(std::move(value));
    }

    bool pop(T& value) {
        size_t h;
        do
        {
            h = _head.load();
            if (h == _tail_updata.load()) {
                return false;
            }
            value = _data[h];// 放弃了std::move;
        } while (!_head.compare_exchange_strong(h, (h+1)%_capacity));
        return true;
    }


private:
    size_t _capacity;
    std::aotimc<size_t> _head;
    std::aotimc<size_t> _tail;
    std::aotimc<size_t> _tail_updata;
    T* _data;
    std::atomic<bool> _atomic;
};

