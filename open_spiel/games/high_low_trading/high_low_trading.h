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
  std::string ObservationString(Player player) const override;
  void InformationStateTensor(Player player,
                              absl::Span<float> values) const override;
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
  std::array<ChanceContractValueAction, 2> contract_values_; 
  ChanceHighLowAction contract_high_settle_; 
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

    ActionManager GetActionManager() const { return action_manager_; }

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