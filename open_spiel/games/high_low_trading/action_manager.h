#pragma once 

#include <iostream>
#include <variant>
#include <string> 
#include <array>
#include <vector>

inline constexpr int kDefaultStepsPerPlayer = 100; 
inline constexpr int kDefaultMaxContractsPerTrade = 5; 
inline constexpr int kDefaultCustomerMaxSize = 5; 
inline constexpr int kDefaultMaxContractValue = 30;
inline constexpr int kDefaultNumPlayers = 5; 
typedef int Action; 
typedef int Player; 

class Config {
  public:
    int steps_per_player_ = kDefaultStepsPerPlayer; 
    int max_contracts_per_trade_ = kDefaultMaxContractsPerTrade; 
    int customer_max_size_ = kDefaultCustomerMaxSize; 
    int max_contract_value_ = kDefaultMaxContractValue; 
    int num_players_ = kDefaultNumPlayers; 

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
    ChanceContractValueAction(Action raw_action) : contract_value_(raw_action) {}
    int contract_value_; // [1, MaxContractValue]
    std::string ToString() const; 
}; 

class ChanceHighLowAction {
  public:
    ChanceHighLowAction(bool is_high) : is_high_(is_high) {}
    
    bool is_high_; 
    std::string ToString() const; 
}; 

class ChanceCustomerTradeAction {
  public:
    ChanceCustomerTradeAction(int bid_size, int bid_price, int ask_size, int ask_price) 
        : bid_size_(bid_size), bid_price_(bid_price), ask_size_(ask_size), ask_price_(ask_price) {}
    
    int bid_size_; // [0, CustomerMaxSize]
    int bid_price_; // [0, MaxContractValue]
    int ask_size_; // [0, CustomerMaxSize]
    int ask_price_; // [0, MaxContractValue]
    std::string ToString() const; 
}; 

class ChancePermutationAction {
  public:
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
    ChanceCustomerSizeAction(int customer_size) : customer_size_(customer_size) {}
    
    int customer_size_; // [-CustomerMaxSize, CustomerMaxSize] - (0)
    std::string ToString() const; 
}; 

class PlayerTradingAction {
  public:
    PlayerTradingAction(int bid_size, int bid_price, int ask_size, int ask_price) 
        : bid_size_(bid_size), bid_price_(bid_price), ask_size_(ask_size), ask_price_(ask_price) {}
    
    int bid_size_; // [-MaxContractPerTrade, MaxContractPerTrade]
    int bid_price_; // [0, MaxContractValue]
    int ask_size_; // [-MaxContractPerTrade, MaxContractPerTrade]
    int ask_price_; // [0, MaxContractValue]
    std::string ToString() const; 
}; 

using ActionVariant = std::variant<
  ChanceContractValueAction,
  ChanceHighLowAction,
  ChanceCustomerTradeAction,
  ChancePermutationAction,
  ChanceCustomerSizeAction,
  PlayerTradingAction
>; 

// Utility functions for ActionVariant
std::string ToString(const ActionVariant& action);
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
    ActionManager(const Config& config); 
    ActionVariant RawToStructuredAction(GamePhase phase, Action raw_action) const; 
    ActionVariant RawToStructuredAction(int timestep, Action raw_action) const; 
    Action StructuredToRawAction(GamePhase phase, const ActionVariant& structured_action) const; 
    GamePhase game_phase_of_timestep(int timestep) const; 
    // Returns the min and max legal action, inclusive. 
    std::pair<int, int> valid_action_range(GamePhase phase) const; 
  private: 
    Config config_; 
};

std::vector<int> nth_permutation(int x, int n); 
int permutation_rank(const std::vector<int>& perm); 