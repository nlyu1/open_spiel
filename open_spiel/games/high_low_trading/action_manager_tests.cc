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

#include "open_spiel/games/high_low_trading/action_manager.h"

#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace high_low_trading {
namespace {

// Set to true to enable verbose output during testing
constexpr bool kVerboseOutput = false;

void TestGamePhaseMapping() {
  Config config(10, 5, 5, 30, 5);
  ActionManager action_manager(config);
  
  // Test valid timesteps
  SPIEL_CHECK_EQ(action_manager.game_phase_of_timestep(0), GamePhase::kChanceValue);
  SPIEL_CHECK_EQ(action_manager.game_phase_of_timestep(1), GamePhase::kChanceValue);
  SPIEL_CHECK_EQ(action_manager.game_phase_of_timestep(2), GamePhase::kChanceHighLow);
  SPIEL_CHECK_EQ(action_manager.game_phase_of_timestep(3), GamePhase::kChancePermutation);
  SPIEL_CHECK_EQ(action_manager.game_phase_of_timestep(4), GamePhase::kCustomerSize);
  SPIEL_CHECK_EQ(action_manager.game_phase_of_timestep(8), GamePhase::kCustomerSize);
  SPIEL_CHECK_EQ(action_manager.game_phase_of_timestep(9), GamePhase::kPlayerTrading);
  
  // Test terminal phase
  int terminal_timestep = 4 + config.num_players_ + config.steps_per_player_ * config.num_players_;
  SPIEL_CHECK_EQ(action_manager.game_phase_of_timestep(terminal_timestep), GamePhase::kTerminal);
}

void TestValidActionRanges() {
  Config config(10, 5, 5, 30, 5);
  ActionManager action_manager(config);
  
  // Test action ranges for each phase
  auto [value_min, value_max] = action_manager.valid_action_range(GamePhase::kChanceValue);
  SPIEL_CHECK_EQ(value_min, 0);
  SPIEL_CHECK_EQ(value_max, config.max_contract_value_ - 1);
  
  auto [high_low_min, high_low_max] = action_manager.valid_action_range(GamePhase::kChanceHighLow);
  SPIEL_CHECK_EQ(high_low_min, 0);
  SPIEL_CHECK_EQ(high_low_max, 1);
  
  auto [perm_min, perm_max] = action_manager.valid_action_range(GamePhase::kChancePermutation);
  SPIEL_CHECK_EQ(perm_min, 0);
  SPIEL_CHECK_EQ(perm_max, 119); // 5! - 1
  
  auto [customer_min, customer_max] = action_manager.valid_action_range(GamePhase::kCustomerSize);
  SPIEL_CHECK_EQ(customer_min, 0);
  SPIEL_CHECK_EQ(customer_max, 2 * config.customer_max_size_);
  
  auto [trading_min, trading_max] = action_manager.valid_action_range(GamePhase::kPlayerTrading);
  SPIEL_CHECK_EQ(trading_min, 0);
  int expected_max = (config.max_contracts_per_trade_ + 1) * (config.max_contracts_per_trade_ + 1) * 
                     config.max_contract_value_ * config.max_contract_value_ - 1;
  SPIEL_CHECK_EQ(trading_max, expected_max);
}

void TestActionConsistency() {
  Config config(10, 5, 5, 30, 5);
  ActionManager action_manager(config);
  
  int total_actions_tested = 0;
  int total_discrepancies = 0;
  
  for (int timestep = 0; timestep <= 15; ++timestep) {
    GamePhase phase = action_manager.game_phase_of_timestep(timestep);
    
    // Skip terminal phase
    if (phase == GamePhase::kTerminal) {
      if (kVerboseOutput) {
        std::cout << "Timestep " << timestep << ": Terminal phase, skipping" << std::endl;
      }
      continue;
    }
    
    auto [min_action, max_action] = action_manager.valid_action_range(phase);
    
    if (kVerboseOutput) {
      std::cout << "Testing timestep " << timestep << " (Phase: " << static_cast<int>(phase) 
                << "): Range [" << min_action << ", " << max_action << "]" << std::endl;
    }
    
    int discrepancy_count = 0;
    int actions_in_phase = 0;
    
    for (int raw_action = min_action; raw_action <= max_action; ++raw_action) {
      ActionVariant structured_action = action_manager.RawToStructuredAction(timestep, raw_action);
      int reverse_raw_action = action_manager.StructuredToRawAction(phase, structured_action);
      
      if (raw_action != reverse_raw_action) {
        if (kVerboseOutput) {
          std::cout << "  DISCREPANCY: Original=" << raw_action << " Reverse=" << reverse_raw_action 
                   << " Structured=" << ToString(structured_action) << std::endl;
        }
        discrepancy_count++;
      }
      actions_in_phase++;
    }
    
    // Assert no discrepancies found
    SPIEL_CHECK_EQ(discrepancy_count, 0);
    
    if (kVerboseOutput) {
      std::cout << "  All " << actions_in_phase << " actions consistent!" << std::endl << std::endl;
    }
    
    total_actions_tested += actions_in_phase;
    total_discrepancies += discrepancy_count;
  }
  
  // Final consistency check
  SPIEL_CHECK_EQ(total_discrepancies, 0);
  
  if (kVerboseOutput) {
    std::cout << "Total actions tested: " << total_actions_tested << std::endl;
    std::cout << "Total discrepancies: " << total_discrepancies << std::endl;
  }
}

void TestSpecificActionConversions() {
  Config config(10, 5, 5, 30, 5);
  ActionManager action_manager(config);
  
  // Test ChanceContractValueAction
  ActionVariant value_action = action_manager.RawToStructuredAction(GamePhase::kChanceValue, 0);
  SPIEL_CHECK_TRUE(std::holds_alternative<ChanceContractValueAction>(value_action));
  SPIEL_CHECK_EQ(std::get<ChanceContractValueAction>(value_action).contract_value_, 1);
  
  // Test ChanceHighLowAction
  ActionVariant high_action = action_manager.RawToStructuredAction(GamePhase::kChanceHighLow, 1);
  SPIEL_CHECK_TRUE(std::holds_alternative<ChanceHighLowAction>(high_action));
  SPIEL_CHECK_TRUE(std::get<ChanceHighLowAction>(high_action).is_high_);
  
  ActionVariant low_action = action_manager.RawToStructuredAction(GamePhase::kChanceHighLow, 0);
  SPIEL_CHECK_TRUE(std::holds_alternative<ChanceHighLowAction>(low_action));
  SPIEL_CHECK_FALSE(std::get<ChanceHighLowAction>(low_action).is_high_);
  
  // Test ChanceCustomerSizeAction
  ActionVariant customer_action = action_manager.RawToStructuredAction(GamePhase::kCustomerSize, 5);
  SPIEL_CHECK_TRUE(std::holds_alternative<ChanceCustomerSizeAction>(customer_action));
  SPIEL_CHECK_EQ(std::get<ChanceCustomerSizeAction>(customer_action).customer_size_, 0);
  
  // Test PlayerTradingAction
  ActionVariant trading_action = action_manager.RawToStructuredAction(GamePhase::kPlayerTrading, 0);
  SPIEL_CHECK_TRUE(std::holds_alternative<PlayerTradingAction>(trading_action));
  PlayerTradingAction trading = std::get<PlayerTradingAction>(trading_action);
  SPIEL_CHECK_EQ(trading.bid_size_, 0);
  SPIEL_CHECK_EQ(trading.ask_size_, 0);
  SPIEL_CHECK_EQ(trading.bid_price_, 1);
  SPIEL_CHECK_EQ(trading.ask_price_, 1);
}

void TestPermutationFunctions() {
  // Test nth_permutation and permutation_rank are inverses
  for (int n = 1; n <= 5; ++n) {
    int factorial_n = 1;
    for (int i = 1; i <= n; ++i) factorial_n *= i;
    
    for (int rank = 0; rank < factorial_n; ++rank) {
      std::vector<int> perm = nth_permutation(rank, n);
      int recovered_rank = permutation_rank(perm);
      SPIEL_CHECK_EQ(rank, recovered_rank);
    }
  }
}

void TestStringRepresentations() {
  // Test that ToString methods don't crash
  ChanceContractValueAction value_action(10);
  std::string value_str = value_action.ToString();
  SPIEL_CHECK_FALSE(value_str.empty());
  
  ChanceHighLowAction high_action(true);
  std::string high_str = high_action.ToString();
  SPIEL_CHECK_FALSE(high_str.empty());
  
  ChanceCustomerSizeAction size_action(3);
  std::string size_str = size_action.ToString();
  SPIEL_CHECK_FALSE(size_str.empty());
  
  PlayerTradingAction trading_action(1, 10, 2, 15);
  std::string trading_str = trading_action.ToString();
  SPIEL_CHECK_FALSE(trading_str.empty());
  
  std::vector<PlayerRole> roles = {PlayerRole::kValueCheater, PlayerRole::kCustomer};
  std::vector<int> perm = {0, 1};
  ChancePermutationAction perm_action(roles, perm);
  std::string perm_str = perm_action.ToString();
  SPIEL_CHECK_FALSE(perm_str.empty());
}

}  // namespace
}  // namespace high_low_trading
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::high_low_trading::TestGamePhaseMapping();
  open_spiel::high_low_trading::TestValidActionRanges();
  open_spiel::high_low_trading::TestActionConsistency();
  open_spiel::high_low_trading::TestSpecificActionConversions();
  open_spiel::high_low_trading::TestPermutationFunctions();
  open_spiel::high_low_trading::TestStringRepresentations();
  return 0;
}