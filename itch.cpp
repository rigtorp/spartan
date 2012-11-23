#define NDEBUG

#include <sys/time.h>
#include <iostream>

#include "itch.hpp"
#include "feed.hpp"

using namespace std;

class Handler
{
public:
  Handler()
    : count(0)
  {}

  void OnQuote(OrderBook *book, bool top)
  {
    count++;
    // if (top) {
    //  cout << book->GetInstrument() << " " << book->GetBestPrice() << endl;
    // }
    //std::cout << book->buy_.size() << " " << book->sell_.size() << std::endl;
  }

  void OnTrade(OrderBook *book, bool bs, int64_t shares, int64_t price, bool top)
  {
    OnQuote(book, top);
  }

  int count;
};

int main(int argc, char *argv[])
{
  Handler handler;
  Feed<Handler> feed(handler, true, true);
  Itch41Parser<Feed<Handler>> parser(feed);

  feed.Subscribe(argv[1]);

  std::vector<int> hist(10);
  std::vector<double> samples;

  for (int i = 0; i < 1000 && std::cin.good(); ++i) {
    parser.ReadMany(std::cin, 100);
  }

  while (std::cin.good()) {
    auto res = parser.ReadMany(std::cin, 100);
    hist[std::min((size_t)std::trunc(res*10), hist.size()-1)]++;
    samples.push_back(res);
  }

  std::sort(samples.begin(), samples.end());
  auto mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  auto max = samples.back();
  auto min = samples.front();
  auto p95 = samples[0.95 * samples.size()];

  std::cout << "Mean:   " << mean << std::endl;
  std::cout << "95%:    " << p95 << std::endl;
  std::cout << "Max:    " << max << std::endl;
  std::cout << "Min:    " << min << std::endl;
  for (size_t i = 0; i < hist.size(); ++i) {
    std::string s(std::trunc(100*(double)hist[i] / samples.size()), '*');
    std::cout << i << " " << s << std::endl;
  }

  return 0;
}
