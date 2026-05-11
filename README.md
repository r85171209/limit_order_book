# Limit Order Book

A single-threaded C++20 Limit Order Book utilizing a price-time-priority matching engine.

Built to explore the data structures and design tradeoffs of a Limit Order Book.

## Build & run tests

The project exists in a single .cpp file aside from a single translation unit for tests and the doctest header.

```
g++ -std=c++20 -O2 -Wall -Wextra test_lob.cpp -o test_lob ./test_lob
```

## Public API

```cpp
class LimitOrderBook {
public:
  struct Price        { int64_t ticks; /* ... */ };
  struct MatchResult  { int64_t filled_qty; int64_t notional_ticks; };
  enum class Side     { Buy, Sell };

  // submit / cancel
  MatchResult add_buy_order (int id, Price price, int64_t qty);
  MatchResult add_sell_order(int id, Price price, int64_t qty);
  bool        cancel_order  (int id);

  // market orders
  MatchResult execute_market_buy (int64_t qty);
  MatchResult execute_market_sell(int64_t qty);

  // top of book + analytics
  std::optional<Price>   get_best_bid()       const;
  std::optional<Price>   get_best_ask()       const;
  std::optional<int64_t> spread()             const; // in ticks
  std::optional<double>  mid_price()          const; // in ticks
  std::optional<double>  quoted_spread_bps()  const;
  int64_t                volume_at(Side, Price) const;
};
```

Prices are in integer ticks using `SCALE = 10000`, so `Price{1010000}` represents $101.0000. `MatchResult::notional_ticks` is `qty * price.ticks`, divide it by `SCALE` for the dollar notional value.

## Data structures & complexity

```
bids:        std::map<Price, std::list<Order>, std::greater<Price>>
asks:        std::map<Price, std::list<Order>, std::less<Price>>
orders_map:  std::unordered_map<int, std::list<Order>::iterator>
```

**Price-keyed levels.** Each side is a `std::map` keyed by `Price`, valued by a `std::list<Order>` representing the FIFO queue at that level. A tree (rather than a hashmap) is required because the matching engine walks levels in *price order*, best price first. The bid map uses `std::greater<Price>` and the ask map uses `std::less<Price>`, so `book.begin()` always points to the best level on either side.

**Per-level FIFO.** `std::list<Order>` preserves time priority within a level. New orders are appended with `push_back` and matching iterates from the front.

**O(1) cancel.** The `orders_map` stores a list-iterator per order id. List iterators are stable across insertions and erasures of *other* nodes, which is what makes O(1) cancel possible without sacrificing the per-level FIFO.

Let `L` = number of distinct price levels on a side, `F` = number of orders touched during a fill, `N` = orders at a single price level.

- **`add_*_order`: O(log L) + O(F).** The level lookup or insertion in `std::map` is O(log L). Appending to the per-level list is O(1). If the order crosses, it consumes up to `F` resting orders.
- **`cancel_order`: O(1) amortised.** Hash lookup of the iterator is O(1), list erase is O(1) on the cached iterator. If cancelling empties the level, removing it from the map is O(log L), but this only happens once per level erasure rather than once per cancel.
- **`execute_market_*`: O(F).** Each filled order costs O(1) plus its level-erasure cost when emptied. The dominant term is the number of orders consumed.
- **`get_best_bid` / `get_best_ask` / `spread` / `mid_price` / `quoted_spread_bps`: O(1).** `std::map::begin()` is constant-time and the comparators (`greater<Price>` for bids, `less<Price>` for asks) ensure the best price is always at the front.
- **`volume_at`: O(log L + N).** Locate the level in O(log L), then sum the per-order quantities in O(N). N is typically small, we could also pre-aggregating per-level volume as a possible optimisation if `volume_at` becomes a hot path.

## Design decisions & tradeoffs

**Match-before-rest semantics.** `add_buy_order` and `add_sell_order` first cross against the opposing book up to the limit price, this means that only the leftover qty rests as a new resting order. This is to mirror a real exchange's behaviour, there is no distinction between a "marketable limit" and a regular limit at the API level.

**Side-templated matching.** A single function template, `execute_match<MapType>`, handles both directions. The `passes_limit` helper uses `book.key_comp()` to decide whether the current best price is acceptable to the incoming order, side-agnostically. The type system encodes the side, so the matching algorithm exists in exactly one place.

**Strong-typed `Price`.** `int64_t ticks` is wrapped in a struct with C++20 spaceship, so prices can't be silently confused with raw integers or quantities. Tick-granularity integer arithmetic avoids any floating-point issues.

**Duplicate order id is a debug-mode precondition.** `add_*_order` asserts `!orders_map.contains(id)`. This is to mirror real exchange architecture where client order IDs are validated for uniqueness by the gateway / order-entry layer before reaching the matching engine, this assumes valid input for hot-path performance.

**Why Standard library and not Boost or custom containers.** The engine uses `std::map`, `std::list`, `std::unordered_map`, and `std::optional`. The main reasons are as follows:

1. *Zero dependencies.* Clone and build and run with just the C++20 compiler. Suitable for exploring the design and tradeoffs of a Limit Order Book.
2. *Starts as a good base for future optimization.* Getting the core design decisions implemented is the main purpose, the Standard library allows that easily and quickly. Benchmarks have not been implemented yet, once they are optimizations can be looked at.

Once benchmarks are implemented, the obvious replacements can be looked if they are justified:

- `std::list<Order>` → intrusive doubly-linked list backed by a slab / pool allocator, which would eliminate per-order `malloc` and improving cache locality
- `std::map<Price, ...>` → sorted vector or B-tree variant if the active-level fan-out is small (often the case in practice), trading O(log L) pointer-chasing for cache-friendly linear scans
- `std::unordered_map` → robin-hood or swiss-table hash with better collision behaviour; `std::unordered_map`'s iterator-stability guarantee constrains its implementation

**Explicit scope.** The focus is on the matching engine, not a fully featured Limit Order Book. The following are out of scope by design:

- Fees, rebates, tick-size rules
- Trading halts, opening / closing auctions, crossing sessions
- Iceberg / hidden / reserve orders
- Self-trade prevention (STP)
- Order modification (`modify`)
- Multi-threading and lock-free queues

## Test coverage

`test_lob.cpp` covers the engine's invariants via doctest. Coverage focuses on what the engine guarantees rather than per-method assertions.

- Empty / one-sided book: all top-of-book accessors return `nullopt`
- Crossing limit orders match before resting, up to the limit price
- Limit price cap is respected at the boundary (limit == best price matches; one tick worse does not)
- Price priority across levels: best price hit first regardless of insertion order
- FIFO within a level: first-in fills first, preserved across cancels and partial fills
- Cancel of a partially-filled order cleans up the remainder cleanly
- Cancel of the last order at a level erases the level entry (no ghost levels in the map)
- Spread / mid recompute against the new touch after a fill consumes the best level
- Market order with qty exceeding total depth returns total notional with no crash
- Boundary inputs (qty=0, empty book, non-existent price) are no-ops with clean return values

```bash
[doctest] test cases:  44 |  44 passed | 0 failed | 0 skipped
[doctest] assertions: 267 | 267 passed | 0 failed |
[doctest] Status: SUCCESS!
```

## TODO:

**Benchmarks.** Synthetic random workload measuring throughput (msg/sec) and latency distribution across `add`, `cancel`, and full match cycles.

**`modify` operation.** a qty *decrease* keeps the order's time priority, a qty *increase* or price change forfeits priority and the order goes to the back of the new queue.

**ITCH replay.** Reconstruct book state from public NASDAQ ITCH sample data. Validate against published end-of-day statistics.
