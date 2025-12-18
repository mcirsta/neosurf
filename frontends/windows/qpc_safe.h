/*
 * Copyright 2024 NetSurf Browser Project
 *
 * QueryPerformanceCounter runtime detection for Windows XP compatibility
 */

#ifndef WINDOWS_QPC_SAFE_H
#define WINDOWS_QPC_SAFE_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * QPC reliability test results
 */
enum qpc_reliability {
	QPC_RELIABILITY_UNKNOWN = 0,
	QPC_RELIABILITY_GOOD,
	QPC_RELIABILITY_BAD,
	QPC_RELIABILITY_UNSTABLE
};

/**
 * Test QueryPerformanceCounter reliability at runtime
 *
 * Performs comprehensive tests to detect QPC bugs on XP systems:
 * - Multi-core consistency check
 * - Monotonicity verification
 * - Timing accuracy validation
 *
 * @return QPC reliability status
 */
enum qpc_reliability qpc_test_reliability(void);

/**
 * Get safe monotonic time in milliseconds
 *
 * Automatically chooses between QPC and GetTickCount64() based
 * on runtime reliability testing.
 *
 * @param time_ms Output: current monotonic time in milliseconds
 * @return true on success, false on failure
 */
bool qpc_safe_get_monotonic_ms(uint64_t *time_ms);

/**
 * Get safe monotonic time in microseconds
 *
 * Automatically chooses between QPC and GetTickCount64() based
 * on runtime reliability testing.
 *
 * @param time_us Output: current monotonic time in microseconds
 * @return true on success, false on failure
 */
bool qpc_safe_get_monotonic_us(uint64_t *time_us);

/**
 * Initialize QPC safe timing subsystem
 *
 * Must be called before using qpc_safe_get_monotonic_ms()
 */
void qpc_safe_init(void);

#endif /* WINDOWS_QPC_SAFE_H */