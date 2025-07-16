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
// HIGH LOW TRADING GAME
// ================================================================================
//
// OVERVIEW:
// A multi-player trading game where players trade contracts that will settle at 
// either a high or low value. Players have asymmetric information and different 
// incentives based on their randomly assigned roles.
//
// GAME MECHANICS:
// 1. Two contract values are randomly drawn from [1, max_contract_value]
// 2. A "high" or "low" settlement is randomly chosen
// 3. Final contract value = max(value1, value2) if "high", min(value1, value2) if "low"
// 4. Players are randomly assigned roles with private information:
//    - ValueCheaters (2): Know one of the candidate contract values
//    - HighLowCheater (1): Knows whether settlement will be "high" or "low" 
//    - Customers (rest): Have target positions they want to achieve
//
// TRADING PHASE:
// Players take turns placing quotes (bid_price, bid_size, ask_price, ask_size)
// that are matched through a continuous double auction market. Orders execute
// immediately when they cross (bid_price >= ask_price).
//
// SCORING:
// - All players: final_cash + final_position * actual_contract_value
// - Customers: additional penalty for missing target position (max contract value per each missed position)
//
// INFORMATION TENSOR LAYOUT:
// Each player observes:
// 1. Game setup & private information (11 elements):
//    - Game parameters (5): [steps_per_player, max_contracts_per_trade, 
//                           customer_max_size, max_contract_value, num_players]
//    - One-hot player role (3): [is_value_cheater, is_high_low_cheater, is_customer]
//    - Player ID encoding (2): [sin(2π*id/players), cos(2π*id/players)]
//    - Private information (1): [contract_value | high_low_signal | target_position]
//
// 2. Public information (dynamic size):
//    - All player positions (num_players * 2): [contracts, cash] for each player
//    - All historical quotes (num_quotes * 6): [bid_px, ask_px, bid_sz, ask_sz, 
//                                              sin(2π*player_id/players), cos(...)]
//
// BUILD & RUN:
// Run the following command to play interactively:
// ./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=true --build_only=true && ./build/examples/high_low_trading_interactive_play
// Components: `market` provides matching & filling. `action_manager` provides unstructured (integer)-structured action conversion. 

// Sample global game state string:
// ********** Game setup **********
// Contract values: 5, 25
// Contract high settle: High
// Player permutation: Player roles: P0=ValueCheater, P1=HighLowCheater, P2=Customer, P3=ValueCheater
// Player 0 target position: No requirement
// Player 1 target position: 2
// Player 2 target position: No requirement
// Player 3 target position: No requirement
// ********************************
// ********** Quote & Fills **********
// Player 0 quote: 1 @ 30 [1 x 1]
// Player 1 quote: 2 @ 29 [1 x 1]
// Player 2 quote: 29 @ 30 [1 x 1]
// Order fill: sz 1 @ px 29 on t=13. User 2 crossed with user 1's quote sz 1 @ px 29
// ********************************
// ********** Player Positions **********
// Player 0 position: [0 contracts, 0 cash]
// Player 1 position: [-1 contracts, 29 cash]
// Player 2 position: [1 contracts, -29 cash]
// Player 3 position: [0 contracts, 0 cash]
// ********************************
// ********** Current Market **********
// ####### 2 sell orders #######
// sz 1 @ px 30   id=2 @ t=15
// sz 1 @ px 30   id=0 @ t=11
// #############################
// ####### 2 buy orders #######
// sz 1 @ px 2   id=1 @ t=12
// sz 1 @ px 1   id=0 @ t=10
// #############################

#ifndef OPEN_SPIEL_GAMES_HIGH_LOW_TRADING_H_
#define OPEN_SPIEL_GAMES_HIGH_LOW_TRADING_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"
#include "open_spiel/games/high_low_trading/market.h"
// Game default parameters are all packaged inside "action_manager.h" 
#include "action_manager.h"

namespace open_spiel {
namespace high_low_trading {

class HighLowTradingGame;

class PlayerPosition {
  public:
    int num_contracts=0; 
    int cash_balance=0; 
    std::string ToString() const; 
}; 

class HighLowTradingState : public State {
 public:
  explicit HighLowTradingState(std::shared_ptr<const Game> game);
  HighLowTradingState(const HighLowTradingState&) = default;

