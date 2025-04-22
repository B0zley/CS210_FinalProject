#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <list>
#include <algorithm>
using namespace std;

string toLower(const string& str) {
    string result = str;
    transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

string searchCityInCSV(const string& filename, const string& countryCode, const string& cityName) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open file.\n";
        return "";
    }

    string line;
    getline(file, line);

    while (getline(file, line)) {
        stringstream ss(line);
        string code, name, population;
        getline(ss, code, ',');
        getline(ss, name, ',');
        getline(ss, population, ',');
        if (toLower(code) == toLower(countryCode) && toLower(name) == toLower(cityName)) {
            return population;
        }
    }

    return "City not found.";
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

class CityCache {
private:
    unordered_map<CityKey, pair<string, list<CityKey>::iterator>, CityKeyHasher> cache;
    list<CityKey> lru;
    const size_t capacity = 10;

public:
    bool get(const string& countryCode, const string& cityName, string& population) {
        CityKey key{countryCode, cityName};
        auto it = cache.find(key);
        if (it != cache.end()) {
            lru.erase(it->second.second);
            lru.push_front(key);
            it->second.second = lru.begin();
            population = it->second.first;
            return true;
        }
        return false;
    }

    void put(const string& countryCode, const string& cityName, const string& population) {
        CityKey key{countryCode, cityName};
        if (cache.find(key) != cache.end()) {
            lru.erase(cache[key].second);
            cache.erase(key);
        }
        if (lru.size() == capacity) {
            CityKey last = lru.back();
            lru.pop_back();
            cache.erase(last);
        }
        lru.push_front(key);
        cache[key] = {population, lru.begin()};
    }
};

string filename = "world_cities.csv";
CityCache cache;

int main() {
    while (true) {
        string city, code;
        cout << "Enter city name (or 'exit' to quit): ";
        getline(cin, city);
        if (toLower(city) == "exit") break;
        cout << "Enter country code: ";
        getline(cin, code);

        string population;
        if (cache.get(code, city, population)) {
            cout << "Population (from cache): " << population << endl;
        } else {
            population = searchCityInCSV(filename, code, city);
            if (population != "City not found.") {
                cache.put(code, city, population);
            }
            cout << "Population: " << population << endl;
        }
    }
    return 0;
}

