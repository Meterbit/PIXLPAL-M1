/**
 * @file workerWorldFlagsFns.h
 * @brief PSRAM-backed world-countries and flag data access API.
 *
 * Provides functions to randomly select country names and their 4×3 SVG flag strings
 * from the `jsonWorldCountries` JSON dataset. The dataset is parsed once into a
 * PSRAM-allocated vector on first access; subsequent calls use a shuffled index
 * for uniform random distribution without repetition until all entries are exhausted.
 */
// workerWorldFlagsFns.h  (declarations only)
#ifndef WORKER_WORLD_FLAGS_FNS_H
#define WORKER_WORLD_FLAGS_FNS_H
#include <string>

extern const char* jsonWorldCountries; /**< Pointer to the static JSON array of {name, flag_4x3} country objects. */

/**
 * @brief Return a random country name from the dataset.
 * @return std::string containing the country name (e.g. "Japan").
 */
std::string getRandomCountryName();

/**
 * @brief Return a random country name and its 4×3 SVG flag string.
 * @param outName    Receives the country name.
 * @param outFlag4x3 Receives the 4×3-aspect-ratio SVG flag string.
 * @return true  on success.
 * @return false if the dataset is empty or failed to parse.
 */
bool getRandomCountryAndFlag4x3(std::string& outName, std::string& outFlag4x3);

/**
 * @brief Return a random 4×3 SVG flag string.
 * @return SVG string for a randomly chosen country's flag.
 */
std::string getRandomFlag4x3();

/**
 * @brief Look up the 4×3 SVG flag string for a specific country by name.
 * @param name  Country name to search for (case-insensitive).
 * @return SVG string if found, empty string if not found.
 */
std::string getFlag4x3ByCountry(const char* name);

#endif // WORKER_WORLD_FLAGS_FNS_H
