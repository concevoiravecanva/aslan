#!/bin/bash

# ============================================================================
# ASLAN TEST RUNNER SCRIPT
# ============================================================================
# Simple script to build and run tests
# Philosophy: Antirez-style - one command to test everything
# ============================================================================

set -e  # Exit on error

echo "═══════════════════════════════════════"
echo "  BUILDING ASLAN TESTS"
echo "═══════════════════════════════════════"

# Create build directory if it doesn't exist
mkdir -p build_tests
cd build_tests

# Configure and build
echo "→ Configuring CMake..."
cmake ../tests -DCMAKE_BUILD_TYPE=Debug 2>&1 | grep -v "^--" || true

echo "→ Building..."
make test_runner 2>&1 | grep -E "Building|Linking|error|warning" || true

echo ""
echo "═══════════════════════════════════════"
echo "  RUNNING TESTS"
echo "═══════════════════════════════════════"

# Run tests
./test_runner

# Return to root
cd ..

echo ""
echo "✨ Test run complete!"
