// Copyright 2019 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ================================================================================
// HIGH LOW TRADING GAME - ACTION MANAGER
// ================================================================================
//
// OVERVIEW:
// The ActionManager handles the conversion between raw integer actions (used by
// OpenSpiel's interface) and structured action objects for the High Low Trading game.
// It manages the different phases of the game and ensures actions are properly
// encoded/decoded for each phase.
//
// GAME PHASES:
// 1. kChanceValue (timesteps 0-1): Draw two random contract values [1, max_value]
// 2. kChanceHighLow (timestep 2): Choose "high" or "low" settlement randomly  
// 3. kChancePermutation (timestep 3): Assign player roles via random permutation
//    - First 2 positions -> ValueCheaters (know contract values)
//    - Position 2 -> HighLowCheater (knows settlement direction)
//    - Remaining positions -> Customers (have target positions)
// 4. kCustomerSize (timesteps 4 to 3+num_customers): Assign target positions
//    to customer players (random values in [-customer_max_size, customer_max_size])
// 5. kPlayerTrading (remaining timesteps): Players place trading quotes in 
//    round-robin order
//
// ACTION ENCODING:
// - Chance actions are encoded as uniform random choices within valid ranges
// - Player trading actions encode (bid_size, bid_price, ask_size, ask_price)
//   into a single integer using positional encoding
// - Permutation actions use factorial number system (Lehmer code) to encode
//   all possible player role assignments
//
// ACTION CONVERSION:
// - RawToStructuredAction(): Converts OpenSpiel integer action to typed action object
// - StructuredToRawAction(): Converts typed action object back to integer
// - valid_action_range(): Returns [min, max] legal action values for each phase
//
// CONFIGURATION:
// Game parameters are encapsulated in Config class:
// - steps_per_player: How many trading rounds each player gets
// - max_contracts_per_trade: Maximum position size in single quote
// - customer_max_size: Range for customer target positions
// - max_contract_value: Maximum possible contract settlement value  
// - num_players: Total number of players (minimum 4)
//
// DEFAULT PARAMETERS:
// All default game parameters are defined as constants and can be overridden
// through OpenSpiel's GameParameters interface.

#pragma once 

#include <iostream>
#include <variant>
#include <string> 
#include <array>
#include <vector>
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"
using namespace open_spiel; 

inline constexpr int kDefaultStepsPerPlayer = 100; 
inline constexpr int kDefaultMaxContractsPerTrade = 5; 
inline constexpr int kDefaultCustomerMaxSize = 5; 
inline constexpr int kDefaultMaxContractValue = 30;
inline constexpr int kDefaultNumPlayers = 5; 
typedef int Player; 

class Config {
  public:
    int steps_per_player_ = kDefaultStepsPerPlayer; 
    int max_contracts_per_trade_ = kDefaultMaxContractsPerTrade; 
    int customer_max_size_ = kDefaultCustomerMaxSize; 
    int max_contract_value_ = kDefaultMaxContractValue; 
    int num_players_ = kDefaultNumPlayers; 

    std::string ToString() const; 
    Config() = default;
    Config(int steps_per_player, int max_contracts_per_trade, int customer_max_size, int max_contract_value, int num_players); 
}; 

// t=0, chance move: draws a uniform number [1, MaxContractValue] inclusive 
// t=1, chance move: draws another uniform number [1, MaxContractValue] inclusive 
// t=2, chance move: draws uniform "high" or "low" 
// t=3, chance move: draws (num_players!) permutation for player roles 
// t=4...num_customers+3, chance move: draws customer size [-CustomerMaxSize, CustomerMaxSize] for each customer
// t=num_customers+4, ....: players execute in round-robin order.
//    Player observation: 
//      - order_book [p0_bid, p0_bid_sz, p1_bid, p1_bid_sz, ...] = CustomerMaxSize * 2 
//      - Player private info: [role\in (0, 1, 2), info]; size 2 
//    Player action: 
//      - (bid_size, bid_price, ask_size, ask_price). Max value `MaxContracValue^2 * MaxContractsPerTrade ^ 2`
//   Player order executes against market 

enum class GamePhase {
    kChanceValue,
    kChanceHighLow, 
    kChancePermutation, 
    kCustomerSize, 
    kPlayerTrading, 
    kTerminal, 
};

// Stream insertion operator for GamePhase (needed for SPIEL_CHECK macros)
std::ostream& operator<<(std::ostream& os, const GamePhase& phase); 

