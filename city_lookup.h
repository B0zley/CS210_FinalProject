#ifndef CITY_LOOKUP_H
#define CITY_LOOKUP_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <vector>
#include <algorithm>
#include <memory>
#include <random>
using namespace std;

string toLower(const string& s) {
    string res = s;
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

// ----- CityKey and Hasher -----
struct CityKey {
    string countryCode;
    string cityName;

    bool operator==(const CityKey& other) const {
        return countryCode == other.countryCode && cityName == other.cityName;
    }
};

struct CityKeyHasher {
    size_t operator()(const CityKey& key) const {
        return hash<string>()(key.countryCode + "|" + key.cityName);
    }
};

// ----- Trie Implementation -----
class Trie {
    struct TrieNode {
        unordered_map<string, string> cityData; // countryCode -> population
        unordered_map<char, TrieNode*> children;
        vector<pair<string, string>> allCities; // for iteration
    };

    TrieNode* root;

public:
    Trie() {
        root = new TrieNode();
    }

    void insert(const string& countryCode, const string& cityName, const string& population) {
        TrieNode* node = root;
        string city = toLower(cityName);
        for (char c : city) {
            if (!node->children[c]) node->children[c] = new TrieNode();
            node = node->children[c];
        }
        node->cityData[countryCode] = population;
        node->allCities.emplace_back(countryCode, cityName);
    }

    bool search(const string& countryCode, const string& cityName, string& population) {
        TrieNode* node = root;
        string city = toLower(cityName);
        for (char c : city) {
            if (!node->children.count(c)) return false;
            node = node->children[c];
        }
        if (node->cityData.count(countryCode)) {
            population = node->cityData[countryCode];
            return true;
        }
        return false;
    }

    vector<pair<string, string>> getAllCities() const {
        vector<pair<string, string>> result;
        getAllCitiesHelper(root, result);
        return result;
    }

    ~Trie() {
        clear(root);
    }

private:
    void clear(TrieNode* node) {
        for (auto& [c, child] : node->children)
            clear(child);
        delete node;
    }

    void getAllCitiesHelper(TrieNode* node, vector<pair<string, string>>& result) const {
        if (!node) return;
        
        for (const auto& city : node->allCities) {
            result.push_back(city);
        }
        
        for (const auto& [c, child] : node->children) {
            getAllCitiesHelper(child, result);
        }
    }
};

// ----- Cache Interface -----
class ICacheStrategy {
public:
    virtual bool get(const string& cc, const string& city, string& pop) = 0;
    virtual void put(const string& cc, const string& city, const string& pop) = 0;
    virtual string strategyName() const = 0;
    virtual ~ICacheStrategy() = default;
};

// ----- LFU Cache Implementation -----
class LFUCache : public ICacheStrategy {
    struct Entry {
        string pop;
        int freq;
        list<CityKey>::iterator it;
    };

    unordered_map<CityKey, Entry, CityKeyHasher> cache;
    unordered_map<int, list<CityKey>> freqMap;
    int minFreq = 0;
    const size_t CAPACITY = 10;

public:
    bool get(const string& cc, const string& city, string& pop) override {
        CityKey key{cc, city};
        auto it = cache.find(key);
        if (it == cache.end()) return false;

        int f = it->second.freq;
        freqMap[f].erase(it->second.it);
        if (freqMap[f].empty()) {
            freqMap.erase(f);
            if (minFreq == f) minFreq++;
        }

        freqMap[f + 1].push_front(key);
        it->second.freq++;
        it->second.it = freqMap[f + 1].begin();
        pop = it->second.pop;
        return true;
    }

    void put(const string& cc, const string& city, const string& pop) override {
        CityKey key{cc, city};
        string dummy;
        if (get(cc, city, dummy)) {
            cache[key].pop = pop;
            return;
        }

        if (cache.size() >= CAPACITY) {
            CityKey evict = freqMap[minFreq].back();
            freqMap[minFreq].pop_back();
            cache.erase(evict);
            if (freqMap[minFreq].empty()) freqMap.erase(minFreq);
        }

        minFreq = 1;
        freqMap[1].push_front(key);
        cache[key] = {pop, 1, freqMap[1].begin()};
    }

    string strategyName() const override { return "LFU"; }
};

// ----- FIFO Cache Implementation -----
class FIFOCache : public ICacheStrategy {
    list<CityKey> queue;
    unordered_map<CityKey, string, CityKeyHasher> cache;
    const size_t CAPACITY = 10;

public:
    bool get(const string& cc, const string& city, string& pop) override {
        CityKey key{cc, city};
        auto it = cache.find(key);
        if (it == cache.end()) return false;
        pop = it->second;
        return true;
    }

    void put(const string& cc, const string& city, const string& pop) override {
        CityKey key{cc, city};
        if (cache.count(key)) {
            cache[key] = pop;
            return;
        }

        if (cache.size() >= CAPACITY) {
            CityKey evict = queue.front();
            queue.pop_front();
            cache.erase(evict);
        }
        queue.push_back(key);
        cache[key] = pop;
    }

    string strategyName() const override { return "FIFO"; }
};

// ----- Random Replacement Cache -----
class RandomCache : public ICacheStrategy {
    vector<CityKey> keys;
    unordered_map<CityKey, string, CityKeyHasher> cache;
    const size_t CAPACITY = 10;
    random_device rd;
    mt19937 gen{rd()};

public:
    bool get(const string& cc, const string& city, string& pop) override {
        CityKey key{cc, city};
        auto it = cache.find(key);
        if (it == cache.end()) return false;
        pop = it->second;
        return true;
    }

    void put(const string& cc, const string& city, const string& pop) override {
        CityKey key{cc, city};
        if (cache.count(key)) {
            cache[key] = pop;
            return;
        }

        if (cache.size() >= CAPACITY) {
            uniform_int_distribution<> dist(0, keys.size()-1);
            CityKey evict = keys[dist(gen)];
            cache.erase(evict);
            keys.erase(find(keys.begin(), keys.end(), evict));
        }
        keys.push_back(key);
        cache[key] = pop;
    }

    string strategyName() const override { return "Random"; }
};

// ----- City Lookup System -----
class CityLookup {
    Trie cityTrie;
    unique_ptr<ICacheStrategy> cache;

public:
    void loadData(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Cannot open " << filename << "\n";
            return;
        }

        string line;
        getline(file, line); // skip header
        while (getline(file, line)) {
            stringstream ss(line);
            string code, name, pop;
            getline(ss, code, ',');
            getline(ss, name, ',');
            getline(ss, pop, ',');
            cityTrie.insert(code, name, pop);
        }

        cout << "Loaded city data into Trie.\n";
        file.close();
    }

    bool search(const string& country, const string& city, 
               string& pop, ICacheStrategy* testCache = nullptr) {
        if (testCache) {
            if (testCache->get(country, city, pop)) return true;
            if (cityTrie.search(country, city, pop)) {
                testCache->put(country, city, pop);
                return false;
            }
            return false;
        }
        
        if (cache->get(country, city, pop)) return true;
        if (cityTrie.search(country, city, pop)) {
            cache->put(country, city, pop);
            return false;
        }
        return false;
    }

    vector<pair<string, string>> getAllCities() const {
        return cityTrie.getAllCities();
    }

    void resetCache() {
        cache.reset();
    }

    void setCacheStrategy(ICacheStrategy* strategy) {
        cache.reset(strategy);
    }
};

#endif // CITY_LOOKUP_H