#include "api.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;

namespace {

const char* kUA =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/122.0 Safari/537.36";

// Free tier: 25 req/day, 5 per minute.
constexpr int kMinDelayMs = 12000;  // respects 5/min (12s between calls)

std::string getApiKey() {
    if (const char* k = std::getenv("ALPHAVANTAGE_API_KEY"); k && *k) return k;
    const char* home = std::getenv("HOME");
    if (home) {
        std::string p = std::string(home) + "/.config/stockanalyzer/apikey.txt";
        std::ifstream f(p);
        if (f) {
            std::string line;
            std::getline(f, line);
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();
            if (!line.empty()) return line;
        }
    }
    throw std::runtime_error(
        "No Alpha Vantage API key found. Get a free key at "
        "https://www.alphavantage.co/support/#api-key then either set "
        "ALPHAVANTAGE_API_KEY=<key> or write it to ~/.config/stockanalyzer/apikey.txt");
}

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else if (c == ' ') {
            out += '+';
        } else {
            out += '%';
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        }
    }
    return out;
}

size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* out) {
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string get(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl init failed");
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, kUA);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK)
        throw std::runtime_error("network error: " + std::string(curl_easy_strerror(res)));
    // persist a bit between requests to respect the 5/min limit
    return body;
}

std::string aliveRequest(const std::string& function,
                         const std::string& params) {
    static std::chrono::steady_clock::time_point next_allowed{};
    auto now = std::chrono::steady_clock::now();
    if (now < next_allowed) {
        std::this_thread::sleep_for(next_allowed - now);
    }
    // throttle: free tier allows 5 calls/min -> 12s spacing
    std::string url = "https://www.alphavantage.co/query?function=" + function +
                      "&" + params + "&apikey=" + urlEncode(getApiKey());
    std::string body = get(url);
    // set next allowed = now + 12s for the following call
    next_allowed = std::chrono::steady_clock::now() + std::chrono::milliseconds(kMinDelayMs);
    return body;
}

// Alpha Vantage returns numbers as STRINGS (e.g. "120.34"). Parse them.
double parseNum(const json& j, double dflt = 0.0) {
    if (j.is_number()) return j.get<double>();
    if (j.is_string()) {
        try { return std::stod(j.get<std::string>()); } catch (...) { return dflt; }
    }
    return dflt;
}
long long parseLL(const json& j, long long dflt = 0) {
    if (j.is_number()) return j.get<long long>();
    if (j.is_string()) {
        try { return std::stoll(j.get<std::string>()); } catch (...) { return dflt; }
    }
    return dflt;
}

void checkError(const json& body) {
    if (body.contains("Error Message"))
        throw std::runtime_error(body["Error Message"].get<std::string>());
    if (body.contains("Information") && body["Information"].is_string()) {
        std::string info = body["Information"].get<std::string>();
        // Allowed for the note about rate limits; but if it's an api-key demand, surface it.
        if (info.find("apikey") != std::string::npos || info.find("API key") != std::string::npos)
            throw std::runtime_error(info);
    }
}

}  // namespace

namespace api {

std::vector<SearchHit> search(const std::string& query, int limit) {
    std::string body = aliveRequest("SYMBOL_SEARCH",
                                    "keywords=" + urlEncode(query));
    json root = json::parse(body);
    checkError(root);

    std::vector<SearchHit> hits;
    if (!root.contains("bestMatches")) return hits;
    for (const auto& m : root["bestMatches"]) {
        if (hits.size() >= static_cast<size_t>(limit)) break;
        std::string sym = m.contains("1. symbol") && m["1. symbol"].is_string()
                              ? m["1. symbol"].get<std::string>() : "";
        if (sym.empty()) continue;
        std::string name = m.contains("2. name") && m["2. name"].is_string() ? m["2. name"].get<std::string>() : "";
        std::string region = m.contains("4. region") && m["4. region"].is_string() ? m["4. region"].get<std::string>() : "";
        hits.push_back({sym, name, region});
    }
    return hits;
}

std::optional<Quote> quote(const std::string& symbol) {
    std::string body = aliveRequest("GLOBAL_QUOTE", "symbol=" + urlEncode(symbol));
    json root = json::parse(body);
    checkError(root);
    if (!root.contains("Global Quote") || root["Global Quote"].empty())
        return std::nullopt;
    const json& q = root["Global Quote"];
    Quote out;
    out.symbol = q.contains("01. symbol") ? q["01. symbol"].get<std::string>() : symbol;
    out.name = symbol;
    out.price = parseNum(q.contains("05. price") ? q["05. price"] : json(nullptr));
    out.change = parseNum(q.contains("09. change") ? q["09. change"] : json(nullptr));
    out.changePct = parseNum(q.contains("10. change percent") ? q["10. change percent"] : json(nullptr));
    out.open = parseNum(q.contains("02. open") ? q["02. open"] : json(nullptr));
    out.high = parseNum(q.contains("03. high") ? q["03. high"] : json(nullptr));
    out.low = parseNum(q.contains("04. low") ? q["04. low"] : json(nullptr));
    out.volume = parseLL(q.contains("06. volume") ? q["06. volume"] : json(nullptr));
    return out;
}

std::vector<Candle> history(const std::string& symbol, const std::string& range) {
    (void)range;  // Alpha Vantage daily only (compact = last 100 sessions).
    std::string body = aliveRequest("TIME_SERIES_DAILY_ADJUSTED",
                                    "symbol=" + urlEncode(symbol) + "&outputsize=compact");
    json root = json::parse(body);
    checkError(root);
    std::vector<Candle> out;
    if (!root.contains("Time Series (Daily)")) return out;
    const json& series = root["Time Series (Daily)"];

    // Keys are dates "YYYY-MM-DD". Convert to unix timestamp.
    for (auto it = series.begin(); it != series.end(); ++it) {
        std::string date = it.key();
        std::tm tm{};
        std::istringstream ss(date);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) continue;
        std::time_t t = std::mktime(&tm);

        const json& d = it.value();
        Candle c;
        c.timestamp = static_cast<double>(t);
        auto parse = [](const json& obj, const char* key) -> double {
            return obj.contains(key) ? parseNum(obj[key]) : 0.0;
        };
        c.open = parse(d, "1. open");
        c.high = parse(d, "2. high");
        c.low = parse(d, "3. low");
        c.close = parse(d, "4. close");
        c.volume = d.contains("6. volume") ? parseLL(d["6. volume"]) : 0;
        out.push_back(c);
    }
    std::sort(out.begin(), out.end(),
              [](const Candle& a, const Candle& b) { return a.timestamp < b.timestamp; });
    return out;
}

std::vector<Quote> quoteMany(const std::vector<std::string>& symbols) {
    std::vector<Quote> out;
    for (const auto& s : symbols) {
        try {
            if (auto q = quote(s)) {
                // quote() throttles 12s between calls; keep it usable.
                out.push_back(*q);
            }
        } catch (...) { /* skip unresolvable symbol */ }
    }
    return out;
}

}  // namespace api
