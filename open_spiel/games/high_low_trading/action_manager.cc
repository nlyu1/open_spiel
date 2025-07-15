#include <cmath>
#include <iostream>
#include <string>
#include <algorithm>
#include <numeric>
#include "action_manager.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/spiel_utils.h"

Config::Config(int steps_per_player, int max_contracts_per_trade, int customer_max_size, int max_contract_value, int num_players) : 
    steps_per_player_(steps_per_player), 
    max_contracts_per_trade_(max_contracts_per_trade), 
    customer_max_size_(customer_max_size), 
    max_contract_value_(max_contract_value), 
    num_players_(num_players) {}

ActionManager::ActionManager(const Config& config) : config_(config) {}

std::string ChanceContractValueAction::ToString() const {
  return "Environment settles one piece of contract value to " + std::to_string(contract_value_);
}

std::string ChanceHighLowAction::ToString() const {
    return absl::StrCat("Environment chooses ", is_high_ ? "high" : "low", " contract settlement");
}

std::string ChanceCustomerTradeAction::ToString() const {
    return absl::StrCat(
        bid_price_, " @ ", ask_price_, " [", bid_size_, " x ", ask_size_, "]"
    );
}

std::string ChancePermutationAction::ToString() const {
    std::string result = "Player roles: ";
    for (size_t i = 0; i < player_roles_.size(); ++i) {
        if (i > 0) result += ", ";
        result += "P" + std::to_string(i) + "=";
        switch (player_roles_[i]) {
            case PlayerRole::kValueCheater: result += "ValueCheater"; break;
            case PlayerRole::kHighLowCheater: result += "HighLowCheater"; break;
            case PlayerRole::kCustomer: result += "Customer"; break;
        }
    }
    return result;
}

std::string ChanceCustomerSizeAction::ToString() const {
    return absl::StrCat("Customer target position: ", customer_size_);
}

std::string PlayerTradingAction::ToString() const {
    return absl::StrCat(
        "Price ", bid_price_, " @ ", ask_price_, " | Size ", bid_size_, " x ", ask_size_
    );
}



// ActionVariant utility functions
std::string ToString(const ActionVariant& action) {
    return std::visit([](const auto& a) { return a.ToString(); }, action);
}

std::ostream& operator<<(std::ostream& os, const ActionVariant& action) {
    os << ToString(action);
    return os;
}

std::ostream& operator<<(std::ostream& os, const GamePhase& phase) {
    switch (phase) {
        case GamePhase::kChanceValue: return os << "kChanceValue";
        case GamePhase::kChanceHighLow: return os << "kChanceHighLow";
        case GamePhase::kChancePermutation: return os << "kChancePermutation";
        case GamePhase::kCustomerSize: return os << "kCustomerSize";
        case GamePhase::kPlayerTrading: return os << "kPlayerTrading";
        case GamePhase::kTerminal: return os << "kTerminal";
    }
    return os << "UnknownGamePhase";
}

std::ostream& operator<<(std::ostream& os, const PlayerRole& role) {
    switch (role) {
        case PlayerRole::kValueCheater: return os << "kValueCheater";
        case PlayerRole::kHighLowCheater: return os << "kHighLowCheater";
        case PlayerRole::kCustomer: return os << "kCustomer";
    }
    return os << "UnknownPlayerRole";
}


GamePhase ActionManager::game_phase_of_timestep(int timestep) const {
    if (timestep < 0) {
        open_spiel::SpielFatalError(absl::StrCat("Invalid timestep: ", timestep)); 
    } else if (timestep < 2) {
        return GamePhase::kChanceValue; 
    } else if (timestep == 2) {
        return GamePhase::kChanceHighLow; 
    } else if (timestep == 3) {
        return GamePhase::kChancePermutation; 
    } else if (timestep < 4 + config_.num_players_) {
        return GamePhase::kCustomerSize; 
    } else if (timestep < 4 + config_.num_players_ + config_.steps_per_player_ * config_.num_players_) {
        return GamePhase::kPlayerTrading; 
    } else {
        return GamePhase::kTerminal; 
    }
}

int factorial(int n) {
    int result = 1; 
    for (int i = 1; i <= n; ++i) {
        result *= i; 
    }
    return result; 
}

