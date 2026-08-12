#pragma once

#include <string>
#include <vector>

// Persistent list of watchlist symbols stored as JSON in the user's home dir.
class Watchlist {
public:
    // Load from the default JSON file (~/.config/stockanalyzer/watchlist.json).
    Watchlist();

    // Load from a specific file.
    explicit Watchlist(const std::string& path);

    void add(const std::string& symbol);
    bool remove(const std::string& symbol);
    bool has(const std::string& symbol) const;
    std::vector<std::string> symbols() const { return symbols_; }
    void clear();

private:
    void load();
    void save() const;

    std::string path_;
    std::vector<std::string> symbols_;
};
