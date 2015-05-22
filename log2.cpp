#include <iostream>
#include <tuple>
#include <utility>
#include <type_traits>
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
    fp = pack<Formatter>(&data, std::forward<Args>(args)...);
  }

  ~Message() {
    // make sure destructors are called
    fp(nullptr, &data);
  }
  
  void Format() {
    fp(&std::cout, &data);
  }

private:

  Message(Message &other) = delete;
  Message(Message &&other) = delete;
  
  template<typename Formatter, typename Tup, std::size_t... index>
  static void call_format_helper(std::ostream &o, Tup &&args, std::index_sequence<index...>) {
    Formatter::format(o, std::get<index>(std::forward<Tup>(args))...);
  }

  template<typename Formatter, typename... Args>
  static void call_format(std::ostream *o, void *p) {
    typedef std::tuple<Args...> Tup;
    Tup &args = *reinterpret_cast<Tup*>(p);
    if (o) {
      call_format_helper<Formatter>(*o, args, std::index_sequence_for<Args...>{});
    } else {
      // important to call destructor for complex types
      args.~Tup();
    }
  }
  
  template<typename Formatter, typename T, typename... Args>
  static decltype(auto) pack(T *p, Args&&... args) {
    typedef std::tuple<typename std::decay<Args>::type...> Tup;
    static_assert(alignof(Tup) <= alignof(T), "invalid alignment");
    static_assert(sizeof(Tup) <= sizeof(T), "storage too small");
    new (p) Tup(std::forward<Args>(args)...);
    return &call_format<Formatter, typename std::decay<Args>::type...>;
  }

  using FormatFun = void (*)(std::ostream*, void*);
  using Storage = typename std::aligned_storage<56, 8>::type;

  FormatFun fp;
  Storage data;
};

static_assert(sizeof(Message) == 64, "message not a cache line in size");

#include <queue>

class Logger {
public:
  
  template<typename... Args>
  void Log(const char* fmt, Args&&... args) {
    logs.emplace(fmt, std::forward<Args>(args)...);
  }

  void Print() {
    while (!logs.empty()) {
      logs.front().Format();
      logs.pop();
    }
  }
  
private:
  std::queue<Message> logs;
};


int main(int argc, char *argv[]) {

  Logger l;
  
  for (int i = 0; i < 100; ++i) {
    l.Log("test", i, i*i);
  }
  //l.Print();

  int a = 1;
  double b = 1.2;
  Message m("test", b, a, 0.0);
  b = 3.0;
  m.Format();
  m.Format();
  
  return 0;
}
