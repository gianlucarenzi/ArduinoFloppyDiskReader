#ifndef DIAGNOSTICCONFIG_H
#define DIAGNOSTICCONFIG_H

// =============================================================================
// DIAGNOSTIC SIMULATION CONFIGURATION
// =============================================================================
//
// Uncomment ONE of the following defines to enable simulation mode:
//
// 1. SIMULATE_DIAGNOSTIC_SUCCESS - Simulates a complete successful diagnostic
//    All tests pass, no hardware required
//
// 2. SIMULATE_DIAGNOSTIC_FAILURE - Simulates a diagnostic with errors
//    Tests fail at INDEX pulse detection stage
//
// If BOTH are commented out (default), real hardware diagnostic will run
//
// These defines work on all operating systems (Windows, macOS, Linux)
// =============================================================================

// Uncomment one of these lines for simulation mode:

// #define SIMULATE_DIAGNOSTIC_SUCCESS
//#define SIMULATE_DIAGNOSTIC_FAILURE

// =============================================================================

#endif // DIAGNOSTICCONFIG_H
