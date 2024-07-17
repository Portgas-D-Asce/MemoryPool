#ifndef MEMORYPOOL_SINGLETON_H
#define MEMORYPOOL_SINGLETON_H
#include <mutex>
#include <memory>

template<typename T>
class Singleton {
public:
    static T& get_instance() {
        static std::once_flag flag;
        std::call_once(flag, [&]() {
            _instance = std::unique_ptr<T>(new T());
        });
        return *_instance;
    }

    static void destroy() {
        static std::once_flag flag;
        std::call_once(flag, [&]() {
            _instance.reset(nullptr);
        });
    }
private:
    static std::unique_ptr<T> _instance;
};

template<typename T>
std::unique_ptr<T> Singleton<T>::_instance = nullptr;

#endif //MEMORYPOOL_SINGLETON_H
