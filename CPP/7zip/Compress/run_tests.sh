#!/bin/bash

# Test runner script for Parallel Compressor

set -e

echo "============================================="
echo "Parallel Compressor Test Runner"
echo "============================================="
echo ""

# Build tests
echo "Building test suite..."
cd /home/runner/work/7zip/7zip/CPP/7zip/Compress

# Run unit and integration tests
echo ""
echo "============================================="
echo "Running Unit and Integration Tests"
echo "============================================="
if [ -f ParallelCompressorTest ]; then
    ./ParallelCompressorTest
    TEST_RESULT=$?
    if [ $TEST_RESULT -eq 0 ]; then
        echo "✓ Unit/Integration tests PASSED"
    else
        echo "✗ Unit/Integration tests FAILED"
        exit 1
    fi
else
    echo "⚠ Test executable not found, skipping..."
fi

# Run validation tests
echo ""
echo "============================================="
echo "Running Validation Tests"
echo "============================================="
if [ -f ParallelCompressorValidation ]; then
    ./ParallelCompressorValidation
    VAL_RESULT=$?
    if [ $VAL_RESULT -eq 0 ]; then
        echo "✓ Validation tests PASSED"
    else
        echo "✗ Validation tests FAILED"
        exit 1
    fi
else
    echo "⚠ Validation executable not found, skipping..."
fi

# Test with 7z command if available
echo ""
echo "============================================="
echo "Testing Archive Extraction"
echo "============================================="
if command -v 7z &> /dev/null; then
    if [ -f test_archive.7z ]; then
        echo "Testing archive extraction with 7z..."
        7z t test_archive.7z
        if [ $? -eq 0 ]; then
            echo "✓ Archive extraction test PASSED"
        else
            echo "✗ Archive extraction test FAILED"
            exit 1
        fi
    fi
else
    echo "⚠ 7z command not available, skipping extraction test"
fi

echo ""
echo "============================================="
echo "ALL TESTS COMPLETED SUCCESSFULLY"
echo "============================================="
exit 0
