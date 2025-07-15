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

#ifndef OPEN_SPIEL_GAMES_SIMPLE_BLUFF_H_
#define OPEN_SPIEL_GAMES_SIMPLE_BLUFF_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

// A simple bluffing game with imperfect information
//
// Game mechanics:
// 1. Two players, each receives a random coin flip (0 or 1) from environment
// 2. Player 1 can choose to "check" or "bet"
//    - If check: go to showdown with pot size 2 (ante from each player)
//    - If bet: Player 2 can "fold" (Player 1 wins pot) or "call" (showdown with pot size 4)
// 3. In showdown: player with higher coin value wins the pot, ties split the pot
// 4. Each player starts with ante of 1, betting adds 1 more
//
// Internal state: [player1_value, player2_value, player1_bets, player2_accepts]
// Player 1 observation: [player1_value]
// Player 2 observation: [player2_value, player1_bets]

namespace open_spiel {
namespace simple_bluff {

enum ActionType { kCheck = 0, kBet = 1, kFold = 0, kCall = 1 };

class SimpleBluffGame;

class SimpleBluffState : public State {
 public:
  explicit SimpleBluffState(std::shared_ptr<const Game> game);
  SimpleBluffState(const SimpleBluffState&) = default;

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

class SimpleBluffGame : public Game {
 public:
  explicit SimpleBluffGame(const GameParameters& params);
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
};

}  // namespace simple_bluff
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_SIMPLE_BLUFF_H_ 