std::pair<int, int> ActionManager::valid_action_range(GamePhase phase) const {
    switch (phase) {
        case GamePhase::kChanceValue: return {0, config_.max_contract_value_ - 1}; 
        case GamePhase::kChanceHighLow: return {0, 1}; 
        case GamePhase::kChancePermutation: return {0, factorial(config_.num_players_) - 1}; 
        case GamePhase::kCustomerSize: return {0, 2 * config_.customer_max_size_}; 
        case GamePhase::kPlayerTrading: return {0, (config_.max_contracts_per_trade_ + 1) * (config_.max_contracts_per_trade_ + 1) * config_.max_contract_value_ * config_.max_contract_value_ - 1}; 
        case GamePhase::kTerminal: open_spiel::SpielFatalError("Invalid terminal phase for action range"); 
    }
    // Default case to suppress compiler warning
    open_spiel::SpielFatalError("Unhandled GamePhase in valid_action_range");
}

ActionVariant ActionManager::RawToStructuredAction(GamePhase phase, Action raw_action) const {
    auto [min_range, max_range] = valid_action_range(phase); 
    if (raw_action < min_range || raw_action > max_range) {
        open_spiel::SpielFatalError(absl::StrCat("Invalid raw action: ", raw_action)); 
    }
    switch (phase) {
        case GamePhase::kChanceValue: {
            // Contract candidate price = raw action 
            return ChanceContractValueAction(raw_action + 1);
        }
        case GamePhase::kChanceHighLow: {
            if (raw_action != 0 && raw_action != 1) {
                open_spiel::SpielFatalError(absl::StrCat("Invalid raw action: ", raw_action)); 
            }
            // Chooses high value if raw_action = 1, else low 
            return ChanceHighLowAction(raw_action == 1 ? true : false); 
        }
        case GamePhase::kChancePermutation: {
            if (raw_action < 0 || raw_action >= factorial(config_.num_players_)) {
                open_spiel::SpielFatalError(absl::StrCat("Invalid raw action: ", raw_action)); 
            }
            std::vector<int> perm = nth_permutation(raw_action, config_.num_players_); 
            std::vector<PlayerRole> player_roles(config_.num_players_); 
            for (int i = 0; i < config_.num_players_; ++i) {
                int perm_id = perm[i]; 
                if (perm_id == 0 || perm_id == 1) {
                    player_roles[i] = PlayerRole::kValueCheater; 
                } else if (perm_id == 2) {
                    player_roles[i] = PlayerRole::kHighLowCheater; 
                } else {
                    player_roles[i] = PlayerRole::kCustomer; 
                }
            }
            return ChancePermutationAction(player_roles, perm); 
        }
        case GamePhase::kCustomerSize: {
            // 0 gets mapped to most negative size; customer size can't be 0 
            // Action range: [0, 2 * CustomerMaxSize]. Customer size range: [-CustomerMaxSize, CustomerMaxSize] - {0}
            if (raw_action < 0 || raw_action > 2 * config_.customer_max_size_) {
                open_spiel::SpielFatalError(absl::StrCat("Invalid raw action: ", raw_action)); 
            }
            int customer_size = raw_action - config_.customer_max_size_; 
            if (customer_size >= 0) {
                customer_size++;
            }
            return ChanceCustomerSizeAction(customer_size); 
        }
        case GamePhase::kPlayerTrading: {
            // max_contract_value and max_contracts_per_trade are both inclusive
            // Bidding 0 size is allowed, but bidding 0 price is not; we manually add 1 to the price 
            // Action range: [0, (max_contracts + 1) ^ 2 * (max_contract_value) ^ 2]
            int rolling = raw_action; 
            int bid_size_denom = (config_.max_contracts_per_trade_ + 1) * config_.max_contract_value_ * config_.max_contract_value_; 
            int bid_size = rolling / bid_size_denom; 
            rolling = rolling % bid_size_denom; 
            int ask_size_denom = config_.max_contract_value_ * config_.max_contract_value_; 
            int ask_size = rolling / ask_size_denom; 
            rolling = rolling % ask_size_denom; 
            
            int bid_price_denom = config_.max_contract_value_; 
            int bid_price = rolling / bid_price_denom + 1; 
            rolling = rolling % bid_price_denom; 
            int ask_price = rolling + 1; 
            return PlayerTradingAction(bid_size, bid_price, ask_size, ask_price); 
        }
        case GamePhase::kTerminal: {
            open_spiel::SpielFatalError("Invalid terminal phase for action conversion"); 
        }
    }
    
    // This should never be reached, but needed to suppress compiler warning
    open_spiel::SpielFatalError("Unhandled GamePhase in RawToStructuredAction");
}

