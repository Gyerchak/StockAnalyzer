# StockAnalyzer — Custom Stock Monitoring & Browse/Search Tool (C++20)

A native C++ command-line tool for monitoring stocks and browsing/searching
through the market. It fetches live data from the **Alpha Vantage** API using
libcurl and parses JSON with nlohmann.

> Requires a **free** Alpha Vantage API key (see below). Free tier: 25
> requests/day, 5 per minute — the tool throttles itself to respect this.

---

## Get a free API key

1. Go to <https://www.alphavantage.co/support/#api-key> and claim a free key
   (takes ~20 seconds).
2. Provide it to the tool one of two ways:
   - Environment variable: `export ALPHAVANTAGE_API_KEY=YOUR_KEY`
   - Config file: write the key (one line) to
     `~/.config/stockanalyzer/apikey.txt`

## Features

- **Search / browse** stocks by name or symbol (`SYMBOL_SEARCH`).
- **Live quote** (`GLOBAL_QUOTE`).
- **History + ASCII sparkline** (`TIME_SERIES_DAILY_ADJUSTED`, last 100 days).
- **Watchlist** — add/remove symbols, then **monitor** them all at once.
- Watchlist persisted to `~/.config/stockanalyzer/watchlist.json`.

## Build

Requirements: CMake ≥ 3.20, C++20 compiler, libcurl, nlohmann/json
(header-only at `/usr/include/nlohmann/json.hpp`).

```bash
./run.sh --help          # build (if needed) and run
```

or manually:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && ./stockanalyzer help
```

## Commands

```
stockanalyzer <command> [args...]

BROWSE / SEARCH
  search <query> [limit]      Browse/keyword search (name, symbol)
  quote  <SYMBOL>             Live quote
  chart  <SYMBOL> [range]     Price history + sparkline
  movers                      Browse a sample universe sorted by change %

WATCHLIST
  watch   <SYMBOL>            Add to watchlist
  unwatch <SYMBOL>            Remove from watchlist
  monitor                     Live quotes for your whole watchlist
  list                        Show your watchlist

GENERAL
  help                        Show help
```

## Examples

```bash
export ALPHAVANTAGE_API_KEY=YOUR_KEY

./run.sh search microsoft
./run.sh search aapl 5
./run.sh quote AAPL
./run.sh chart TSLA
./run.sh watch MSFT
./run.sh watch NVDA
./run.sh monitor
```

## Rate-limit note

The free Alpha Vantage tier allows 5 requests per minute. Each API call in this
tool waits ~12 seconds before the next one, so `monitor` on a large watchlist
can be slow. A paid key raises the limit.
