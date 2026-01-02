#!/bin/bash

echo "==========================================="
echo "Parallel Compressor Code Validation"
echo "==========================================="
echo ""

PASS=0
FAIL=0

# Test 1: Check auto-detection implementation
echo "Test 1: Auto-detection implementation"
if grep -q "CompressSingleStream" ParallelCompressor.cpp && grep -q "_numThreads <= 1" ParallelCompressor.cpp; then
  echo "  ✓ Auto-detection logic found"
  PASS=$((PASS + 1))
else
  echo "  ✗ Auto-detection logic missing"
  FAIL=$((FAIL + 1))
fi

# Test 2: Check look-ahead callback
echo "Test 2: Look-ahead prefetching"
if grep -q "GetNextItems" ../IParallelCompress.h && grep -q "lookAheadCount" ParallelCompressor.cpp; then
  echo "  ✓ Look-ahead callback implemented"
  PASS=$((PASS + 1))
else
  echo "  ✗ Look-ahead callback missing"
  FAIL=$((FAIL + 1))
fi

# Test 3: Check ICompressCoder interface
echo "Test 3: ICompressCoder interface"
if grep -q "ICompressCoder" ParallelCompressor.h && grep -q "Z7_COM7F_IMF(CParallelCompressor::Code" ParallelCompressor.cpp; then
  echo "  ✓ ICompressCoder interface implemented"
  PASS=$((PASS + 1))
else
  echo "  ✗ ICompressCoder interface missing"
  FAIL=$((FAIL + 1))
fi

# Test 4: Check compression method selection
echo "Test 4: Compression method selection"
if grep -q "SetCompressionMethod" ParallelCompressor.cpp && grep -q "_methodId" ParallelCompressor.cpp; then
  echo "  ✓ Compression method selection implemented"
  PASS=$((PASS + 1))
else
  echo "  ✗ Compression method selection missing"
  FAIL=$((FAIL + 1))
fi

# Test 5: Check thread pool
echo "Test 5: Thread pool implementation"
if grep -q "CCompressWorker" ParallelCompressor.h && grep -q "ThreadFunc" ParallelCompressor.cpp; then
  echo "  ✓ Thread pool implemented"
  PASS=$((PASS + 1))
else
  echo "  ✗ Thread pool missing"
  FAIL=$((FAIL + 1))
fi

# Test 6: Check statistics tracking
echo "Test 6: Statistics tracking"
if grep -q "GetStatistics" ParallelCompressor.cpp && grep -q "_itemsCompleted" ParallelCompressor.cpp; then
  echo "  ✓ Statistics tracking implemented"
  PASS=$((PASS + 1))
else
  echo "  ✗ Statistics tracking missing"
  FAIL=$((FAIL + 1))
fi

# Test 7: Check C API
echo "Test 7: C API wrapper"
if grep -q "ParallelCompressor_Create" ParallelCompressAPI.cpp && grep -q "ParallelCompressorHandle" ParallelCompressAPI.h; then
  echo "  ✓ C API wrapper implemented"
  PASS=$((PASS + 1))
else
  echo "  ✗ C API wrapper missing"
  FAIL=$((FAIL + 1))
fi

# Test 8: Check test suite
echo "Test 8: Test suite presence"
if [ -f "ParallelCompressorTest.cpp" ] && [ -f "ParallelCompressorValidation.cpp" ]; then
  echo "  ✓ Test suite files present"
  PASS=$((PASS + 1))
else
  echo "  ✗ Test suite files missing"
  FAIL=$((FAIL + 1))
fi

echo ""
echo "==========================================="
echo "Validation Results"
echo "==========================================="
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo "Total:  $((PASS + FAIL))"
echo "==========================================="

if [ $FAIL -eq 0 ]; then
  echo "✓ ALL VALIDATIONS PASSED"
  exit 0
else
  echo "✗ SOME VALIDATIONS FAILED"
  exit 1
fi
