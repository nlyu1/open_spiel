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

#ifndef OPEN_SPIEL_GAMES_SIMPLE_MATCH_H_
#define OPEN_SPIEL_GAMES_SIMPLE_MATCH_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "open_spiel/json/include/nlohmann/json.hpp"
#include "open_spiel/spiel.h"
#include "open_spiel/pybind11_json/include/pybind11_json/pybind11_json.hpp"

// Simple game of Noughts and Crosses:
//
// Parameters: none

namespace open_spiel {
namespace simple_match {

// Constants.
inline constexpr int kNumPlayers = 2;
inline constexpr int maxRounds = 5; 

enum class ChoiceState {
  kEmpty,
  kHeads, 
  kTails, 
};

// State of an in-play game.
class SimpleMatchState : public State {
 public:
  SimpleMatchState(std::shared_ptr<const Game> game);

  SimpleMatchState(const SimpleMatchState&) = default;
  SimpleMatchState& operator=(const SimpleMatchState&) = default;

  Player CurrentPlayer() const override {
    return IsTerminal() ? kTerminalPlayerId : current_player_;
  }
  std::string ActionToString(Player player, Action action_id) const override;
  std::string ToString() const override;
  bool IsTerminal() const override;
  std::vector<double> Returns() const override;
  std::string InformationStateString(Player player) const override;
  std::string ObservationString(Player player) const override;
  void ObservationTensor(Player player,
                         absl::Span<float> values) const override;
  std::unique_ptr<State> Clone() const override;
  void UndoAction(Player player, Action move) override;
  std::vector<Action> LegalActions() const override;
  void ChangePlayer() { current_player_ = current_player_ == 0 ? 1 : 0; }

  void SetCurrentPlayer(Player player) { current_player_ = player; }

 protected:
  void DoApplyAction(Action move) override;

  std::array<ChoiceState, maxRounds*kNumPlayers> player_choices_; 

 private:         
  Player current_player_ = 0;     
  int num_moves_ = 0;
};

// Game object.
class SimpleMatchGame : public Game {
 public:
  explicit SimpleMatchGame(const GameParameters& params);
  int NumDistinctActions() const override { return 2; }
  std::unique_ptr<State> NewInitialState() const override {
    return std::unique_ptr<State>(new SimpleMatchState(shared_from_this()));
  }
  int NumPlayers() const override { return kNumPlayers; }
  double MinUtility() const override { return -maxRounds; }
  absl::optional<double> UtilitySum() const override { return 0; }
  double MaxUtility() const override { return maxRounds; }
  std::vector<int> ObservationTensorShape() const override {
    return {maxRounds * kNumPlayers};
  }
  // int MaxGameLength() const override { return kNumCells; }
  int MaxGameLength() const override { return maxRounds*kNumPlayers; }
  std::string ActionToString(Player player, Action action_id) const override;
};

std::string StateToString(ChoiceState state) ; 

inline std::ostream& operator<<(std::ostream& stream, const ChoiceState& state) {
  return stream << StateToString(state);
}

} 
}  // namespace open_spiel

#endif
