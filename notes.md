# Example 
Making individual builds & examples:
```
cd /home/nlyu/Code/open_spiel/build

make -j$(nproc) market_example && ./examples/market_example
make -j$(nproc) market_test && ./games/market_test
```
For a single environment:
```
make -C build simple_bluff_test && ./build/games/simple_bluff_test
python3 open_spiel/python/examples/simple_bluff_example.py

make -C build high_low_trading && ./build/games/high_low_trading
```
Misc code 
```
# Comprehensive building & testing:
./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=false

./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=false --test_only="python/tests/pyspiel_test.py"
./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=false --test_only="python/../integration_tests/playthrough_test.py"

# Generate playthrough
python open_spiel/python/examples/playthrough.py --game simple_match

# Generate playthroughs & test 
python open_spiel/python/examples/playthrough.py --game simple_match --output_file open_spiel/integration_tests/playthroughs/simple_match.txt
# Playthrough test 
./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=false --test_only="python/../integration_tests/playthrough_test.py"

./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=false --test_only="python/../integration_tests/api_test.py"

# Build-only
./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=true --build_only=true

# Build-only & test in console 
./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=true --build_only=true && python3 open_spiel/python/examples/mcts.py --game=simple_match --player1=human --player2=mcts

./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=true --build_only=true && python3 open_spiel/python/examples/mcts.py --game=tic_tac_toe --player1=mcts --player2=human
```

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

# Question on subclass management

Looking at the OpenSpiel framework in `spiel.h`, I can answer your questions about history and information state management:

## 1. PlayerAction Type Definition

`PlayerAction` is defined as a struct within the `State` class around line 476:

```cpp
// We store (player, action) pairs in the history.
struct PlayerAction {
  Player player;
  Action action;
  bool operator==(const PlayerAction&) const;
};
```

This simple struct stores which player took what action, allowing the framework to track the complete sequence of (player, action) pairs that led to the current state.

## 2. History Management

**Child games do NOT need to manually manage history for regular gameplay**. The base `State` class handles this automatically:
Ran tool

**Perfect! The history is managed automatically by the framework.** Here's how it works:

Looking at the `ApplyAction` implementation in `spiel.cc`:

```cpp
void State::ApplyAction(Action action_id) {
  // history_ needs to be modified *after* DoApplyAction which could
  // be using it.
  Player player = CurrentPlayer();
  DoApplyAction(action_id);              // Game-specific logic
  history_.push_back({player, action_id}); // Framework adds to history
  ++move_number_;                         // Framework increments move number
}
```

Child games only implement `DoApplyAction()` for game-specific logic. The base class automatically:
- Captures the current player before the action
- Calls the child's `DoApplyAction()`  
- Adds the `{player, action}` pair to `history_`
- Increments `move_number_`

**Exception:** For `UndoAction()`, child games DO need to manually manage history (as seen in your simple_match example):

```cpp
void SimpleMatchState::UndoAction(Player player, Action move) {
  // Game-specific undo logic
  player_choices_[move] = ChoiceState::kEmpty;
  current_player_ = player;
  num_moves_ -= 1;
  
  // Must manually manage history for undo
  history_.pop_back();
  --move_number_;
}
```

## 3. Information Tracked & Required Methods

**Information tracked by base State class:**
- `history_`: Complete sequence of `{player, action}` pairs
- `move_number_`: Current move number in the game
- `game_`: Shared pointer to parent game object
- `num_distinct_actions_`, `num_players_`: Cached game properties

**Required methods for child games:**
```cpp
// Core game logic
virtual Player CurrentPlayer() const = 0;
virtual std::vector<Action> LegalActions() const = 0;
virtual void DoApplyAction(Action action_id);
virtual std::string ActionToString(Player player, Action action_id) const = 0;
virtual std::string ToString() const = 0;
virtual bool IsTerminal() const = 0;
virtual std::vector<double> Returns() const = 0;
virtual std::unique_ptr<State> Clone() const = 0;
```

**Optional methods for specific features:**
```cpp
// For imperfect information games
virtual std::string InformationStateString(Player player) const;
virtual void InformationStateTensor(Player player, absl::Span<float> values) const;

// For partial observability  
virtual std::string ObservationString(Player player) const;
virtual void ObservationTensor(Player player, absl::Span<float> values) const;

// For algorithms needing fast undo
virtual void UndoAction(Player player, Action action);

// For simultaneous move games
virtual void DoApplyActions(const std::vector<Action>& actions);
```

## 4. Exposing Environment Internals

OpenSpiel doesn't have a direct equivalent to Gymnasium's `info` dict, but provides several ways to expose information:

**Built-in exposure mechanisms:**
- **`ToString()`**: Human-readable state representation
- **`ObservationString()/ObservationTensor()`**: What players can observe
- **`InformationStateString()`**: Perfect-recall information states
- **Direct state querying**: The `State` object itself can be queried for any properties
- **Custom methods**: Add your own methods to expose specific internals

