#ifndef WORKER_WORLD_FLAGS_FNS_H
#define WORKER_WORLD_FLAGS_FNS_H
/*******************************************************
 *  workerWorldFlagsFns.h
 *******************************************************/

/*******************************************************
 *  world_flags_psram.cpp
 *******************************************************/
#include "psram_allocator.h"
#include <ArduinoJson.h>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <random>   // after <algorithm>
#include "mtb_engine.h"


extern const char* jsonWorldCountries;

// Use std::basic_string with our PSRAM allocator
using PsString = std::basic_string<char,
                   std::char_traits<char>,
                   PSRAMAllocator<char>>;

// Country struct now stores its text in PSRAM too
struct Country {
    PsString name;
    PsString flag4x3;
};

// All big containers live in PSRAM
static std::vector<Country, PSRAMAllocator<Country>> gCountries;
static std::vector<size_t, PSRAMAllocator<size_t>>   gShuffledIdx;
static size_t gNext = 0;

namespace {

struct EspRandomEngine {
    using result_type = uint32_t;
    result_type operator()() const { return esp_random(); }
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }
};

void makePermutation()
{
    gShuffledIdx.resize(gCountries.size());
    std::iota(gShuffledIdx.begin(), gShuffledIdx.end(), 0);
    std::shuffle(gShuffledIdx.begin(), gShuffledIdx.end(), EspRandomEngine{});
    gNext = 0;
}

void parseJsonOnce(){
    if (!gCountries.empty()) return;

    /* ---------- ArduinoJson in PSRAM ---------- */
    constexpr size_t CAPACITY = 200 * 1024;

    // Allocate the variant pool itself in PSRAM
    SpiRamJsonDocument doc(CAPACITY);

    DeserializationError err = deserializeJson(doc, jsonWorldCountries);
    if (err) {
        Serial.printf("JSON error: %s\n", err.c_str());
        return;
    }

    for (JsonObject obj : doc.as<JsonArray>()) {
        Country c;
        c.name    = obj["name"].as<const char*>();
        c.flag4x3 = obj["flag_4x3"].as<const char*>();
        gCountries.emplace_back(std::move(c));
    }
    makePermutation();
}

} // anonymous

/* ---------- PUBLIC API ---------- */
std::string getRandomFlag4x3(){
    parseJsonOnce();
    if (gCountries.empty()) return {};

    if (gNext >= gShuffledIdx.size()) makePermutation();

    const PsString& ps = gCountries[gShuffledIdx[gNext++]].flag4x3;
    return std::string(ps.data(), ps.size());   // explicit conversion
}

std::string getFlag4x3ByCountry(const char* name){
    parseJsonOnce();
    if (!name) return {};

    for (const auto& c : gCountries)
        if (strcasecmp(c.name.c_str(), name) == 0)
            return std::string(c.flag4x3.data(), c.flag4x3.size());
    return {};
}

#endif // WORKER_WORLD_FLAGS_FNS_H