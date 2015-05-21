#include <iostream>
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>

class Variant {
  // Simple variant class
public:
  
  Variant() : tag(Tag::t_none) {};
  Variant(const int as_int) : tag(Tag::t_int), as_int(as_int) {};
  Variant(const double as_double) : tag(Tag::t_double), as_double(as_double) {};
    
  // delete copy and move constructors to make sure we always construct in-place
  //Variant(const Variant&) = delete;
  //Variant& operator=(const Variant&) = delete;
  //Variant(Variant&&) = delete;
  //Variant& operator=(Variant&&) = delete;

  friend std::ostream& operator<<(std::ostream &o, const Variant &v) {
    switch (v.tag) {
    case Tag::t_none: break;
    case Tag::t_int: o << v.as_int; break;
    case Tag::t_double: o << v.as_double; break;
    }
    return o;
  }

private:

  enum class Tag {t_none, t_int, t_double} tag;
  union {
    int as_int;
    double as_double;
  };

};

template <size_t N>
class Message {
  // Holds a single log message
public:

  Message() : s(nullptr), argc(0) {}
  template<typename... Args>
  Message(const char *s, Args&&... args) : s(s) {
    static_assert(sizeof...(Args) <= std::extent<decltype(argv)>::value, 
		  "too many arguments");
    Pack<0, Args...>(std::forward<Args>(args)...);
  }

  friend std::ostream& operator<<(std::ostream &o, const Message &m) {
    o << m.s << " ";
    for (int i = 0; i < m.argc; ++i) {
      o << m.argv[i] << " ";
    }
    return o;
  }

private:

  template<int I, typename T, typename... Args>
  void Pack(T&& t, Args&&... args) {
    new(&argv[I]) Variant(std::forward<T>(t));
    Pack<I+1, Args...>(std::forward<Args>(args)...);
  }

  template<int I>
  void Pack() { 
    argc=I; 
  };

  const char *s;
  int argc;
  Variant argv[N];

};

template<typename T>
class Queue {
  // Ringbuffer, fixed size single producer single consumer lock-free queue
public:

  Queue(const size_t size) : head_(0), tail_(0) {
    q.resize(size);
  }

  bool empty() {
    return head_ == tail_;
  }

  template<typename... Args>
  void emplace(Args&&... args) {
    auto const head = head_.load(std::memory_order_relaxed);
    auto const next_head = (head + 1) % q.size();
    while (next_head == tail_.load(std::memory_order_acquire));
    new (&q[head]) T(std::forward<Args>(args)...);
    head_.store(next_head, std::memory_order_release);
  }

  T pop() { 
    auto tail = tail_.load(std::memory_order_relaxed);
    while (head_.load(std::memory_order_acquire) == tail);
    auto ret = q[tail];
    auto next_tail = (tail + 1) % q.size();
    tail_.store(next_tail, std::memory_order_release);
    return ret;
  }

private:

  std::vector<T> q;
  std::atomic<size_t> head_, tail_;

};

thread_local std::shared_ptr<Queue<Message<5>>> queue;

class Logger {
public:

  Logger()  {}

  template<typename... Args>
  void Log(const char* s, Args&&... args) {
    if (queue == nullptr) {
      std::cout << "creating thread local queue\n";
      queue = std::make_shared<Queue<Message<5>>>(200);
      std::lock_guard<std::mutex> lock(mutex);
      queues.push_back(queue);
    }
    queue->emplace(s, args...);
  };

  void Print() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto q : queues) {
      while (!q->empty()) {
	std::cout << q->pop() << "\n";
      }
    }
  }
  
private:
  
  std::mutex mutex;
  std::vector<std::shared_ptr<Queue<Message<5>>>> queues;
  
};



int main(int argc, char *argv[]) {

  Variant v1(0.1);
  Variant v2(1);

  std::cout << v1 << " " << v2 << "\n";

  Message<5> m("test", 0.1, 1, 2, 3.5, 6.9);
  //Message m2("test", 0.1, 1, 2, 3.5, 6.9, 7);

  std::cout << m << "\n";

  Logger l;
  l.Log("test2", 0.1);
  l.Log("test3", 0.9, 5);
  l.Log("test3", 0.9, 5);

  l.Print();
  return 0;
}
