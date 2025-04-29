#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <list>
#include <algorithm>
using namespace std;

string toLower(const string& s) {
    string res = s;
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

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

string searchCityInCSV(const string& filename, const string& countryCode, const string& cityName) {
    ifstream file(filename);
    if (!file.is_open()) return "";

    string line;
    getline(file, line);
    while (getline(file, line)) {
        stringstream ss(line);
        string code, name, pop;
        getline(ss, code, ',');
        getline(ss, name, ',');
        getline(ss, pop, ',');
        if (toLower(code) == toLower(countryCode) && toLower(name) == toLower(cityName))
            return pop;
    }
    return "City not found.";
}

class ICacheStrategy {
public:
    virtual bool get(const string& country, const string& city, string& population) = 0;
    virtual void put(const string& country, const string& city, const string& population) = 0;
    virtual void printCache() const = 0;
    virtual ~ICacheStrategy() = default;
};

class LFUCache : public ICacheStrategy {
    struct CacheEntry {
        string population;
        int freq;
        list<CityKey>::iterator iter;
    };

    unordered_map<CityKey, CacheEntry, CityKeyHasher> cache;
    unordered_map<int, list<CityKey>> freqList;
    const size_t capacity = 10;
    int minFreq = 0;

public:
    bool get(const string& cc, const string& name, string& pop) override {
        CityKey key{cc, name};
        auto it = cache.find(key);
        if (it == cache.end()) return false;

        int freq = it->second.freq;
        freqList[freq].erase(it->second.iter);
        if (freqList[freq].empty()) {
            freqList.erase(freq);
            if (freq == minFreq) minFreq++;
        }

        freqList[freq + 1].push_front(key);
        it->second.freq++;
        it->second.iter = freqList[freq + 1].begin();
        pop = it->second.population;
        return true;
    }

    void put(const string& cc, const string& name, const string& pop) override {
        CityKey key{cc, name};

        string dummy;
        if (get(cc, name, dummy)) {
            cache[key].population = pop;
            return;
        }

        if (cache.size() == capacity) {
            CityKey evictKey = freqList[minFreq].back();
            freqList[minFreq].pop_back();
            if (freqList[minFreq].empty()) freqList.erase(minFreq);
            cache.erase(evictKey);
        }

        minFreq = 1;
        freqList[1].push_front(key);
        cache[key] = {pop, 1, freqList[1].begin()};
    }

    void printCache() const override {
        cout << "[LFU Cache]:" << endl;
        for (const auto& [key, entry] : cache)
            cout << key.cityName << " (" << key.countryCode << ") - Pop: " << entry.population << ", Freq: " << entry.freq << endl;
    }
};

class FIFOCache : public ICacheStrategy {
    unordered_map<CityKey, string, CityKeyHasher> cache;
    list<CityKey> order;
    const size_t capacity = 10;

public:
    bool get(const string& cc, const string& name, string& pop) override {
        CityKey key{cc, name};
        auto it = cache.find(key);
        if (it != cache.end()) {
            pop = it->second;
            return true;
        }
        return false;
    }

    void put(const string& cc, const string& name, const string& pop) override {
        CityKey key{cc, name};
        if (cache.find(key) != cache.end()) return;

        if (cache.size() == capacity) {
            CityKey old = order.front();
            order.pop_front();
            cache.erase(old);
        }

        cache[key] = pop;
        order.push_back(key);
    }

    void printCache() const override {
        cout << "[FIFO Cache]:" << endl;
        for (const auto& key : order)
            cout << key.cityName << " (" << key.countryCode << ") - Pop: " << cache.at(key) << endl;
    }
};

int main() {
    ICacheStrategy* cache = new LFUCache();
    string filename = "world_cities.csv";

    while (true) {
        string city, country;
        cout << "Enter city (or 'exit'): ";
        getline(cin, city);
        if (toLower(city) == "exit") break;
        cout << "Enter country code: ";
        getline(cin, country);

        string population;
        if (cache->get(country, city, population)) {
            cout << "Population (from cache): " << population << endl;
        } else {
            population = searchCityInCSV(filename, country, city);
            if (population != "City not found.") {
                cache->put(country, city, population);
            }
            cout << "Population: " << population << endl;
        }

        cache->printCache();
    }

    delete cache;
    return 0;
}
