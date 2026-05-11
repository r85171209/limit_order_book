#include <optional>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "lob.cpp"

TEST_CASE("empty book has no best bid, ask, spread, or mid") {
  LimitOrderBook book;
  CHECK(book.get_best_bid() == std::nullopt);
  CHECK(book.get_best_ask() == std::nullopt);
  CHECK(book.spread() == std::nullopt);
  CHECK(book.mid_price() == std::nullopt);
}

TEST_CASE("single buy order shows up as best bid") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1000000}, 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1000000);
  CHECK(book.get_best_ask() == std::nullopt);
}

TEST_CASE("single sell order shows up as best ask") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1010000);
  CHECK(book.get_best_bid() == std::nullopt);
}

TEST_CASE("cancel removes an order; cancelling unknown id returns false") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1000000}, 10);
  CHECK(book.cancel_order(1) == true);
  CHECK(book.get_best_bid() == std::nullopt);
  CHECK(book.cancel_order(999) == false);
  CHECK(book.cancel_order(1) == false);
}

TEST_CASE("two-sided book has spread and mid") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.spread().has_value());
  CHECK(*book.spread() == 20000);
  REQUIRE(book.mid_price().has_value());
  CHECK(*book.mid_price() == 1000000.0);
}

TEST_CASE("market sell with sufficient liquidity") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 990000);
  CHECK(book.execute_market_sell(5).notional_ticks == 990000 * 5);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{990000}) == 5);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 10);
  CHECK(book.get_best_bid().has_value());
}

TEST_CASE("market sell with insufficient liquidity") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 990000);
  CHECK(book.execute_market_sell(15).notional_ticks == 990000 * 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{990000}) == 0);

  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 10);
  CHECK(book.get_best_bid() == std::nullopt);
}

TEST_CASE("market buy with sufficient liquidity") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1010000);
  CHECK(book.execute_market_buy(5).notional_ticks == 1010000 * 5);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{990000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 5);
  CHECK(book.get_best_ask().has_value());
}

TEST_CASE("market buy with insufficient liquidity") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1010000);
  CHECK(book.execute_market_buy(15).notional_ticks == 1010000 * 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{990000}) == 10);

  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.get_best_ask() == std::nullopt);
}

TEST_CASE("limit buy fully crosses and matches asks") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(4, LimitOrderBook::Price{1000000}, 10);
  REQUIRE(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1000000);
  book.add_buy_order(5, LimitOrderBook::Price{1010000}, 30);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.get_best_ask() == std::nullopt);
}

TEST_CASE("limit sell fully crosses and matches bids") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1010000}, 10);
  book.add_buy_order(3, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(4, LimitOrderBook::Price{1030000}, 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1010000);
  LimitOrderBook::MatchResult result =
      book.add_sell_order(5, LimitOrderBook::Price{990000}, 30);
  CHECK(result.notional_ticks ==
        (990000 * 10) + (1000000 * 10) + (1010000 * 10));
  CHECK(result.filled_qty == 30);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{990000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{990000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1030000}) == 10);
  CHECK(book.get_best_bid() == std::nullopt);
}

TEST_CASE("limit buy partially crosses and matches asks") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(4, LimitOrderBook::Price{1030000}, 10);
  REQUIRE(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1000000);
  LimitOrderBook::MatchResult result =
      book.add_buy_order(5, LimitOrderBook::Price{1010000}, 25);
  CHECK(result.notional_ticks == (1000000 * 10) + (1010000 * 10));
  CHECK(result.filled_qty == 20);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 5);
  CHECK(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1010000);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1030000}) == 10);
  CHECK(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1030000);
}

TEST_CASE("limit sell partially crosses and matches bids") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_buy_order(3, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(4, LimitOrderBook::Price{1030000}, 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1010000);
  LimitOrderBook::MatchResult result =
      book.add_sell_order(5, LimitOrderBook::Price{1000000}, 25);
  CHECK(result.notional_ticks == (1010000 * 10) + (1000000 * 10));
  CHECK(result.filled_qty == 20);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{990000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 5);
  CHECK(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1000000);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1030000}) == 10);
  CHECK(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 990000);
}

TEST_CASE("limit buy at exactly best ask price matches") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(4, LimitOrderBook::Price{1000000}, 10);
  REQUIRE(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1000000);
  LimitOrderBook::MatchResult result =
      book.add_buy_order(5, LimitOrderBook::Price{1000000}, 20);
  CHECK(result.notional_ticks == 1000000 * 20);
  CHECK(result.filled_qty == 20);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 10);
  CHECK(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1010000);
}

