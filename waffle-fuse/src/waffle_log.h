#pragma once
// Shared verbose-logging flag for waffle-nbd.

#include <iostream>
#include <atomic>

extern std::atomic<bool> g_verbose;

// VLOG(expr) prints to stderr only when --verbose is active.
#define VLOG(msg) do { if (g_verbose.load()) { std::cerr << msg << "\n"; } } while(0)
