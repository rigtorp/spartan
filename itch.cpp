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

  timeval start, stop;
  gettimeofday(&start, NULL);
  parser.ReadMany(std::cin);
  gettimeofday(&stop, NULL);
  std::cout << (stop.tv_sec - start.tv_sec) * 1e6 / (double) handler.count << std::endl;

  return 0;
}
