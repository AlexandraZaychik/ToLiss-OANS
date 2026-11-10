#pragma once
#include <iostream>
#include <stdexcept>
#include <string>

// Scenario labels are accounting only; assertions still use the existing
// exception-based harness. A label inside a parameter loop counts every row.
inline void scenario(const std::string& name) { std::cout << "CASE " << name << '\n'; }
inline void verify(bool ok, const std::string& reason) {
    if (!ok) throw std::runtime_error(reason);
}
