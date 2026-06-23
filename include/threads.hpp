#pragma once

#include <zephyr/kernel.h>

// 1. Centralized Configuration
namespace ThreadConfig
{
constexpr size_t DefaultStackSize = 1024;
constexpr size_t PayloadStackSize = 2048;

// Priority levels (lower number = higher priority in Zephyr)
constexpr int SysHealthPriority = 5;
constexpr int PayloadPriority = 6;
constexpr int DfaPriority = 7;
constexpr int CommsPriority = 8;
constexpr int CommandsPriority = 9;
} // namespace ThreadConfig

// 2. Entry Function Declarations
extern void payload_entry(void *, void *, void *);
extern void dfa_entry(void *, void *, void *);
extern void comms_entry(void *, void *, void *);
extern void commands_entry(void *, void *, void *);

// 3. Thread ID Declarations (k_tid_t)
// This allows system_health.cpp to start threads defined in other files.
extern const k_tid_t payload_thread_id;
extern const k_tid_t dfa_thread_id;
extern const k_tid_t comms_thread_id;
extern const k_tid_t commands_thread_id;
