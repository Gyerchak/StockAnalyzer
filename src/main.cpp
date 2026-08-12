#include "api.hpp"
#include "watchlist.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string usage() {
    return R"(
StockAnalyzer — custom stock monitoring & browse/search tool (C++20, no API key)

USAGE
  stockanalyzer <command> [args...]

BROWSE / SEARCH COMMANDS
  search <query> [limit]        Browse/Keyword search for stocks (name, symbol)
  quote  <SYMBOL>               Show a live quote for one symbol
  chart  <SYMBOL> [range]       Show price history + sparkline
                                range: 1d 5d 1mo 3mo 6mo 1y 5y  (default 1mo)
  movers                        Browse top movers (top gainers/losers)

WATCHLIST COMMANDS
  watch  <SYMBOL>               Add a symbol to your watchlist
  unwatch <SYMBOL>              Remove a symbol from your watchlist
  monitor                       Show live quotes for your whole watchlist

GENERAL
  list                          Print your watchlist
  help                          Show this help

EXAMPLES
  stockanalyzer search microsoft
  stockanalyzer search aapl 5
  stockanalyzer quote AAPL
  stockanalyzer chart TSLA 3mo
  stockanalyzer watch MSFT
  stockanalyzer monitor
)";
}

[[noreturn]] void fail(const std::string& msg) {
    std::cerr << "error: " << msg << "\n";
    std::exit(1);
}

std::string formatNumber(double v) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2);
    if (v > 0) os << "+";
    os << v;
    return os.str();
}

std::string formatBig(long long v) {
    std::ostringstream os;
    if (v >= 1000000000) os << std::fixed << std::setprecision(2) << (v / 1e9) << "B";
    else if (v >= 1000000) os << std::fixed << std::setprecision(2) << (v / 1e6) << "M";
    else if (v >= 1000) os << std::fixed << std::setprecision(1) << (v / 1e3) << "K";
    else os << v;
    return os.str();
}

std::string dateStr(double ts) {
    std::time_t t = static_cast<std::time_t>(ts);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
    return buf;
}

// ASCII sparkline from closes.
std::string sparkline(const std::vector<double>& closes) {
    if (closes.size() < 2) return "";
    double lo = *std::min_element(closes.begin(), closes.end());
    double hi = *std::max_element(closes.begin(), closes.end());
    double span = (hi - lo) > 1e-9 ? (hi - lo) : 1.0;
    const char* bars = "▁▂▃▄▅▆▇█";
    std::string s;
    for (double c : closes) {
        int idx = static_cast<int>(((c - lo) / span) * 7.0 + 0.5);
        if (idx < 0) idx = 0;
        if (idx > 7) idx = 7;
        s += bars[idx];
    }
    return s;
}

void printQuote(const Quote& q, const std::string& label = "") {
    std::string name = q.name.empty() ? q.symbol : q.name;
    if (!label.empty()) std::cout << label << " ";
    std::cout << name << " (" << q.symbol << ")\n";
    std::cout << "  Price:   $" << std::fixed << std::setprecision(2) << q.price << "\n";
    std::cout << "  Change:  " << formatNumber(q.change) << "  ("
              << std::fixed << std::setprecision(2) << q.changePct << "%)\n";
    std::cout << "  Open:    $" << q.open << "  High: $" << q.high
              << "  Low: $" << q.low << "\n";
    std::cout << "  Volume:  " << formatBig(q.volume) << "\n";
    if (!q.exchange.empty()) std::cout << "  Market:  " << q.exchange << "\n";
}

int cmdSearch(const std::vector<std::string>& args) {
    if (args.empty()) fail("search needs a query\n" + usage());
    int limit = args.size() > 1 ? std::stoi(args[1]) : 8;
    auto hits = api::search(args[0], limit);
    if (hits.empty()) { std::cout << "No matches for \"" << args[0] << "\".\n"; return 0; }
    std::cout << "Search results for \"" << args[0] << "\":\n\n";
    for (const auto& h : hits) {
        std::cout << "  " << std::left << std::setw(8) << h.symbol
                  << std::setw(34) << (h.name.size() > 32 ? h.name.substr(0,32) : h.name)
                  << h.exchange << "\n";
    }
    return 0;
}

int cmdQuote(const std::vector<std::string>& args) {
    if (args.empty()) fail("quote needs a SYMBOL\n" + usage());
    auto q = api::quote(args[0]);
    if (!q) fail("could not fetch quote for '" + args[0] + "' (symbol not found?)");
    printQuote(*q);
    return 0;
}

int cmdChart(const std::vector<std::string>& args) {
    if (args.empty()) fail("chart needs a SYMBOL\n" + usage());
    std::string range = args.size() > 1 ? args[1] : "1mo";
    auto candles = api::history(args[0], range);
    if (candles.empty()) fail("could not fetch history for '" + args[0] + "'");
    std::vector<double> closes;
    for (auto& c : candles) closes.push_back(c.close);

    std::cout << args[0] << " — history (" << range << "), " << candles.size()
              << " sessions\n";
    std::cout << "  Sparkline: " << sparkline(closes) << "\n\n";

    double last = candles.back().close;
    double first = candles.front().close;
    double chg = last - first;
    double pct = first != 0 ? (chg / first) * 100.0 : 0.0;
    std::cout << "  Period close: $" << std::fixed << std::setprecision(2) << first
              << " -> $" << last << "  (" << formatNumber(chg) << ", "
              << std::fixed << std::setprecision(2) << pct << "%)\n\n";

    std::cout << std::left << std::setw(12) << "Date"
              << std::setw(10) << "Open" << std::setw(10) << "High"
              << std::setw(10) << "Low" << std::setw(10) << "Close"
              << std::setw(12) << "Volume" << "\n";
    // Print last ~20 rows.
    size_t start = candles.size() > 20 ? candles.size() - 20 : 0;
    for (size_t i = start; i < candles.size(); ++i) {
        auto& c = candles[i];
        std::cout << std::left << std::setw(12) << dateStr(c.timestamp)
                  << std::setw(10) << c.open << std::setw(10) << c.high
                  << std::setw(10) << c.low << std::setw(10) << c.close
                  << std::setw(12) << formatBig(c.volume) << "\n";
    }
    return 0;
}

