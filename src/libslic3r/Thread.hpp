#ifndef GUI_THREAD_HPP
#define GUI_THREAD_HPP

#include <utility>
#include <string>
#include <thread>
#include <boost/thread.hpp>

#include <tbb/task_scheduler_observer.h>
#include <tbb/enumerable_thread_specific.h>

namespace Slic3r {

// Set / get thread name.
// Returns false if the API is not supported.
//
// It is a good idea to name the main thread before spawning children threads, because dynamic linking is used on Windows 10
// to initialize Get/SetThreadDescription functions, which is not thread safe.
//
// pthread_setname_np supports maximum 15 character thread names! (16th character is the null terminator)
// 
// Methods taking the thread as an argument are not supported by OSX.
// Naming threads is only supported on newer Windows 10.

bool set_thread_name(std::thread &thread, const char *thread_name);
inline bool set_thread_name(std::thread &thread, const std::string &thread_name) { return set_thread_name(thread, thread_name.c_str()); }
bool set_thread_name(boost::thread &thread, const char *thread_name);
inline bool set_thread_name(boost::thread &thread, const std::string &thread_name) { return set_thread_name(thread, thread_name.c_str()); }
bool set_current_thread_name(const char *thread_name);
inline bool set_current_thread_name(const std::string &thread_name) { return set_current_thread_name(thread_name.c_str()); }

// OSX specific: Set Quality of Service to "user initiated", so that the threads will be scheduled to high performance
// cores if available.
void set_current_thread_qos();

// Returns nullopt if not supported.
// Not supported by OSX.
// Naming threads is only supported on newer Windows 10.
std::optional<std::string> get_current_thread_name();

// To be called somewhere before the TBB threads are spinned for the first time, to
// give them names recognizible in the debugger.
// Also it sets locale of the worker threads to "C" for the G-code generator to produce "." as a decimal separator.
void name_tbb_thread_pool_threads_set_locale();

template<class Fn>
inline boost::thread create_thread(boost::thread::attributes &attrs, Fn &&fn)
{
    // Duplicating the stack allocation size of Thread Building Block worker
    // threads of the thread pool: allocate 4MB on a 64bit system, allocate 2MB
    // on a 32bit system by default.
    
    attrs.set_stack_size((sizeof(void*) == 4) ? (2048 * 1024) : (4096 * 1024));
    return boost::thread{attrs, std::forward<Fn>(fn)};
}

template<class Fn> inline boost::thread create_thread(Fn &&fn)
{
    boost::thread::attributes attrs;
    return create_thread(attrs, std::forward<Fn>(fn));    
}

// For unknown reasons and in sporadic cases when GCode export is processing, some participating thread
// in tbb::parallel_pipeline has not set locales to "C", probably because this thread is newly spawned.
// So in this class method on_scheduler_entry is called for every thread before it starts participating
// in tbb::parallel_pipeline to ensure that locales are set correctly
//
// For tbb::parallel_pipeline, it seems that on_scheduler_entry is called for every layer and every filter.
// We ensure using thread-local storage that locales will be set to "C" just once for any participating thread.
class TBBLocalesSetter : public tbb::task_scheduler_observer
{
public:
    TBBLocalesSetter() { this->observe(true); }
    ~TBBLocalesSetter() override { this->observe(false); };

    void on_scheduler_entry(bool is_worker) override;

private:
    tbb::enumerable_thread_specific<bool, tbb::cache_aligned_allocator<bool>, tbb::ets_key_usage_type::ets_key_per_instance> m_is_locales_sets{ false };
};

}

#endif // GUI_THREAD_HPP
