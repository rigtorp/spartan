#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

class Formatter {
public:
  static void format(std::ostream &o) {
    o << "\n";
  }

  template<typename T, typename... Args>
  static void format(std::ostream &o, T&& t, Args&&... args) {
    o << t << " ";
    format(o, std::forward<Args>(args)...);
  }
};

class Message {
public:

  template<typename... Args>
  Message(Args&&... args) {
    fp = pack(&data, std::forward<Args>(args)...);
  }

  ~Message() {
    // make sure destructors are called
    fp(nullptr, &data);
  }
  
  void Format(std::ostream& o) {
    fp(&o, &data);
  }

private:

  Message(Message &other) = delete;
  Message& operator=(const Message&) = delete;
  Message(Message &&other) = delete;
  Message& operator=(Message&&) = delete;
  
  template<typename Tup, std::size_t... index>
  static void call_format_helper(std::ostream &o, Tup &&args, std::index_sequence<index...>) {
    Formatter::format(o, std::get<index>(std::forward<Tup>(args))...);
  }

  template<typename... Args>
  static void call_format(std::ostream *o, void *p) {
    typedef std::tuple<Args...> Tup;
    Tup &args = *reinterpret_cast<Tup*>(p);
    if (o) {
      call_format_helper(*o, args, std::index_sequence_for<Args...>{});
    } else {
      // important to call destructor for complex types
      args.~Tup();
    }
  }
  
  template<typename T, typename... Args>
  static decltype(auto) pack(T *p, Args&&... args) {
    typedef std::tuple<typename std::decay<Args>::type...> Tup;
    static_assert(alignof(Tup) <= alignof(T), "invalid alignment");
    static_assert(sizeof(Tup) <= sizeof(T), "storage too small");
    new (p) Tup(std::forward<Args>(args)...);
    return &call_format<typename std::decay<Args>::type...>;
  }

  using FormatFun = void (*)(std::ostream*, void*);
  using Storage = typename std::aligned_storage<56, 8>::type;

  FormatFun fp;
  Storage data;
};

static_assert(sizeof(Message) == 64, "message not a cache line in size");

template<typename T>
class Queue {
  // Ringbuffer, fixed size single producer single consumer lock-free queue
public:

  Queue(const size_t size)
    : size_(size),
      buffer_(static_cast<T*>(std::malloc(sizeof(T) * size))),
      head_(0), tail_(0)
  {
    if (!buffer_) {
      throw std::bad_alloc();
    }
  }

  ~Queue()
  {
    while (front()) {
      pop();
    }
    std::free(buffer_);
  }

  template<typename... Args>
  void emplace(Args&&... args) {
    auto const head = head_.load(std::memory_order_relaxed);
    auto const next_head = (head + 1) % size_;    
    while (next_head == tail_.load(std::memory_order_acquire));
    new (&buffer_[head]) T(std::forward<Args>(args)...);
    head_.store(next_head, std::memory_order_release);
  }

  T* front()
  {
    auto tail = tail_.load(std::memory_order_relaxed);
    if (head_.load(std::memory_order_acquire) == tail) {
      return nullptr;
    }
    return &buffer_[tail];
  }

  void pop()
  {
    auto const tail = tail_.load(std::memory_order_relaxed);
    if (head_.load(std::memory_order_acquire) == tail) {
      return;
    }
    auto const next_tail = (tail + 1) % size_;
    buffer_[tail].~T();
    tail_.store(next_tail, std::memory_order_release);
  }

private:
  
  const size_t size_;
  T* const buffer_;
  std::atomic<size_t> head_, tail_;

};


class Logger {
public:
  
  template<typename... Args>
  static void Log(const char* fmt, Args&&... args) {
    queue().emplace(fmt, std::forward<Args>(args)...);
  }

  static void SetQueueSize(const size_t size)
  {
    instance().queue_size_ = size;
  }

  static void SetOutput(const std::string& fname)
  {    
    Logger& logger = instance();
    std::lock_guard<std::mutex> lock(logger.mutex_);
    if (fname == "") {
      logger.ostream_.reset();
      logger.cout_ = false;
    } else if (fname == "-") {
      logger.ostream_.reset();
      logger.cout_ = true;
    } else {
      logger.ostream_ = std::make_unique<std::ofstream>(fname);
      logger.cout_ = false;
    }
  }
  
private:

  Logger()
    : active_(true),
      queues_size_(1),
      queue_size_(1024),
      cout_(true)      
  {
    thread_ = std::thread([this] { Writer(); });
  }

  ~Logger() {
    active_ = false;
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  Logger(Logger &other) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger &&other) = delete;
  Logger& operator=(Logger&&) = delete;
  
  void Writer() {
    using namespace std::chrono_literals;
    while (active_ || queues_size_) {
      {
	std::lock_guard<std::mutex> lock(instance().mutex_);
	for (auto it = queues_.begin(); it != queues_.end();) {
	  auto &q = *it;	    
	  while (q->front()) {
	    if (ostream_) {
	      q->front()->Format(*ostream_);
	    }
	    if (cout_) {
	      q->front()->Format(std::cout);
	    }
	    q->pop();
	  }
	  if (q.unique() && !q->front()) {
	    it = queues_.erase(it);
	  } else {
	    ++it;
	  }
	}
	queues_size_ = queues_.size();
      }
      //std::this_thread::sleep_for(100ms);
    }
  }
  
  static Logger& instance() {
    static Logger instance;
    return instance;
  }

  using QueueType = Queue<Message>;
  
  static QueueType& queue() {
    static thread_local std::shared_ptr<QueueType> queue;
    if (queue == nullptr) {
      queue = std::make_shared<QueueType>(instance().queue_size_);
      std::lock_guard<std::mutex> lock(instance().mutex_);
      instance().queues_.push_back(queue);
    }
    return *queue;
  }
  
  std::mutex mutex_;
  std::vector<std::shared_ptr<QueueType>> queues_;
  std::thread thread_;
  bool active_;
  size_t queues_size_;
  size_t queue_size_;
  bool cout_;
  std::unique_ptr<std::ostream> ostream_;
};