  Player CurrentPlayer() const override;
  std::string ActionToString(Player player, Action move) const override;
  std::string ToString() const override;
  bool IsTerminal() const override;
  std::vector<double> Returns() const override;
  std::string InformationStateString(Player player) const override;
  // Each player's information state tensor: 
  // 1. Game setup & private information (11):
  //    - Game setup (5): [num_steps, max_contracts_per_trade, customer_max_size, max_contract_value, players]
  //    - One-hot player role (3): [is_value, is_maxmin, is_customer]
  //    - Player id (2): *player_id = [sin(2 pi player_id / players), cos(...)]. *player_id is always denoted with sin and cos
  //    - Private information (1): [contract value, max / min, customer target size]
  // 2. Public information (num_timesteps * num_players * 6 + num_players * 2): quotes, positions
  //    - Positions (num_players, 2): [num_contracts, cash_position]
  //    - Quotes (num_timesteps, num_players, 6): [bid_px, ask_px, bid_sz, ask_sz, *player_id]
  void InformationStateTensor(Player player,
                              absl::Span<float> values) const override;
  std::string ObservationString(Player player) const override;
  void ObservationTensor(Player player,
                         absl::Span<float> values) const override;
  std::unique_ptr<State> Clone() const override;
  void UndoAction(Player player, Action move) override;
  std::vector<std::pair<Action, double>> ChanceOutcomes() const override;
  std::vector<Action> LegalActions() const override;
  std::unique_ptr<State> ResampleFromInfostate(
      int player_id, std::function<double()> rng) const override;

 protected:
  void DoApplyAction(Action move) override;
  const HighLowTradingGame* GetGame() const;
  const ActionManager& GetActionManager() const;
  int GetContractValue() const; 

 private:
  std::string PublicInformationString() const; 
  std::array<ChanceContractValueAction, 2> contract_values_; 
  ChanceHighLowAction contract_high_settle_; 
  // player_permutation_.permutation[unpermed_id] = [permed_id]
  // The (0, 1, 2, ...) = (valueCheater1, valueCheater2, highLowCheater, Customers...) arrangement is in `perm`. 
  // player_role[permed id] = permed_id's role. All other "target_position, player_positions" are in permed basis
  ChancePermutationAction player_permutation_; 
  std::vector<std::pair<int, PlayerQuoteAction>> player_quotes_; 
  std::vector<PlayerPosition> player_positions_; 
  // Encodes the target positions of each player (indexed by player_id)
  // 0 stands for no requirement, since customer sizes are always nonzero. 
  std::vector<int> player_target_positions_; 
  std::vector<trade_matching::OrderFillEntry> order_fills_; 
  trade_matching::Market market_; 
};

class HighLowTradingGame : public Game {
  public:
    explicit HighLowTradingGame(const GameParameters& params);
    int NumDistinctActions() const override; 
    int MaxChanceOutcomes() const override; 
    std::unique_ptr<State> NewInitialState() const override;
    std::vector<int> InformationStateTensorShape() const override;
    std::vector<int> ObservationTensorShape() const override;

    int MaxGameLength() const override {
      return MaxChanceNodesInHistory() + GetStepsPerPlayer() * GetNumPlayers(); 
    }
    int MaxChanceNodesInHistory() const override {
      // See action_manager.h: four chance moves (high, low, choice, permutation) with num_customer assignments. 
      return 4 + (GetNumPlayers() - 3); 
    }
    double MinUtility() const override { return -MaxUtility(); }
    double MaxUtility() const override {
      return (GetMaxContractValue() - 1) * GetMaxContractsPerTrade() * GetStepsPerPlayer() * GetNumPlayers(); 
    }
    int NumPlayers() const override { return GetNumPlayers(); }
    absl::optional<double> UtilitySum() const override { return 0; }

    const ActionManager& GetActionManager() const { return action_manager_; }

    int GetNumPlayers() const { return action_manager_.GetNumPlayers(); }
    int GetStepsPerPlayer() const { return action_manager_.GetStepsPerPlayer(); }
    int GetMaxContractsPerTrade() const { return action_manager_.GetMaxContractsPerTrade(); }
    int GetMaxContractValue() const { return action_manager_.GetMaxContractValue(); }
    int GetCustomerMaxSize() const { return action_manager_.GetCustomerMaxSize(); }

  private:
    ActionManager action_manager_; 
};

}  // namespace high_low_trading
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HIGH_LOW_TRADING_H_ 