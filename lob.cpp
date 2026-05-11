#include <cassert>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>

static constexpr int64_t SCALE = 10000;

class LimitOrderBook {
public:
  struct Price {
    int64_t ticks;
    auto operator<=>(const Price &other) const = default;
    bool operator==(const Price &other) const = default;
  };

  struct MatchResult {
    int64_t filled_qty;
    int64_t notional_ticks;
  };

  enum class Side { Buy, Sell };

private:
  struct Order {
    int id;
    Price price;
    int64_t qty;
    Side side;
  };

  // price, orders at this price level, largest first
  std::map<Price, std::list<Order>, std::greater<Price>> bids{};
  std::map<Price, std::list<Order>, std::less<Price>> asks{};

  // id -> list iterator
  std::unordered_map<int, std::list<Order>::iterator> orders_map{};

public:
  LimitOrderBook() = default;

  std::optional<Price> get_best_bid() const {
    if (bids.empty())
      return std::nullopt;

    return bids.begin()->first;
  }

  std::optional<Price> get_best_ask() const {
    if (asks.empty())
      return std::nullopt;

    return asks.begin()->first;
  }

  // returns spread in integer ticks
  std::optional<int64_t> spread() const {
    auto best_ask = get_best_ask();
    auto best_bid = get_best_bid();

    if (best_ask && best_bid) {
      return best_ask->ticks - best_bid->ticks;
    }

    return std::nullopt;
  }

  // returns quoted spread in bps as a double
  std::optional<double> quoted_spread_bps() const {
    auto spr = spread();
    auto mid_point = mid_price();
    if (spr && mid_point) {
      return (*spr / *mid_point) * 10000;
    }

    return std::nullopt;
  }

  std::optional<double> mid_price() const {
    auto best_ask = get_best_ask();
    auto best_bid = get_best_bid();

    if (best_ask && best_bid) {
      return static_cast<double>(best_ask->ticks + best_bid->ticks) / 2;
    }

    return std::nullopt;
  }

  int64_t volume_at(Side side, Price price) const {
    if (side == Side::Buy) {
      return volume_in(bids, price);
    }

    return volume_in(asks, price);
  }

  MatchResult add_buy_order(int id, Price price, int64_t qty) {
    assert(!orders_map.contains(id));
    auto match_result = execute_match(asks, qty, price);
    int64_t leftover = qty - match_result.filled_qty;
    if (leftover > 0) {
      auto &bids_price_level = bids[price];
      bids_price_level.push_back({id, price, leftover, Side::Buy});
      auto iter = std::prev(bids_price_level.end());
      orders_map[id] = iter;
    }

    return match_result;
  }

  MatchResult add_sell_order(int id, Price price, int64_t qty) {
    assert(!orders_map.contains(id));
    auto match_result = execute_match(bids, qty, price);
    int64_t leftover = qty - match_result.filled_qty;
    if (leftover > 0) {
      auto &asks_price_level = asks[price];
      asks_price_level.push_back({id, price, leftover, Side::Sell});
      auto iter = std::prev(asks_price_level.end());
      orders_map[id] = iter;
    }

    return match_result;
  }

  bool cancel_order(int id) {
    auto order_it = orders_map.find(id);
    if (order_it == orders_map.end()) {
      return false;
    }

    auto list_it = order_it->second;
    if (list_it->side == Side::Buy) {
      return cancel_in(bids, list_it->price, order_it, list_it);
    }

    return cancel_in(asks, list_it->price, order_it, list_it);
  }

  MatchResult execute_market_sell(int64_t qty) {
    return execute_match(bids, qty, std::nullopt);
  }

  MatchResult execute_market_buy(int64_t qty) {
    return execute_match(asks, qty, std::nullopt);
  }

private:
  template <typename MapType>
  int64_t volume_in(MapType &book, Price price_level) const {
    auto price_level_it = book.find(price_level);

    if (price_level_it == book.end())
      return 0;

    int64_t total_qty = 0;
    for (auto &it : price_level_it->second) {
      total_qty += it.qty;
    }

    return total_qty;
  }

  template <typename MapType>
  bool cancel_in(
      MapType &book, Price price_level,
      std::unordered_map<int, std::list<Order>::iterator>::iterator order_it,
      std::list<Order>::iterator list_it) {
    auto price_level_it = book.find(price_level);
    if (price_level_it == book.end()) {
      orders_map.erase(order_it);
      return false;
    }

    price_level_it->second.erase(list_it);
    orders_map.erase(order_it);

    if (price_level_it->second.empty()) {
      book.erase(price_level_it);
    }

    return true;
  }

  template <typename MapType>
  bool passes_limit(const MapType &book, std::optional<Price> limit) {
    if (book.empty()) {
      return false;
    }

    if (limit == std::nullopt) {
      return true;
    }

    const Price &current_price = book.begin()->first;
    if (book.key_comp()(*limit, current_price)) {
      return false;
    }

    return true;
  }

  template <typename MapType>
  MatchResult execute_match(MapType &book, int64_t qty,
                            std::optional<Price> limit) {
    int64_t notional_ticks = 0;
    int64_t filled_qty = 0;

    while (!book.empty() && qty > 0 && passes_limit(book, limit)) {
      auto current_level = book.begin();
      for (auto it = current_level->second.begin();
           it != current_level->second.end();) {
        if (it->qty <= qty) {
          filled_qty += it->qty;
          qty -= it->qty;

          notional_ticks += it->qty * it->price.ticks;

          orders_map.erase(it->id);
          it = current_level->second.erase(it);
        } else {
          it->qty -= qty;
          filled_qty += qty;

          notional_ticks += qty * it->price.ticks;

          qty = 0;
          break;
        }
      }

      if (current_level->second.empty()) {
        book.erase(current_level);
      }
    }

    return {filled_qty, notional_ticks};
  }
};