int cmdMovers() {
    // Browse top tickers by moving the most: use S&P-ish symbols as a proxy.
    std::vector<std::string> tickers = {
        "AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","BRK-B","JPM","V",
        "UNH","XOM","LLY","JNJ","AVGO","PG","MA","HD","COST","ORCL"};
    auto quotes = api::quoteMany(tickers);
    if (quotes.empty()) { std::cout << "Could not fetch movers.\n"; return 0; }

    std::sort(quotes.begin(), quotes.end(),
              [](const Quote& a, const Quote& b) { return a.changePct < b.changePct; });

    std::cout << "Top movers (sample universe, sorted by change %):\n\n";
    std::cout << std::left << std::setw(10) << "Symbol" << std::setw(14) << "Change%"
              << std::setw(12) << "Price" << "Name\n";
    for (const auto& q : quotes) {
        std::cout << std::left << std::setw(10) << q.symbol
                  << std::setw(14) << (std::ostringstream{} << std::fixed << std::setprecision(2)
                      << q.changePct << "%").str()
                  << std::setw(12) << q.price
                  << (q.name.size() > 28 ? q.name.substr(0,28) : q.name) << "\n";
    }
    std::cout << "\nNOTE: 'movers' browses a fixed sample. Use 'search' to browse "
                 "by keyword or add to watchlist with 'watch'.\n";
    return 0;
}

int cmdWatch(const std::vector<std::string>& args) {
    if (args.empty()) fail("watch needs a SYMBOL\n" + usage());
    Watchlist w;
    w.add(args[0]);
    std::cout << "Added " << args[0] << " to watchlist.\n";
    return 0;
}

int cmdUnwatch(const std::vector<std::string>& args) {
    if (args.empty()) fail("unwatch needs a SYMBOL\n" + usage());
    Watchlist w;
    std::cout << (w.remove(args[0]) ? "Removed " + args[0] + " from watchlist.\n"
                                    : args[0] + " was not in the watchlist.\n");
    return 0;
}

int cmdMonitor() {
    Watchlist w;
    auto syms = w.symbols();
    if (syms.empty()) {
        std::cout << "Watchlist is empty. Add symbols with: stockanalyzer watch <SYMBOL>\n";
        return 0;
    }
    auto quotes = api::quoteMany(syms);
    if (quotes.empty()) { std::cout << "Could not fetch watchlist quotes.\n"; return 0; }

    std::cout << std::left << std::setw(10) << "Symbol" << std::setw(12) << "Price"
              << std::setw(12) << "Change%" << std::setw(14) << "Volume" << "Name\n";
    std::cout << std::string(60, '-') << "\n";
    double totalChange = 0;
    int n = 0;
    for (const auto& q : quotes) {
        std::cout << std::left << std::setw(10) << q.symbol
                  << std::setw(12) << q.price
                  << std::setw(12) << (std::ostringstream{} << std::fixed
                      << std::setprecision(2) << q.changePct << "%").str()
                  << std::setw(14) << formatBig(q.volume)
                  << (q.name.size() > 28 ? q.name.substr(0,28) : q.name) << "\n";
        totalChange += q.changePct;
        ++n;
    }
    std::cout << std::string(60, '-') << "\n";
    if (n) std::cout << "Average change: " << std::fixed << std::setprecision(2)
                     << (totalChange / n) << "%\n";
    return 0;
}

int cmdList() {
    Watchlist w;
    auto syms = w.symbols();
    if (syms.empty()) { std::cout << "Watchlist is empty.\n"; return 0; }
    std::cout << "Watchlist (" << syms.size() << "):\n";
    for (const auto& s : syms) std::cout << "  " << s << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) { std::cout << usage(); return 0; }

    std::string cmd = args[0];
    std::vector<std::string> rest(args.begin() + 1, args.end());
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    try {
        if (cmd == "search" || cmd == "browse") return cmdSearch(rest);
        if (cmd == "quote") return cmdQuote(rest);
        if (cmd == "chart" || cmd == "history") return cmdChart(rest);
        if (cmd == "movers" || cmd == "top") return cmdMovers();
        if (cmd == "watch") return cmdWatch(rest);
        if (cmd == "unwatch" || cmd == "remove") return cmdUnwatch(rest);
        if (cmd == "monitor") return cmdMonitor();
        if (cmd == "list" || cmd == "watchlist") return cmdList();
        if (cmd == "help" || cmd == "--help" || cmd == "-h") { std::cout << usage(); return 0; }
        fail("unknown command '" + args[0] + "'\n" + usage());
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
