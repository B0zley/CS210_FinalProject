#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include "city_lookup.h"

using namespace std;
using namespace std::chrono;

struct TestResult {
    double avg_time;
    double hit_rate;
    string strategy;
};

TestResult run_test(const vector<pair<string, string>>& queries,
                   ICacheStrategy* cache, CityLookup& lookup) {
    int hits = 0;
    double total_time = 0;

    for (const auto& [city, country] : queries) {
        string pop;

        auto start = high_resolution_clock::now();
        bool hit = lookup.search(country, city, pop, cache);
        auto end = high_resolution_clock::now();

        total_time += duration_cast<microseconds>(end - start).count();
        hits += hit ? 1 : 0;
    }

    return {
        total_time / queries.size(),  // avg_time (microseconds)
        static_cast<double>(hits) / queries.size(),  // hit_rate
        cache->strategyName()  // strategy name
    };
}

vector<pair<string, string>> generate_queries(const vector<pair<string, string>>& cities,
                                           int count, float repeat_prob) {
    vector<pair<string, string>> queries;
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0, 1);

    for (int i = 0; i < count; ++i) {
        if (dis(gen) < repeat_prob && !queries.empty()) {
            // Repeat a previous query to test cache
            uniform_int_distribution<> prev(0, queries.size()-1);
            queries.push_back(queries[prev(gen)]);
        } else {
            // Select a random city
            uniform_int_distribution<> dist(0, cities.size()-1);
            queries.push_back(cities[dist(gen)]);
        }
    }

    return queries;
}

void write_results(const vector<TestResult>& results, const string& filename) {
    ofstream out(filename);
    out << "Strategy,AvgTime(μs),HitRate\n";
    for (const auto& res : results) {
        out << res.strategy << "," << res.avg_time << "," << res.hit_rate << "\n";
    }
}

int main() {
    // Load city data
    CityLookup lookup;
    lookup.loadData("world_cities.csv");

    // Get all cities for query generation
    auto all_cities = lookup.getAllCities();

    // Generate test queries (1000 with 30% repeat probability)
    auto queries = generate_queries(all_cities, 1000, 0.3);

    vector<TestResult> results;

    // Test each strategy
    vector<unique_ptr<ICacheStrategy>> strategies;
    strategies.emplace_back(new LFUCache());
    strategies.emplace_back(new FIFOCache());
    strategies.emplace_back(new RandomCache());

    for (auto& strategy : strategies) {
        // Reset lookup system
        lookup.resetCache();

        // Run test
        auto res = run_test(queries, strategy.get(), lookup);
        results.push_back(res);

        cout << "Tested " << res.strategy
             << " - Avg: " << res.avg_time << "μs"
             << ", Hit Rate: " << res.hit_rate * 100 << "%\n";
    }

    // Save results
    write_results(results, "performance_results.csv");

    return 0;
}