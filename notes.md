## 1. Where the Tests Are Located

The OpenSpiel tests are organized in several key locations:

### **Main Test Infrastructure**
- **`open_spiel/tests/`** - Contains core test utilities and framework
  - `basic_tests.h`/`basic_tests.cc` - Common test functions used by all games
  - `spiel_test.cc` - Core OpenSpiel framework tests
  - `action_view_test.cc` - Tests for action view functionality
  - `shared_lib_test.cc` - Tests for shared library functionality

### **Game-Specific Tests**
Individual C++ tests are scattered throughout the codebase following the pattern `<game_name>_test.cc`:
- `open_spiel/games/othello/othello_test.cc`
- `open_spiel/games/chess/chess_test.cc`  
- `open_spiel/games/tic_tac_toe/tic_tac_toe_test.cc`
- And many more...

### **Python Tests**
Python tests follow a similar distributed pattern with `_test.py` files:
- `open_spiel/python/tests/pyspiel_test.py` - Core Python bindings tests
- `open_spiel/python/algorithms/cfr_test.py` - Algorithm tests
- `open_spiel/python/games/tic_tac_toe_test.py` - Game-specific Python tests

### **Integration Tests**
- **`open_spiel/integration_tests/`** - High-level tests that test both C++ and Python
  - `api_test.py` - Comprehensive API testing across all games
  - `playthrough_test.py` - Tests game playthroughs for consistency
  - `playthroughs/` - Directory containing reference game playthroughs

## 2. Build System and Testing Infrastructure

### **CMake-Based Build System**
The project uses CMake as its primary build system:

```cmake
# Root CMakeLists.txt
cmake_minimum_required (VERSION 3.17)
project (open_spiel)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Testing is enabled by default
enable_testing()
```

### **Build Configuration**
The build system supports different build types configured in `open_spiel/CMakeLists.txt`:

```cmake
if(${BUILD_TYPE} STREQUAL "Testing")
  # A build used for running tests: keep all runtime checks (assert,
  # SPIEL_CHECK_*, SPIEL_DCHECK_*), but turn on some speed optimizations,
  # otherwise tests run for too long.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()
```

### **Test Registration Pattern**
Each game/component registers its tests in CMake. For example, from `open_spiel/games/CMakeLists.txt`:

```cmake
add_executable(othello_test othello/othello_test.cc ${OPEN_SPIEL_OBJECTS}
               $<TARGET_OBJECTS:tests>)
add_test(othello_test othello_test)

add_executable(chess_test chess/chess_test.cc ${OPEN_SPIEL_OBJECTS}
               $<TARGET_OBJECTS:tests>)
add_test(chess_test chess_test)
```

## 3. Multi-threaded vs Single-threaded Testing

### **Multi-threaded Build and Testing**
The system is designed to be **multi-threaded** for both building and testing:

From `open_spiel/scripts/build_and_run_tests.sh`:

```bash
if [ "$ARG_num_threads" -eq -1 ]; then
  NPROC="nproc"
  if [[ "$OSTYPE" == "darwin"* ]]; then  # Mac OSX
    NPROC="sysctl -n hw.physicalcpu"
  fi

  MAKE_NUM_PROCS=$(${NPROC})
  let TEST_NUM_PROCS=4*${MAKE_NUM_PROCS}  # 4x CPUs for testing
else
  MAKE_NUM_PROCS=$ARG_num_threads
  TEST_NUM_PROCS=$ARG_num_threads
fi

# Build with parallelization
make -j$MAKE_NUM_PROCS

# Run tests in parallel
ctest -j$TEST_NUM_PROCS --output-on-failure ../open_spiel
```

### **Key Parallelization Features**
1. **Build Parallelization**: Uses `make -j<N>` where N defaults to the number of CPU cores
2. **Test Parallelization**: Uses `ctest -j<N>` where N defaults to 4×CPU cores
3. **Configurable**: You can override with `--num_threads` flag

## 4. Testing Framework and Utilities

### **Basic Test Infrastructure**
The core testing utilities are in `open_spiel/tests/basic_tests.h`:

```cpp
// Perform num_sims random simulations of the specified game
void RandomSimTest(const Game& game, int num_sims, bool serialize = true,
                   bool verbose = true, bool mask_test = true,
                   const std::function<void(const State&)>& state_checker_fn =
                       &DefaultStateChecker,
                   int mean_field_population = -1,
                   std::shared_ptr<Observer> observer = nullptr);

// Test to ensure that there are no chance outcomes
void NoChanceOutcomesTest(const Game& game);

// Checks that the game can be loaded
void LoadGameTest(const std::string& game_name);
```

### **Typical Game Test Pattern**
Most games follow this pattern (from `open_spiel/games/othello/othello_test.cc`):

```cpp
void BasicOthelloTests() {
  testing::LoadGameTest("othello");
  testing::NoChanceOutcomesTest(*LoadGame("othello"));
  testing::RandomSimTest(*LoadGame("othello"), 100);
}

int main(int argc, char** argv) {
  open_spiel::othello::BasicOthelloTests();
}
```

### **Integration Testing**
The integration tests (`open_spiel/integration_tests/api_test.py`) run comprehensive tests across all games:

```python
# Tests run on all available games
_GAMES_TO_TEST = set([g.short_name for g in _ALL_GAMES if g.default_loadable])

# Full tree traversal tests for specific games
_GAMES_FULL_TREE_TRAVERSAL_TESTS = [
    ("kuhn_poker", "kuhn_poker"),
    ("leduc_poker", "leduc_poker"),
    ("tic_tac_toe", "tic_tac_toe"),
    # ... more games
]
```

## 5. Running the Tests

### **Standard Build and Test**
```bash
# From project root
./open_spiel/scripts/build_and_run_tests.sh
```

### **Manual Build and Test**
```bash
mkdir build && cd build
cmake -DPython3_EXECUTABLE=$(which python3) -DCMAKE_CXX_COMPILER=clang++ ../open_spiel
make -j$(nproc)          # Multi-threaded build
ctest -j$(nproc)         # Multi-threaded test execution
```

### **Test-Specific Execution**
```bash
# Run only specific tests
./open_spiel/scripts/build_and_run_tests.sh --test_only python_algorithms
```

## Summary

OpenSpiel uses a **multi-threaded** build and testing infrastructure built on CMake and CTest. The tests are distributed throughout the codebase following clear patterns, with common utilities provided in `open_spiel/tests/`. The system is designed to scale with your hardware, defaulting to using all available CPU cores for building and 4×CPU cores for testing, making it efficient for large-scale development and CI/CD pipelines.

The testing framework is comprehensive, covering unit tests, integration tests, and full game playthroughs, ensuring robust validation of the game implementations and algorithms.

## Usage 

1. Running a single test: 
```
./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=false --test_only="python/tests/pyspiel_test.py"
```