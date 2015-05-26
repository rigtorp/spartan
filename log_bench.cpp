#include <thread>
#include "log.hpp"

int main(int argc, char *argv[]) {

  Logger::SetOutput("");

  int nthreads = 2;
  std::vector<std::thread> threads;

  for (int i = 0; i < nthreads; ++i) {
    threads.emplace_back([] {
	for (int i = 0; i < 5; ++i) {
	  Logger::Log("test", i, i*i);
	}
      });
  }
  for (auto& t : threads) {
    t.join();
  }

  return 0;
}
