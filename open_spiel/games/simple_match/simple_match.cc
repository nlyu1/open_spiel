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

// Sample pipeline: 
//    ./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=true --build_only=true && build/examples/example --game=simple_match

// ./open_spiel/scripts/build_and_run_tests.sh --virtualenv=false --install=true --build_only=true && python3 open_spiel/python/examples/mcts.py --game=simple_match --player1=human --player2=mcts

#include "open_spiel/games/simple_match/simple_match.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <iostream>

#include "open_spiel/spiel_utils.h"
#include "open_spiel/utils/tensor_view.h"

namespace open_spiel {
namespace simple_match {
namespace {

// Facts about the game.
const GameType kGameType{
    /*short_name=*/"simple_match",
    /*long_name=*/"Simple match",
    GameType::Dynamics::kSequential,
    GameType::ChanceMode::kDeterministic,
    GameType::Information::kPerfectInformation,
    GameType::Utility::kZeroSum,
    GameType::RewardModel::kTerminal,
    /*max_num_players=*/2,
    /*min_num_players=*/2,
    /*provides_information_state_string=*/true,
    /*provides_information_state_tensor=*/false,
    /*provides_observation_string=*/true,
    /*provides_observation_tensor=*/true,
    /*parameter_specification=*/{}  // no parameters
};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new SimpleMatchGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

RegisterSingleTensorObserver single_tensor(kGameType.short_name);

}  // namespace

ChoiceState ActionToState(Action action) {
  switch (action) {
    case 0: 
      return ChoiceState::kHeads; 
    case 1: 
      return ChoiceState::kTails; 
    default:
      SpielFatalError(absl::StrCat("Invalid action ", action));
      return ChoiceState::kEmpty; 
  }
}

std::string StateToString(ChoiceState state) {
  switch (state) {
    case ChoiceState::kHeads: 
      return "x";
    case ChoiceState::kTails: 
      return "o";
    case ChoiceState::kEmpty: 
      return "-";
    default:
      SpielFatalError("Unknown state.");
  }
}


void SimpleMatchState::DoApplyAction(Action action) {
  SPIEL_CHECK_EQ(player_choices_[num_moves_], ChoiceState::kEmpty);
  player_choices_[num_moves_] = ActionToState(action);
  current_player_ = 1 - current_player_;
  num_moves_ += 1;
}

std::vector<Action> SimpleMatchState::LegalActions() const {
  if (IsTerminal()) return {};
  return {0, 1}; 
}

std::string SimpleMatchState::ActionToString(Player player,
                                           Action action_id) const {
  return game_->ActionToString(player, action_id);
}

SimpleMatchState::SimpleMatchState(std::shared_ptr<const Game> game) : State(game) {
  std::fill(begin(player_choices_), end(player_choices_), ChoiceState::kEmpty);
}

std::string SimpleMatchState::ToString() const {
  std::string str;
  for (int j=0; j<num_moves_; j++) {
    absl::StrAppend(&str, "(", j%kNumPlayers, ",", StateToString(player_choices_[j]), ") "); 
  }
  absl::StrAppend(&str, "\n");
  return str;
}

bool SimpleMatchState::IsTerminal() const {
  return num_moves_ == maxRounds * kNumPlayers; 
}

std::vector<double> SimpleMatchState::Returns() const {
  double num_equals = 0; 
  for (int j=0; j<maxRounds; j++) {
    num_equals += (player_choices_[kNumPlayers*j] == player_choices_[kNumPlayers*j+1] ? 1 : 0);
  }
  std::cout << "Num equals: " << num_equals << std::endl;
  return {-num_equals, num_equals}; 
}

std::string SimpleMatchState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return HistoryString();
}

std::string SimpleMatchState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return ToString();
}

void SimpleMatchState::ObservationTensor(Player player,
                                         absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, kNumPlayers);

  int tensor_size = maxRounds * kNumPlayers;

  for (int i = 0; i < tensor_size; i++) {
    float state_value = 0.0f;
    switch (player_choices_[i]) {
      case ChoiceState::kEmpty:
        state_value = 0.0f;
        break;
      case ChoiceState::kHeads:
        state_value = -1.0f;
        break;
      case ChoiceState::kTails:
        state_value = 1.0f;
        break;
      default:
        state_value = 0.0f; 
        break;
    }
    values[i] = state_value;
  }
}

void SimpleMatchState::UndoAction(Player player, Action move) {
  player_choices_[move] = ChoiceState::kEmpty;
  current_player_ = player;
  num_moves_ -= 1;
  history_.pop_back();
  --move_number_;
}

std::unique_ptr<State> SimpleMatchState::Clone() const {
  return std::unique_ptr<State>(new SimpleMatchState(*this));
}

std::string SimpleMatchGame::ActionToString(Player player,
  Action action_id) const {
return absl::StrCat(player, ":", action_id);
}

SimpleMatchGame::SimpleMatchGame(const GameParameters& params)
    : Game(kGameType, params) {}

} 
}  // namespace open_spiel