**Example from your simple_match game:**
```cpp
// You could add custom methods to expose internals
int GetNumMatches() const { return num_equals; }
const std::array<ChoiceState, maxRounds * kNumPlayers>& GetChoices() const { 
  return player_choices_; 
}
```

The framework is quite flexible - you can expose any internal state through custom methods on your state class, and algorithms can access this information by downcasting the state pointer to your specific state type when needed.

## Make process

Great questions about the build system! Let me explain exactly how this works:

## 🔧 **Why `make market` doesn't work**
Read file: open_spiel/games/CMakeLists.txt

```bash
make help | grep -E "(market|games|Available)"
```

```bash
ls -la /home/nlyu/Code/open_spiel/build | head -10
```

Perfect questions! Let me explain the build system mechanics:

## 1. **Why `make market` doesn't work**

**The key insight**: `market.cc` is compiled as part of a **library**, not as a standalone **executable**.

Looking at the CMakeLists.txt:

```cmake
set(GAME_SOURCES
  # ... other files ...
  high_low_trading/market.cc
  high_low_trading/market.h
  # ... more files ...
)

add_library (games OBJECT ${GAME_SOURCES})  # ← This creates a library
```

**What's happening:**
- `market.cc` is added to the `GAME_SOURCES` list
- `GAME_SOURCES` is used to create a **library target** called `games`
- You can run `make games` (which compiles your market.cc as part of the games library)
- But there's no `make market` because `market` isn't an executable target

**Available targets:**
```bash
make help | grep market
```

**So you have:**
- ✅ `make market_example` - Builds the executable
- ✅ `make market_test` - Builds the test executable  
- ✅ `make games` - Builds the games library (which includes your market.cc)
- ❌ `make market` - **Doesn't exist** because market is not an executable target

## 2. **CMakeLists.txt → Makefile → Binaries Flow**

### **The Build System Architecture:**

```
Source Code (CMakeLists.txt files)
         ↓
    [cmake command]
         ↓
    Generated Makefiles (/build/Makefile)
         ↓
    [make command]
         ↓
    Compiled Binaries (/build/examples/, /build/games/, etc.)
```

### **Step-by-Step Process:**

**1. Initial Setup (CMake Generation Phase):**
```bash
# This happens when you first run the build script
cd /home/nlyu/Code/open_spiel
mkdir build
cd build
cmake ..  # This reads all CMakeLists.txt files and generates Makefiles
```

**2. How CMake works:**
- **Reads all `CMakeLists.txt` files** throughout the project
- **Processes dependencies** and build rules
- **Generates `Makefile`** and other build files in the `/build` directory

**3. Compilation Phase:**
```bash
# Now you can use make (which reads the generated Makefile)
make market_example  # Builds the executable
```

### **Where Everything Lives:**
```bash
find . -name "market_example" -o -name "market_test" | head -5
```

```bash
find . -name "*market*" -type f | head -10
```

Perfect! Here's the complete picture:

### **Binary Locations:**
```
/build/
├── examples/
│   └── market_example              # ← Your main executable
├── games/
│   ├── market_test                 # ← Your test executable
│   └── CMakeFiles/games.dir/high_low_trading/
│       └── market.cc.o            # ← Compiled market.cc object
└── examples/CMakeFiles/market_example.dir/
    └── market_example.cc.o        # ← Compiled example object
```

## 🎯 **Build System Summary**

### **What CMake Does:**
1. **Reads** all `CMakeLists.txt` files in the project
2. **Generates** `Makefile` and build configuration in `/build/`
3. **Creates build targets** like `market_example`, `market_test`, `games`

### **What Make Does:**
1. **Reads** the generated `Makefile`
2. **Compiles** source files into object files (`.cc.o`)
3. **Links** object files into executables
4. **Manages** dependencies and incremental builds

### **File Flow:**
```
Source: market.cc
    ↓ (compile)
Object: market.cc.o  (part of games library)
    ↓ (link with market_example.cc.o)
Executable: market_example
```

## ✅ **Available Build Commands**

**For your market development:**
```bash
# Build just the market-related targets
make market_example    # Fast: builds only what's needed for the example
make market_test      # Fast: builds only what's needed for tests
make games           # Builds the games library (includes market.cc)

# Build everything (slower)
make -j$(nproc)      # Parallel build of entire project
```

**Why this design?**
- **Efficiency**: `market.cc` is compiled once and reused by multiple executables
- **Modularity**: Games are libraries that can be linked into different programs
- **Scalability**: OpenSpiel has 100+ games - individual make targets would be unwieldy

**The key insight**: OpenSpiel treats games as **reusable libraries**, not standalone programs. Your `market_example` executable links against the `games` library that contains your compiled `market.cc` code.