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

namespace open_spiel {
namespace high_low_trading {

inline constexpr int kDefaultStepsPerPlayer = 100; 
inline constexpr int kDefaultMaxContractsPerTrade = 5; 
inline constexpr int kDefaultCustomerMaxSize = 5; 
inline constexpr int kDefaultMaxContractValue = 30;
inline constexpr int kDefaultMaxPlayers = 5; 

class HighLowTradingGame;

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

  // Getters for internal state
  int GetPlayerValue(Player player) const { return player_values_[player]; }
  bool DidPlayerBet() const { return player1_bet_; }
  bool DidPlayerAccept() const { return player2_accept_; }

 protected:
  void DoApplyAction(Action move) override;

 private:
  // Internal state: [player1_value, player2_value, player1_bets, player2_accepts]
  std::array<int, 2> player_values_;  // 0 or 1 for each player
  bool player1_bet_;                  // whether player 1 chose to bet
  bool player2_accept_;               // whether player 2 accepted the bet
  int winner_;                        // winning player, or kInvalidPlayer if not terminal
  int pot_;                          // current pot size
  std::array<int, 2> contributions_;  // how much each player contributed
};

class HighLowTradingGame : public Game {
  public:
    explicit HighLowTradingGame(const GameParameters& params);
    int NumDistinctActions() const override { return 2; }
    std::unique_ptr<State> NewInitialState() const override;
    int MaxChanceOutcomes() const override { return 2; }
    int NumPlayers() const override { return 2; }
    double MinUtility() const override { return -2.0; }
    double MaxUtility() const override { return 2.0; }
    absl::optional<double> UtilitySum() const override { return 0; }
    std::vector<int> InformationStateTensorShape() const override;
    std::vector<int> ObservationTensorShape() const override;
    int MaxGameLength() const override { return 4; }  // 2 chance + 2 player moves
    int MaxChanceNodesInHistory() const override { return 2; }

    int GetCustomerMaxSize() const { return customer_max_size_; }
    int GetStepsPerPlayer() const { return steps_per_player_; }
    int GetMaxContractsPerTrade() const { return max_contracts_per_trade_; }
    int GetContractMaxValue() const { return contract_max_value_; }

  private: 
    int customer_max_size_; 
    int steps_per_player_; 
    int max_contracts_per_trade_; 
    int contract_max_value_; 
};


struct ChanceContractValueAction {
  int contract_value; // [1, MaxContractValue]
}; 

struct ChanceHighLowAction {
  bool is_high; 
}; 

struct ChanceCustomerSizeAction {
  int customer_size; // [-CustomerMaxSize, CustomerMaxSize]
}; 

struct ChancePermutationAction {
  std::array<int, 5> permutation; // [0, 4]
}; 

struct PlayerTradingAction {
  int bid_size; // [-MaxContractPerTrade, MaxContractPerTrade]
  int bid_price; // [0, MaxContractValue]
  int ask_size; // [-MaxContractPerTrade, MaxContractPerTrade]
  int ask_price; // [0, MaxContractValue]
}; 

using ActionVariant = std::variant<
  ChanceContractValueAction,
  ChanceHighLowAction,
  ChanceCustomerSizeAction,
  ChancePermutationAction,
  PlayerTradingAction
>; 

// Base class for Codecs which convert between action_id (int) and structured actions
class ActionCodec {
  public:
    virtual ~ActionCodec() = default; 
    virtual Action Encode(const ActionVariant& action) const = 0; 
    virtual ActionVariant Decode(Action action) const = 0; 
    virtual std::vector<ActionVariant> GetLegalActionVariants() const = 0; 
    virtual std::vector<Action> GetLegalActions() const = 0; 
};

class ChanceValueCodec : public ActionCodec {
  public:
    ChanceValueCodec(int max_contract_value); 
    Action Encode(const ActionVariant& action) const override; 
    ActionVariant Decode(Action action) const override; 
    std::vector<ActionVariant> GetLegalActionVariants() const override; 
    std::vector<Action> GetLegalActions() const override; 
};

class ChanceHighLowCodec : public ActionCodec {
  public:
    ChanceHighLowCodec(); 
    Action Encode(const ActionVariant& action) const override; 
    ActionVariant Decode(Action action) const override; 
    std::vector<ActionVariant> GetLegalActionVariants() const override; 
    std::vector<Action> GetLegalActions() const override; 
}; 

class ChanceCustomerSizeCodec : public ActionCodec {
  public:
    ChanceCustomerSizeCodec(int customer_max_size); 
    Action Encode(const ActionVariant& action) const override; 
    ActionVariant Decode(Action action) const override; 
    std::vector<ActionVariant> GetLegalActionVariants() const override; 
    std::vector<Action> GetLegalActions() const override; 
}; 

class ChancePermutationCodec : public ActionCodec {
  public:
    ChancePermutationCodec(); 
    Action Encode(const ActionVariant& action) const override; 
    ActionVariant Decode(Action action) const override; 
    std::vector<ActionVariant> GetLegalActionVariants() const override; 
    std::vector<Action> GetLegalActions() const override; 
};

class PlayerTradingCodec : public ActionCodec {
  public:
    PlayerTradingCodec(int max_contract_value, int max_contracts_per_trade); 
    Action Encode(const ActionVariant& action) const override; 
    ActionVariant Decode(Action action) const override; 
    std::vector<ActionVariant> GetLegalActionVariants() const override; 
    std::vector<Action> GetLegalActions() const override; 
}; 

// t=0, chance move: draws a uniform number [1, MaxContractValue] inclusive 
// t=1, chance move: draws another uniform number [1, MaxContractValue] inclusive 
// t=2, chance move: draws uniform "high" or "low" 
// t=3, chance move: draws customer size [-CustomerMaxSize, CustomerMaxSize]
// t=4, chance move: draws another customer size [-CustomerMaxSize, CustomerMaxSize] 
// t=5, draws a permutation of 5! for the 5 customers 
// t=6, ....: players execute in round-robin order.
//    Player observation: 
//      - order_book [p0_bid, p0_bid_sz, p1_bid, p1_bid_sz, ...] = 5 * 2 
//      - Player private info: [role\in (0, 1, 2), info]; size 2 
//    Player action: 
//      - (bid_size, bid_price, ask_size, ask_price). Max value `MaxContracValue^2 * MaxContractsPerTrade ^ 2`
//   Player order executes against market 
enum class GamePhase {
  kChanceValue1, 
  kChanceValue2, 
  kChanceHighLow, 
  kChanceCustomerSize1, 
  kChanceCustomerSize2, 
  kChancePermutation, 
  kPlayerTrading, 
}; 

class HighLowTradingActionManager {
  public:
      explicit HighLowTradingActionManager(const HighLowTradingGame* game);
      
      // Main interface methods
      Action EncodeAction(const ActionVariant& action, const HighLowTradingState& state) const;
      ActionVariant DecodeAction(Action action, const HighLowTradingState& state) const;
      std::vector<ActionVariant> GetLegalActionVariants(const HighLowTradingState& state) const;
      std::vector<Action> GetLegalActions(const HighLowTradingState& state) const;
      
      // Utility methods
      std::string ActionVariantToString(const ActionVariant& action) const;
      ActionVariant StringToActionVariant(const std::string& str, const HighLowTradingState& state) const;
  
  private:
      std::unique_ptr<ActionCodec> GetCodecForState(const HighLowTradingState& state) const;
      GamePhase DeterminePhase(const HighLowTradingState& state) const;
      
      const HighLowTradingGame* game_;
};

}  // namespace high_low_trading
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HIGH_LOW_TRADING_H_ 