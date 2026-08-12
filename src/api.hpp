#pragma once

#include <string>
#include <vector>
#include <optional>

// A single stock that has been searched / looked up.
struct Quote {
    std::string symbol;      // e.g. "AAPL"
    std::string name;        // e.g. "Apple Inc."
    std::string exchange;    // e.g. "NMS"
    double price = 0.0;      // last price
    double change = 0.0;     // absolute change
    double changePct = 0.0;  // change percent
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    long long volume = 0;
};

struct Candle {
    double timestamp = 0;
    double open = 0, high = 0, low = 0, close = 0;
    long long volume = 0;
};

struct SearchHit {
    std::string symbol;
    std::string name;
    std::string exchange;
};

// API module: fetch stock data from free web endpoints (no API key).
namespace api {

// Perform a keyword/symbol search. Returns up to `limit` matches.
std::vector<SearchHit> search(const std::string& query, int limit = 8);

// Fetch a live quote for a symbol.
std::optional<Quote> quote(const std::string& symbol);

// Fetch historical daily candles: range = "1d","5d","1mo","3mo","6mo","1y","5y".
std::vector<Candle> history(const std::string& symbol, const std::string& range = "1mo");

// QuoteIndex summary given a list of symbols (used for watchlist monitor).
std::vector<Quote> quoteMany(const std::vector<std::string>& symbols);

}  // namespace api