enum class PlayerRole {
    kValueCheater,
    kHighLowCheater, 
    kCustomer
};

// Stream insertion operator for PlayerRole (needed for SPIEL_CHECK macros)
std::ostream& operator<<(std::ostream& os, const PlayerRole& role); 

class ChanceContractValueAction {
  public:
    ChanceContractValueAction() : contract_value_(0) {}  // Default constructor
    ChanceContractValueAction(Action raw_action) : contract_value_(raw_action) {}
    int contract_value_; // [1, MaxContractValue]
    std::string ToString() const; 
}; 

class ChanceHighLowAction {
  public:
    ChanceHighLowAction() : is_high_(false) {}  // Default constructor
    ChanceHighLowAction(bool is_high) : is_high_(is_high) {}
    
    bool is_high_; 
    std::string ToString() const; 
}; 

class PlayerQuoteAction {
  public:
    PlayerQuoteAction() : bid_size_(0), bid_price_(1), ask_size_(0), ask_price_(1) {}  // Default constructor
    PlayerQuoteAction(int bid_size, int bid_price, int ask_size, int ask_price) 
        : bid_size_(bid_size), bid_price_(bid_price), ask_size_(ask_size), ask_price_(ask_price) {}
    
    int bid_size_; // [0, CustomerMaxSize]
    int bid_price_; // [0, MaxContractValue]
    int ask_size_; // [0, CustomerMaxSize]
    int ask_price_; // [0, MaxContractValue]
    std::string ToString() const; 
}; 

class ChancePermutationAction {
  public:
    ChancePermutationAction() {}  // Default constructor
    ChancePermutationAction(const std::vector<PlayerRole>& player_roles, const std::vector<int>& permutation) 
        : player_roles_(player_roles), permutation_(permutation) {}
    ChancePermutationAction(std::vector<PlayerRole>&& player_roles, std::vector<int>&& permutation) 
        : player_roles_(std::move(player_roles)), permutation_(std::move(permutation)) {}
    
    std::vector<PlayerRole> player_roles_; // player_roles_[player_id] = player's assigned role 
    std::vector<int> permutation_; // permutation[player_id] = player's role ranking 
    std::string ToString() const; 
}; 

class ChanceCustomerSizeAction {
  public:
    ChanceCustomerSizeAction() : customer_size_(0) {}  // Default constructor
    ChanceCustomerSizeAction(int customer_size) : customer_size_(customer_size) {}
    
    int customer_size_; // [-CustomerMaxSize, CustomerMaxSize] - (0)
    std::string ToString() const; 
}; 

using ActionVariant = std::variant<
  ChanceContractValueAction,
  ChanceHighLowAction,
  PlayerQuoteAction,
  ChancePermutationAction,
  ChanceCustomerSizeAction
>; 

// Utility functions for ActionVariant
std::string ActionVariantToString(const ActionVariant& action);
std::ostream& operator<<(std::ostream& os, const ActionVariant& action);

// Helper function to safely get a specific action type
// template<typename T>
// const T& GetAction(const ActionVariant& action) {
//     if (!std::holds_alternative<T>(action)) {
//         throw std::runtime_error("ActionVariant does not hold the requested type");
//     }
//     return std::get<T>(action);
// }
// template<typename T>
// bool IsActionType(const ActionVariant& action) {
//     return std::holds_alternative<T>(action);
// }

class ActionManager {
  public:
    ActionManager() = default;  // Default constructor
    ActionManager(const Config& config); 
    ActionVariant RawToStructuredAction(GamePhase phase, Action raw_action) const; 
    ActionVariant RawToStructuredAction(int timestep, Action raw_action) const; 
    Action StructuredToRawAction(GamePhase phase, const ActionVariant& structured_action) const; 
    GamePhase game_phase_of_timestep(int timestep) const; 
    // Returns the min and max legal action, inclusive. 
    std::pair<int, int> valid_action_range(GamePhase phase) const; 
    int GetNumPlayers() const { return config_.num_players_; }
    int GetStepsPerPlayer() const { return config_.steps_per_player_; }
    int GetMaxContractsPerTrade() const { return config_.max_contracts_per_trade_; }
    int GetMaxContractValue() const { return config_.max_contract_value_; }
    int GetCustomerMaxSize() const { return config_.customer_max_size_; }
  private: 
    Config config_; 
};

std::vector<int> nth_permutation(int x, int n); 
int permutation_rank(const std::vector<int>& perm); 
int factorial(int n);