TEST_CASE("limit sell at exactly best bid price matches") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1000000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1010000}, 10);
  book.add_buy_order(3, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(4, LimitOrderBook::Price{1030000}, 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1010000);
  LimitOrderBook::MatchResult result =
      book.add_sell_order(5, LimitOrderBook::Price{1010000}, 20);
  CHECK(result.notional_ticks == 1010000 * 20);
  CHECK(result.filled_qty == 20);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1030000}) == 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1000000);
}

TEST_CASE("limit buy below best ask rests, no fill") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1000000);
  LimitOrderBook::MatchResult result =
      book.add_buy_order(4, LimitOrderBook::Price{995000}, 10);
  CHECK(result.notional_ticks == 0);
  CHECK(result.filled_qty == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{995000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 10);
  CHECK(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1000000);
  CHECK(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 995000);
}

TEST_CASE("limit sell above best bid rests, no fill") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1000000);
  LimitOrderBook::MatchResult result =
      book.add_sell_order(4, LimitOrderBook::Price{1005000}, 10);
  CHECK(result.notional_ticks == 0);
  CHECK(result.filled_qty == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1005000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 10);
  CHECK(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1000000);
  CHECK(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1005000);
}

TEST_CASE("limit buy price cap respected - fills cheaper level only, rests at "
          "limit") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1000000);
  LimitOrderBook::MatchResult result =
      book.add_buy_order(4, LimitOrderBook::Price{1000000}, 20);
  CHECK(result.notional_ticks == 1000000 * 10);
  CHECK(result.filled_qty == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 10);
  CHECK(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1010000);
  CHECK(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1000000);
}

TEST_CASE("limit sell price cap respected - fills cheaper level only, rests at "
          "limit") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 1000000);
  LimitOrderBook::MatchResult result =
      book.add_sell_order(4, LimitOrderBook::Price{1000000}, 20);
  CHECK(result.notional_ticks == 1000000 * 10);
  CHECK(result.filled_qty == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{990000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 10);
  CHECK(book.get_best_bid().has_value());
  CHECK(book.get_best_bid()->ticks == 990000);
  CHECK(book.get_best_ask().has_value());
  CHECK(book.get_best_ask()->ticks == 1000000);
}

TEST_CASE("market buy FIFO within a price level") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  LimitOrderBook::MatchResult result = book.execute_market_buy(10);
  CHECK(result.notional_ticks == 1000000 * 10);
  CHECK(result.filled_qty == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.cancel_order(1) == false);
  CHECK(book.cancel_order(2) == true);
}

TEST_CASE("market sell FIFO within a price level") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1000000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1000000}, 10);
  LimitOrderBook::MatchResult result = book.execute_market_sell(10);
  CHECK(result.notional_ticks == 1000000 * 10);
  CHECK(result.filled_qty == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.cancel_order(1) == false);
  CHECK(book.cancel_order(2) == true);
}

TEST_CASE("market buy partial fill FIFO within a price level") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  LimitOrderBook::MatchResult result = book.execute_market_buy(5);
  CHECK(result.notional_ticks == 1000000 * 5);
  CHECK(result.filled_qty == 5);
  CHECK(book.cancel_order(2) == true);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 5);
  CHECK(book.cancel_order(1) == true);
}

TEST_CASE("market sell partial fill FIFO within a price level") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1000000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1000000}, 10);
  LimitOrderBook::MatchResult result = book.execute_market_sell(5);
  CHECK(result.notional_ticks == 1000000 * 5);
  CHECK(result.filled_qty == 5);
  CHECK(book.cancel_order(2) == true);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 5);
  CHECK(book.cancel_order(1) == true);
}

TEST_CASE("market buy price priority across price levels") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  LimitOrderBook::MatchResult result = book.execute_market_buy(5);
  CHECK(result.notional_ticks == 1000000 * 5);
  CHECK(result.filled_qty == 5);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 5);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 10);
  CHECK(book.cancel_order(1) == true);
  CHECK(book.cancel_order(2) == true);
}

