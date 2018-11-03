#include "Constants.h"
#include <vector>
#include <sstream>
#include <unordered_map>
using namespace std;

int Constants::MAX_HALITE;
int Constants::SHIP_COST;
int Constants::DROPOFF_COST;
int Constants::MAX_TURNS;
int Constants::Constants::EXTRACT_RATIO;
double Constants::INV_EXTRACT_RATIO;
int Constants::MOVE_COST_RATIO;
bool Constants::INSPIRATION_ENABLED;
int Constants::INSPIRATION_RADIUS;
int Constants::INSPIRATION_SHIP_COUNT;
int Constants::INSPIRED_EXTRACT_RATIO;
double Constants::INSPIRED_BONUS_MULTIPLIER;
int Constants::INSPIRED_MOVE_COST_RATIO;

static const string& get_string(const unordered_map<string, string>& map, const string& key) {
    auto it = map.find(key);
    if (it == map.end()) {
        //log::log("Error: constants: server did not send " + key + " constant.");
        exit(1);
    }
    return it->second;
}

static int get_int(unordered_map<string, string>& map, const string& key) {
    return stoi(get_string(map, key));
}

static double get_double(unordered_map<string, string>& map, const string& key) {
    return stod(get_string(map, key));
}

static bool get_bool(unordered_map<string, string>& map, const string& key) {
    const string string_value = get_string(map, key);
    if (string_value == "true") {
        return true;
    }
    if (string_value == "false") {
        return false;
    }

    //log::log("Error: constants: " + key + " constant has value of '" + string_value +
    //    "' from server. Do not know how to parse that as boolean.");
    exit(1);
}



void Constants::parse(const string &line)
{
    string clean_line;
    for (char c : line) {
        switch (c) {
            case '{':
            case '}':
            case ',':
            case ':':
            case '"':
                clean_line.push_back(' ');
                break;
            default:
                clean_line.push_back(c);
                break;
        }
    }

    stringstream input_stream(clean_line);
    vector<string> tokens;

    while (true) {
        string token;
        if (!std::getline(input_stream, token, ' ')) {
            break;
        }
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    if ((tokens.size() % 2) != 0) {
        //log::log("Error: constants: expected even total number of key and value tokens from server.");
        exit(1);
    }

    unordered_map<string, string> constants_map;

    for (size_t i = 0; i < tokens.size(); i += 2) {
        constants_map[tokens[i]] = tokens[i+1];
    }

    SHIP_COST = get_int(constants_map, "NEW_ENTITY_ENERGY_COST");
    DROPOFF_COST = get_int(constants_map, "DROPOFF_COST");
    MAX_HALITE = get_int(constants_map, "MAX_ENERGY");
    MAX_TURNS = get_int(constants_map, "MAX_TURNS");
    EXTRACT_RATIO = get_int(constants_map, "EXTRACT_RATIO");
    INV_EXTRACT_RATIO = 1.0 / EXTRACT_RATIO;
    MOVE_COST_RATIO = get_int(constants_map, "MOVE_COST_RATIO");
    INSPIRATION_ENABLED = get_bool(constants_map, "INSPIRATION_ENABLED");
    INSPIRATION_RADIUS = get_int(constants_map, "INSPIRATION_RADIUS");
    INSPIRATION_SHIP_COUNT = get_int(constants_map, "INSPIRATION_SHIP_COUNT");
    INSPIRED_EXTRACT_RATIO = get_int(constants_map, "INSPIRED_EXTRACT_RATIO");
    INSPIRED_BONUS_MULTIPLIER = get_double(constants_map, "INSPIRED_BONUS_MULTIPLIER");
    INSPIRED_MOVE_COST_RATIO = get_int(constants_map, "INSPIRED_MOVE_COST_RATIO");
}
