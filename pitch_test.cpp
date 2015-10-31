/*
Copyright (c) 2015 Erik Rigtorp <erik@rigtorp.se>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include "pitch.hpp"
#include "feed.hpp"

struct Handler {
  void OnQuote(OrderBook *book, bool top) { bp = book->GetBestPrice(); }

  void OnTrade(OrderBook *book, int64_t shares, int64_t price, bool top) {
    lastq = shares;
    lastp = price;
    bp = book->GetBestPrice();
  }

  BestPrice bp;
  int lastq = 0;
  int lastp = 0;
};

int main(int argc, char *argv[]) {

  {
    // Test add order messages
    Handler handler;
    Feed<Handler> feed(handler, 100, false, false);
    PitchParser<Feed<Handler>> parser(feed);
    feed.Subscribe("A");
    char addl[] = {34,  0x21, 0,   0,   0, 0, 1, 0,   0,   0,   0,   0,
                   0,   0,    'B', 100, 0, 0, 0, 'A', ' ', ' ', ' ', ' ',
                   ' ', 1,    0,   0,   0, 0, 0, 0,   0,   0};
    parser.ParseMessage(0, addl);
    char adds[] = {26, 0x22, 0,   0, 0,   0,   2,   0,   0,   0,   0, 0, 0,
                   0,  'S',  100, 0, 'A', ' ', ' ', ' ', ' ', ' ', 1, 0, 0};
    parser.ParseMessage(0, adds);
    assert(handler.bp.bid == 1);
    assert(handler.bp.ask == 100);
    char adde[] = {40,  0x2F, 0,   0,   0,   0,   3,   0,   0,   0,
                   0,   0,    0,   0,   'B', 100, 0,   0,   0,   'A',
                   ' ', ' ',  ' ', ' ', ' ', ' ', ' ', 10,  0,   0,
                   0,   0,    0,   0,   0,   0,   ' ', ' ', ' ', ' '};
    parser.ParseMessage(0, adde);
    assert(handler.bp.bid == 10);
    assert(handler.bp.ask == 100);
    char addl2[] = {34,  0x21, 0,   0,   0, 0, 4, 0,   0,   0,   0,   0,
                    0,   0,    'S', 100, 0, 0, 0, 'A', ' ', ' ', ' ', ' ',
                    ' ', 50,   0,   0,   0, 0, 0, 0,   0,   0};
    parser.ParseMessage(0, addl2);
    assert(handler.bp.ask == 50);
  }

  {
    // Test exec order messages
    Handler handler;
    Feed<Handler> feed(handler, 100, false, false);
    PitchParser<Feed<Handler>> parser(feed);
    feed.Subscribe("A");
    char addl[] = {34,  0x21, 0,   0,   0, 0, 1, 0,   0,   0,   0,   0,
                   0,   0,    'B', 100, 0, 0, 0, 'A', ' ', ' ', ' ', ' ',
                   ' ', 1,    0,   0,   0, 0, 0, 0,   0,   0};
    parser.ParseMessage(0, addl);
    char adds[] = {26, 0x22, 0,   0, 0,   0,   2,   0,   0,   0,   0, 0, 0,
                   0,  'S',  100, 0, 'A', ' ', ' ', ' ', ' ', ' ', 1, 0, 0};
    parser.ParseMessage(0, adds);
    char exec1[] = {26, 0x23, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
                    0,  50,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    parser.ParseMessage(0, exec1);
    assert(handler.lastq == 50);
    assert(handler.lastp == 1);
    assert(handler.bp.bid == 1);
    assert(handler.bp.bidqty == 50);
    char exec2[] = {26, 0x23, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0,
                    0,  50,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    parser.ParseMessage(0, exec2);
    assert(handler.lastq == 50);
    assert(handler.lastp == 100);
    assert(handler.bp.ask == 100);
    assert(handler.bp.askqty == 50);
    char execps[] = {38, 0x24, 0, 0, 0, 0,  1, 0, 0, 0, 0, 0, 0,
                     0,  25,   0, 0, 0, 25, 0, 0, 0, 0, 0, 0, 0,
                     0,  0,    0, 0, 2, 0,  0, 0, 0, 0, 0, 0};
    parser.ParseMessage(0, execps);
    assert(handler.lastq == 25);
    assert(handler.lastp == 2);
    assert(handler.bp.bid == 1);
    assert(handler.bp.bidqty == 25);
    char execps2[] = {38, 0x24, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
                      0,  50,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      0,  0,    0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
    parser.ParseMessage(0, execps2);
    assert(handler.lastq == 50);
    assert(handler.lastp == 1);
    assert(handler.bp.bid == 0);
    assert(handler.bp.bidqty == 0);
  }

  {
    // Test reduce order messages
    Handler handler;
    Feed<Handler> feed(handler, 100, false, false);
    PitchParser<Feed<Handler>> parser(feed);
    feed.Subscribe("A");
    char addl[] = {34,  0x21, 0,   0,   0, 0, 1, 0,   0,   0,   0,   0,
                   0,   0,    'B', 100, 0, 0, 0, 'A', ' ', ' ', ' ', ' ',
                   ' ', 1,    0,   0,   0, 0, 0, 0,   0,   0};
    parser.ParseMessage(0, addl);
    char adds[] = {26, 0x22, 0,   0, 0,   0,   2,   0,   0,   0,   0, 0, 0,
                   0,  'S',  100, 0, 'A', ' ', ' ', ' ', ' ', ' ', 1, 0, 0};
    parser.ParseMessage(0, adds);
    assert(handler.bp.bidqty == 100);
    assert(handler.bp.askqty == 100);
    char redl[] = {18, 0x25, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 50, 0, 0, 0};
    parser.ParseMessage(0, redl);
    assert(handler.bp.bidqty == 50);
    char reds[] = {16, 0x26, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 100, 0};
    parser.ParseMessage(0, reds);
    assert(handler.bp.bidqty == 50);
    assert(handler.bp.askqty == 0);
  }

  {
    // Test modify order messages
    Handler handler;
    Feed<Handler> feed(handler, 100, false, false);
    PitchParser<Feed<Handler>> parser(feed);
    feed.Subscribe("A");
    char addl[] = {34,  0x21, 0,   0,   0, 0, 1, 0,   0,   0,   0,   0,
                   0,   0,    'B', 100, 0, 0, 0, 'A', ' ', ' ', ' ', ' ',
                   ' ', 1,    0,   0,   0, 0, 0, 0,   0,   0};
    parser.ParseMessage(0, addl);
    char adds[] = {26, 0x22, 0,   0, 0,   0,   2,   0,   0,   0,   0, 0, 0,
                   0,  'S',  100, 0, 'A', ' ', ' ', ' ', ' ', ' ', 1, 0, 0};
    parser.ParseMessage(0, adds);
    assert(handler.bp.bidqty == 100);
    assert(handler.bp.askqty == 100);
    char modl[] = {27, 0x27, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                   50, 0,    0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
    parser.ParseMessage(0, modl);
    assert(handler.bp.bid == 256);
    assert(handler.bp.bidqty == 50);
    char mods[] = {19, 0x28, 0, 0, 0,   0, 1, 0, 0, 0,
                   0,  0,    0, 0, 100, 0, 1, 0, 0};
    parser.ParseMessage(0, mods);
    assert(handler.bp.bid == 100);
    assert(handler.bp.bidqty == 100);
    char mods2[] = {19, 0x28, 0, 0, 0, 0, 1, 0, 0, 0,
                    0,  0,    0, 0, 0, 0, 1, 0, 0};
    parser.ParseMessage(0, mods2);
    assert(handler.bp.bid == 0);
    assert(handler.bp.bidqty == 0);
  }

  {
    // Test delete order messages
    Handler handler;
    Feed<Handler> feed(handler, 100, false, false);
    PitchParser<Feed<Handler>> parser(feed);
    feed.Subscribe("A");
    char addl[] = {34,  0x21, 0,   0,   0, 0, 1, 0,   0,   0,   0,   0,
                   0,   0,    'B', 100, 0, 0, 0, 'A', ' ', ' ', ' ', ' ',
                   ' ', 1,    0,   0,   0, 0, 0, 0,   0,   0};
    parser.ParseMessage(0, addl);
    char adds[] = {26, 0x22, 0,   0, 0,   0,   2,   0,   0,   0,   0, 0, 0,
                   0,  'S',  100, 0, 'A', ' ', ' ', ' ', ' ', ' ', 1, 0, 0};
    parser.ParseMessage(0, adds);
    assert(handler.bp.bidqty == 100);
    assert(handler.bp.askqty == 100);
    char del[] = {14, 0x29, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
    parser.ParseMessage(0, del);
    assert(handler.bp.bidqty == 0);
    assert(handler.bp.askqty == 100);
  }

  {
    // Test trade messages
    Handler handler;
    Feed<Handler> feed(handler, 100, false, false);
    PitchParser<Feed<Handler>> parser(feed);
    feed.Subscribe("A");
    char tradel[] = {41,  0x2A, 0, 0, 0, 0,   1,   1,   1,   1,   1,   1, 1, 1,
                     'B', 1,    0, 0, 0, 'A', ' ', ' ', ' ', ' ', ' ', 1, 0, 0,
                     0,   0,    0, 0, 0, 1,   0,   0,   0,   0,   0,   0, 0};
    handler.lastq = 0;
    handler.lastp = 0;
    parser.ParseMessage(0, tradel);
    assert(handler.lastq == 1);
    assert(handler.lastp == 1);
    char trades[] = {33,  0x2B, 0, 0,   0, 0, 1,   1,   1,   1,   1,
                     1,   1,    1, 'B', 1, 0, 'A', ' ', ' ', ' ', ' ',
                     ' ', 1,    0, 1,   0, 0, 0,   0,   0,   0,   0};
    handler.lastq = 0;
    handler.lastp = 0;
    parser.ParseMessage(0, trades);
    assert(handler.lastq == 1);
    assert(handler.lastp == 100);
    char tradee[] = {43,  0x30, 0,   0,   0,   0, 1, 1, 1,   1,   1,
                     1,   1,    1,   'B', 1,   0, 0, 0, 'A', ' ', ' ',
                     ' ', ' ',  ' ', ' ', ' ', 1, 0, 0, 0,   0,   0,
                     0,   0,    1,   0,   0,   0, 0, 0, 0,   0};
    handler.lastq = 0;
    handler.lastp = 0;
    parser.ParseMessage(0, tradee);
    assert(handler.lastq == 1);
    assert(handler.lastp == 1);
  }

  return 0;
}
