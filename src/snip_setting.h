#pragma once

#include <optional>

// Reads and writes Snipping Tool's "HDR screenshot color corrector"
// (IsHDRToneMappingEnabled) in its packaged settings.dat, via RegLoadAppKey.
// Writes fail (return false / nullopt) when Snipping Tool currently holds the
// hive; the caller simply retries on the next cycle.
namespace snip {

std::optional<bool> read();
bool write(bool enabled);

} // namespace snip
