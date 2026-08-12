#include "watchlist.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

Watchlist::Watchlist() {
    const char* home = std::getenv("HOME");
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::string base = xdg && *xdg ? xdg : std::string(home ? home : ".") + "/.config";
    path_ = base + "/stockanalyzer/watchlist.json";
    load();
}

Watchlist::Watchlist(const std::string& path) : path_(path) { load(); }

void Watchlist::load() {
    symbols_.clear();
    std::ifstream f(path_);
    if (!f) return;
    try {
        json j = json::parse(f);
        if (j.contains("symbols") && j["symbols"].is_array()) {
            for (const auto& s : j["symbols"]) {
                if (s.is_string()) {
                    std::string sym = s.get<std::string>();
                    std::transform(sym.begin(), sym.end(), sym.begin(),
                                   [](unsigned char c) { return std::toupper(c); });
                    if (!sym.empty()) symbols_.push_back(sym);
                }
            }
        }
    } catch (...) { /* ignore corrupt file */ }
}

void Watchlist::save() const {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path_).parent_path(), ec);
    json j;
    j["symbols"] = symbols_;
    std::ofstream f(path_);
    if (f) f << j.dump(2);
}

void Watchlist::add(const std::string& symbol) {
    std::string sym = symbol;
    std::transform(sym.begin(), sym.end(), sym.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    if (sym.empty()) return;
    auto it = std::find(symbols_.begin(), symbols_.end(), sym);
    if (it == symbols_.end()) symbols_.push_back(sym);
    save();
}

bool Watchlist::remove(const std::string& symbol) {
    std::string sym = symbol;
    std::transform(sym.begin(), sym.end(), sym.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    auto it = std::find(symbols_.begin(), symbols_.end(), sym);
    if (it == symbols_.end()) return false;
    symbols_.erase(it);
    save();
    return true;
}

bool Watchlist::has(const std::string& symbol) const {
    std::string sym = symbol;
    std::transform(sym.begin(), sym.end(), sym.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return std::find(symbols_.begin(), symbols_.end(), sym) != symbols_.end();
}

void Watchlist::clear() {
    symbols_.clear();
    save();
}
