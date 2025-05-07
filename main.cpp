#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <vector>
#include <algorithm>
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

    ~Trie() {
        clear(root);
    }

private:
    void clear(TrieNode* node) {
        for (auto& [c, child] : node->children)
            clear(child);
        delete node;
    }
};

// ----- Cache Interface -----
class ICacheStrategy {
public:
    virtual bool get(const string& cc, const string& city, string& pop) = 0;
    virtual void put(const string& cc, const string& city, const string& pop) = 0;
    virtual void printCache() const = 0;
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

    void printCache() const override {
        cout << "\n[LFU Cache State]:\n";
        for (const auto& [key, val] : cache) {
            cout << key.cityName << " (" << key.countryCode << ") - Pop: "
                 << val.pop << ", Freq: " << val.freq << "\n";
        }
    }
};

// ----- Load CSV into Trie -----
void loadCitiesIntoTrie(Trie& trie, const string& filename) {
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
        trie.insert(code, name, pop);
    }

    cout << "Loaded city data into Trie.\n";
    file.close();
}

// ----- Main Program -----
int main() {
    Trie cityTrie;
    loadCitiesIntoTrie(cityTrie, "world_cities.csv");

    ICacheStrategy* cache = nullptr;

    cout << "Select cache strategy (lfu): ";
    string strategy;
    getline(cin, strategy);
    strategy = toLower(strategy);

    if (strategy == "lfu") cache = new LFUCache();
    else {
        cout << "Unsupported strategy.\n";
        return 1;
    }

    while (true) {
        string city, country;
        cout << "\nEnter city (or 'exit'): ";
        getline(cin, city);
        if (toLower(city) == "exit") break;
        cout << "Enter country code: ";
        getline(cin, country);

        string pop;
        if (cache->get(country, city, pop)) {
            cout << "Population (cache): " << pop << endl;
        } else if (cityTrie.search(country, city, pop)) {
            cache->put(country, city, pop);
            cout << "Population (trie): " << pop << endl;
        } else {
            cout << "City not found.\n";
        }

        cache->printCache();
    }

    delete cache;
    return 0;
}
