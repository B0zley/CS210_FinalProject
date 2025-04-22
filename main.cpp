#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

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
        if (code == countryCode && name == cityName) {
            return population;
        }
    }

    return "City not found.";
}

int main() {
    string filename = "world_cities.csv";
    while (true) {
        string city, code;
        cout << "Enter city name (or 'exit' to quit): ";
        getline(cin, city);
        if (city == "exit") break;
        cout << "Enter country code: ";
        getline(cin, code);
        string population = searchCityInCSV(filename, code, city);
        cout << "Population: " << population << endl;
    }
    return 0;
}