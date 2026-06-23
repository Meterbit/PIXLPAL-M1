/**
 * @file workerWorldFlagsFns.cpp
 * @brief PSRAM-backed world-countries dataset: parse, shuffle, and access implementation.
 *
 * Parses jsonWorldCountries (a JSON array of {name, flag_4x3} objects) into a
 * PSRAM-allocated std::vector<Country> on first call, then maintains a shuffled index
 * for uniform random access without repetition. Uses EspRandomEngine (backed by
 * esp_random()) for hardware-seeded shuffling.
 */
// workerWorldFlagsFns.cpp  (definitions + state)

#include <ArduinoJson.h>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <random>
#include "mtb_engine.h"
#include "psram_allocator.h"
#include "workerWorldFlagsFns.h"

static const char TAGG[] = "WORKER_WORLD_FLAGS_FNS";

using PsString = std::basic_string<char, std::char_traits<char>, PSRAMAllocator<char>>;
struct Country { PsString name; PsString flag4x3; };

static std::vector<Country, PSRAMAllocator<Country>> gCountries;
static std::vector<size_t, PSRAMAllocator<size_t>>   gShuffledIdx;
static size_t gNext = 0;

namespace {

static inline std::string toStdString(const PsString& s) {
  return std::string(s.data(), s.size());
}

struct EspRandomEngine {
  using result_type = uint32_t;
  result_type operator()() const { return esp_random(); }
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return UINT32_MAX; }
};

void makePermutation() {
  gShuffledIdx.resize(gCountries.size());
  std::iota(gShuffledIdx.begin(), gShuffledIdx.end(), 0);
  std::shuffle(gShuffledIdx.begin(), gShuffledIdx.end(), EspRandomEngine{});
  gNext = 0;
}

void parseJsonOnce() {
  if (!gCountries.empty()) return;

  constexpr size_t CAPACITY = 200 * 1024;
  SpiRamJsonDocument doc(CAPACITY);
  DeserializationError err = deserializeJson(doc, jsonWorldCountries);
  if (err) { ESP_LOGI(TAGG, "JSON error: %s\n", err.c_str()); return; }

  for (JsonObject obj : doc.as<JsonArray>()) {
    Country c;
    c.name    = obj["name"].as<const char*>();
    c.flag4x3 = obj["flag_4x3"].as<const char*>();
    gCountries.emplace_back(std::move(c));
  }
  makePermutation();
}

// Single source of truth for "next random country"
const Country* pickNextCountry() {
  parseJsonOnce();
  if (gCountries.empty()) return nullptr;
  if (gNext >= gShuffledIdx.size()) makePermutation();
  return &gCountries[gShuffledIdx[gNext++]];
}

} // namespace

std::string getRandomFlag4x3() {
  const Country* c = pickNextCountry();
  if (!c) return {};
  return toStdString(c->flag4x3);
}

std::string getFlag4x3ByCountry(const char* name) {
  parseJsonOnce();
  if (!name) return {};
  for (const auto& c : gCountries)
    if (strcasecmp(c.name.c_str(), name) == 0)
      return toStdString(c.flag4x3);
  return {};
}

// ------------------------------------------------------------
// NEW: random country name
// ------------------------------------------------------------
std::string getRandomCountryName() {
  const Country* c = pickNextCountry();
  if (!c) return {};
  return toStdString(c->name);
}

// ------------------------------------------------------------
// NEW: random country + flag in one call (avoids duplicate lookups)
// Returns false if DB not loaded / empty.
// ------------------------------------------------------------
bool getRandomCountryAndFlag4x3(std::string& outName, std::string& outFlag4x3) {
  const Country* c = pickNextCountry();
  if (!c) {
    outName.clear();
    outFlag4x3.clear();
    return false;
  }
  outName    = toStdString(c->name);
  outFlag4x3 = toStdString(c->flag4x3);
  return true;
}