TEST_CASE("market sell price priority across price levels") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1000000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1010000}, 10);
  LimitOrderBook::MatchResult result = book.execute_market_sell(5);
  CHECK(result.notional_ticks == 1010000 * 5);
  CHECK(result.filled_qty == 5);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 5);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.cancel_order(1) == true);
  CHECK(book.cancel_order(2) == true);
}

TEST_CASE(
    "market buy order walks multiple levels, value reflects per-level prices") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1020000}, 10);
  LimitOrderBook::MatchResult result = book.execute_market_buy(20);
  CHECK(result.notional_ticks == (1000000 * 10) + (1010000 * 10));
  CHECK(result.filled_qty == 20);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1000000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1020000}) == 10);
}

TEST_CASE("market sell order walks multiple levels, value reflects per-level "
          "prices") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1010000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_buy_order(3, LimitOrderBook::Price{1020000}, 10);
  LimitOrderBook::MatchResult result = book.execute_market_sell(20);
  CHECK(result.notional_ticks == (1020000 * 10) + (1010000 * 10));
  CHECK(result.filled_qty == 20);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1000000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1020000}) == 0);
}

TEST_CASE("buy side cancel one of many at same level, other orders and FIFO "
          "preserved") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1010000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1010000}, 10);
  book.add_buy_order(3, LimitOrderBook::Price{1010000}, 10);
  CHECK(book.cancel_order(2) == true);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 20);
  book.execute_market_sell(10);
  CHECK(book.cancel_order(1) == false);
  CHECK(book.cancel_order(3) == true);
}

TEST_CASE("sell side cancel one of many at same level, other orders and FIFO "
          "preserved") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  CHECK(book.cancel_order(2) == true);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 20);
  book.execute_market_buy(10);
  CHECK(book.cancel_order(1) == false);
  CHECK(book.cancel_order(3) == true);
}

TEST_CASE("cancel last order at a level, removes level, best bid updates") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1000000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_buy_order(3, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_bid().has_value() == true);
  CHECK(book.get_best_bid()->ticks == 1010000);
  CHECK(book.cancel_order(3) == true);
  REQUIRE(book.get_best_bid().has_value() == true);
  CHECK(book.get_best_bid()->ticks == 1000000);
}

TEST_CASE("cancel last order at a level, removes level, best ask updates") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.get_best_ask().has_value() == true);
  CHECK(book.get_best_ask()->ticks == 1000000);
  CHECK(book.cancel_order(1) == true);
  REQUIRE(book.get_best_ask().has_value() == true);
  CHECK(book.get_best_ask()->ticks == 1010000);
}

TEST_CASE("cancel partially filled order, buy side") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1010000}, 10);
  book.add_buy_order(3, LimitOrderBook::Price{1010000}, 25);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 5);
  CHECK(book.cancel_order(3) == true);
  CHECK(book.get_best_bid() == std::nullopt);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 0);
}

TEST_CASE("cancel partially filled order, sell side") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{1010000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1010000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 25);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 5);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{1010000}) == 0);
  CHECK(book.cancel_order(3) == true);
  CHECK(book.get_best_ask() == std::nullopt);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{1010000}) == 0);
}

TEST_CASE("quoted spread bps returns expected value on two-sided book") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{90000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{110000}, 10);
  CHECK(book.quoted_spread_bps() == doctest::Approx(2000.0));
}

TEST_CASE("quoted spread bps nullopt on empty or one-sided book") {
  LimitOrderBook book;
  CHECK(book.quoted_spread_bps() == std::nullopt);
  book.add_buy_order(1, LimitOrderBook::Price{90000}, 10);
  CHECK(book.quoted_spread_bps() == std::nullopt);
  CHECK(book.cancel_order(1) == true);
  book.add_sell_order(2, LimitOrderBook::Price{110000}, 10);
  CHECK(book.quoted_spread_bps() == std::nullopt);
}

TEST_CASE("volume_at non-existent price returns 0") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{90000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{110000}, 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{95000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{105000}) == 0);
}

TEST_CASE("volume_at aggregates multiple orders at same level, buy side") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{90000}, 5);
  book.add_buy_order(2, LimitOrderBook::Price{90000}, 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{90000}) == 15);
}

TEST_CASE("volume_at aggregates multiple orders at same level, sell side") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{90000}, 5);
  book.add_sell_order(2, LimitOrderBook::Price{90000}, 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{90000}) == 15);
}