ActionVariant ActionManager::RawToStructuredAction(int timestep, Action raw_action) const {
    return RawToStructuredAction(game_phase_of_timestep(timestep), raw_action); 
}

Action ActionManager::StructuredToRawAction(GamePhase phase, const ActionVariant& structured_action) const {
    switch (phase) {
        case GamePhase::kChanceValue: {
            return std::get<ChanceContractValueAction>(structured_action).contract_value_ - 1; 
        }
        case GamePhase::kChanceHighLow: {
            return std::get<ChanceHighLowAction>(structured_action).is_high_ ? 1 : 0; 
        }
        case GamePhase::kChancePermutation: {
            return permutation_rank(std::get<ChancePermutationAction>(structured_action).permutation_); 
        } 
        case GamePhase::kCustomerSize: {
            auto customer_size = std::get<ChanceCustomerSizeAction>(structured_action).customer_size_;
            int adjusted_size = customer_size > 0 ? customer_size - 1 : customer_size; 
            return adjusted_size + config_.customer_max_size_; 
        }
        case GamePhase::kPlayerTrading: {
            PlayerTradingAction quote_action = std::get<PlayerTradingAction>(structured_action); 
            int bid_size = quote_action.bid_size_; 
            int adjusted_bid_price = quote_action.bid_price_ - 1; 
            int ask_size = quote_action.ask_size_; 
            int adjusted_ask_price = quote_action.ask_price_ - 1; 

            return adjusted_ask_price + adjusted_bid_price * config_.max_contract_value_ + 
                ask_size * config_.max_contract_value_ * config_.max_contract_value_ + 
                bid_size * (config_.max_contracts_per_trade_ + 1) * config_.max_contract_value_ * config_.max_contract_value_; 
        }
        case GamePhase::kTerminal: {
            open_spiel::SpielFatalError("Invalid terminal phase for action conversion"); 
        }
    }
    open_spiel::SpielFatalError("Unhandled GamePhase in StructuredToRawAction"); 
}

std::vector<int> nth_permutation(int x, int n)
{
    // ---------- pre-compute factorials up to n (fits if x < n!) ----------
    std::vector<int> fact(n + 1, 1);
    for (int i = 1; i <= n; ++i) fact[i] = fact[i - 1] * i;

    // ---------- Lehmer code digits ----------
    std::vector<int> lehmer(n);
    for (int i = n - 1; i >= 0; --i) {
        lehmer[n - 1 - i] = x / fact[i];
        x %= fact[i];
    }

    // ---------- decode Lehmer code into the permutation ----------
    std::vector<int> pool(n);                    // remaining elements
    std::iota(pool.begin(), pool.end(), 0);      // 0 1 2 … n-1

    std::vector<int> perm;
    perm.reserve(n);

    for (int d : lehmer) {
        perm.push_back(pool[d]);
        pool.erase(pool.begin() + d);            // O(n) per erase
    }
    return perm;
}

int permutation_rank(const std::vector<int>& perm)
{
    const int n = static_cast<int>(perm.size());

    // ---------- factorial table ----------
    std::vector<int> fact(n + 1, 1);
    for (int i = 1; i <= n; ++i) fact[i] = fact[i - 1] * static_cast<int>(i);

    // ---------- build a mutable pool ----------
    std::vector<int> pool(n);
    std::iota(pool.begin(), pool.end(), 0);  // 0 1 2 … n-1

    // ---------- accumulate rank ----------
    int rank = 0;
    for (int i = 0; i < n; ++i) {
        auto it = std::find(pool.begin(), pool.end(), perm[i]);
        int idx  = static_cast<int>(it - pool.begin());   // elements “skipped”
        rank    += static_cast<int>(idx) * fact[n - 1 - i];
        pool.erase(it);                                   // remove used element
    }
    return rank;
}