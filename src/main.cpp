#include <iostream>
#include <thread>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "size_map.h"
#include "system_alloc.h"
#include "singleton.h"
#include "page_heap.h"
#include "central_cache.h"
#include "thread_cache.h"

void test_system() {
    auto& sa = Singleton<SystemAlloc>::get_instance();
    size_t pre = 0;
    for(int i = 0; i < 513; ++i) {
        auto [ptr, actual] = sa.alloc(100, 100);
        std::cout << ptr << ": " << actual << std::endl;
        std::cout << pre - reinterpret_cast<size_t>(ptr) << std::endl;
        pre = reinterpret_cast<size_t>(ptr);

        sa.dealloc(ptr, actual);
    }
}

void test_page_heap() {
    auto& ph = SinglePageHeap::get_instance();
    std::vector<Span*> spans(10);
    for(int i = 1; i < 10; ++i) {
        spans[i] = ph.alloc(i);
        std::cout << spans[i]->start_addr() << ": " << spans[i]->num_pages() << std::endl;
    }

    for(int i = 1; i < 10; ++i) {
        ph.dealloc(spans[i]);
    }
}

void test_central_cache() {
    auto& cc = SingleCentralCahce::get_instance();
    const int m = 3000;
    for(int i = 1; i < 85; ++i) {
        void* batch[m] = {nullptr};
        cc.alloc(i, batch, m);
        cc.dealloc(i, batch, m);
    }
}

void test_thread_cache() {
    auto tc = ThreadCache::tc;
    std::cout << tc.get() << std::endl;
    const int m = 3000;
    void* objects[m];
    for(int i = 1; i < m; ++i) {
        objects[i] = tc->alloc(i % 84 + 1);
    }
    for(int i = 1; i < m; ++i) {
        tc->dealloc(i % 84 + 1, objects[i]);
    }
}

int main() {
    spdlog::set_default_logger(spdlog::stdout_color_mt("memory pool"));
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S][thread %t][%^%-5l%$] %v");

    // test_system();
    // test_page_heap();
    // test_central_cache();
    // test_thread_cache();

    std::thread t1(test_thread_cache);


    std::thread t2(test_thread_cache);
    t1.join();
    t2.join();

    SingleCentralCahce::destroy();

    SinglePageHeap::destroy();

    SingleSystemAlloc::destroy();


    return 0;
}