TEST_CASE("market order on empty book returns 0") {
  LimitOrderBook book;
  LimitOrderBook::MatchResult result_buy = book.execute_market_buy(10);
  CHECK(result_buy.notional_ticks == 0);
  CHECK(result_buy.filled_qty == 0);
  REQUIRE(book.get_best_bid() == std::nullopt);
  LimitOrderBook::MatchResult result_sell = book.execute_market_sell(10);
  CHECK(result_sell.notional_ticks == 0);
  CHECK(result_sell.filled_qty == 0);
  REQUIRE(book.get_best_ask() == std::nullopt);
}

TEST_CASE("market order with qty zero does nothing") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{90000}, 5);
  book.add_sell_order(2, LimitOrderBook::Price{100000}, 10);
  LimitOrderBook::MatchResult result_buy = book.execute_market_buy(0);
  CHECK(result_buy.notional_ticks == 0);
  CHECK(result_buy.filled_qty == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{100000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{90000}) == 5);
  LimitOrderBook::MatchResult result_sell = book.execute_market_sell(0);
  CHECK(result_sell.notional_ticks == 0);
  CHECK(result_sell.filled_qty == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{100000}) == 10);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{90000}) == 5);
}

TEST_CASE("buy market order exceeeds total book depth across multiple levels") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{90000}, 5);
  book.add_sell_order(2, LimitOrderBook::Price{100000}, 5);
  book.add_sell_order(3, LimitOrderBook::Price{110000}, 10);
  book.add_sell_order(4, LimitOrderBook::Price{120000}, 15);
  REQUIRE(book.get_best_ask()->ticks == 100000);
  LimitOrderBook::MatchResult result = book.execute_market_buy(40);
  CHECK(result.notional_ticks == (5 * 100000) + (10 * 110000) + (15 * 120000));
  CHECK(result.filled_qty == 30);
  REQUIRE(book.get_best_ask() == std::nullopt);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{100000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{110000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{120000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{90000}) == 5);
}

TEST_CASE("sell market order exceeds total book depth across multiple levels") {
  LimitOrderBook book;
  book.add_sell_order(1, LimitOrderBook::Price{130000}, 5);
  book.add_buy_order(2, LimitOrderBook::Price{100000}, 5);
  book.add_buy_order(3, LimitOrderBook::Price{110000}, 10);
  book.add_buy_order(4, LimitOrderBook::Price{120000}, 15);
  REQUIRE(book.get_best_bid()->ticks == 120000);
  LimitOrderBook::MatchResult result = book.execute_market_sell(40);
  CHECK(result.notional_ticks == (5 * 100000) + (10 * 110000) + (15 * 120000));
  CHECK(result.filled_qty == 30);
  REQUIRE(book.get_best_bid() == std::nullopt);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{100000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{110000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Buy,
                       LimitOrderBook::Price{120000}) == 0);
  CHECK(book.volume_at(LimitOrderBook::Side::Sell,
                       LimitOrderBook::Price{130000}) == 5);
}

TEST_CASE("spread/mid_price updated after market buy") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_sell_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.spread().has_value() == true);
  REQUIRE(book.mid_price().has_value() == true);
  CHECK(*book.spread() == 10000);
  CHECK(*book.mid_price() == doctest::Approx(995000.0));
  book.execute_market_buy(10);
  REQUIRE(book.spread().has_value() == true);
  REQUIRE(book.mid_price().has_value() == true);
  CHECK(*book.spread() == 20000);
  CHECK(*book.mid_price() == doctest::Approx(1000000.0));
}

TEST_CASE("spread/mid_price updated after market sell") {
  LimitOrderBook book;
  book.add_buy_order(1, LimitOrderBook::Price{990000}, 10);
  book.add_buy_order(2, LimitOrderBook::Price{1000000}, 10);
  book.add_sell_order(3, LimitOrderBook::Price{1010000}, 10);
  REQUIRE(book.spread().has_value() == true);
  REQUIRE(book.mid_price().has_value() == true);
  CHECK(*book.spread() == 10000);
  CHECK(*book.mid_price() == doctest::Approx(1005000.0));
  book.execute_market_sell(10);
  REQUIRE(book.spread().has_value() == true);
  REQUIRE(book.mid_price().has_value() == true);
  CHECK(*book.spread() == 20000);
  CHECK(*book.mid_price() == doctest::Approx(1000000.0));
}

// TEST_CASE duplicate order id: enforced by debug-only assert in add_*_order;
// not unit-testable without death-test support.
