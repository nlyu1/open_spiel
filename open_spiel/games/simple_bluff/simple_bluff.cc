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

#include "open_spiel/games/simple_bluff/simple_bluff.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace simple_bluff {

// Default parameters
constexpr double kAnte = 1.0;

// Facts about the game
const GameType kGameType{/*short_name=*/"simple_bluff",
                         /*long_name=*/"Simple Bluff",
                         GameType::Dynamics::kSequential,
                         GameType::ChanceMode::kExplicitStochastic,
                         GameType::Information::kImperfectInformation,
                         GameType::Utility::kZeroSum,
                         GameType::RewardModel::kTerminal,
                         /*max_num_players=*/2,
                         /*min_num_players=*/2,
                         /*provides_information_state_string=*/true,
                         /*provides_information_state_tensor=*/true,
                         /*provides_observation_string=*/true,
                         /*provides_observation_tensor=*/true,
                         /*parameter_specification=*/{},
                         /*default_loadable=*/true,
                         /*provides_factored_observation_string=*/false};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new SimpleBluffGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);
RegisterSingleTensorObserver single_tensor(kGameType.short_name);

SimpleBluffState::SimpleBluffState(std::shared_ptr<const Game> game)
    : State(game),
      player_values_{-1, -1},
      player1_bet_(false),
      player2_accept_(false),
      winner_(kInvalidPlayer),
      pot_(2 * kAnte),
      contributions_{kAnte, kAnte} {}

Player SimpleBluffState::CurrentPlayer() const {
  if (IsTerminal()) {
    return kTerminalPlayerId;
  }
  
  // First deal coins to both players
  if (history_.size() < 2) {
    return kChancePlayerId;
  }
  
  // Player 1 acts first (after coins are dealt)
  if (history_.size() == 2) {
    return 0;
  }
  
  // If Player 1 bet, Player 2 must respond
  if (history_.size() == 3 && player1_bet_) {
    return 1;
  }
  
  // Game should be terminal by now
  return kTerminalPlayerId;
}

void SimpleBluffState::DoApplyAction(Action move) {
  if (history_.size() < 2) {
    // Dealing coins to players
    player_values_[history_.size()] = move;
  } else if (history_.size() == 2) {
    // Player 1's action
    player1_bet_ = (move == ActionType::kBet);
    if (player1_bet_) {
      pot_ += kAnte;
      contributions_[0] += kAnte;
    } else {
      // Player 1 checked, go to showdown
      if (player_values_[0] > player_values_[1]) {
        winner_ = 0;
      } else if (player_values_[1] > player_values_[0]) {
        winner_ = 1;
      } else {
        // Tie - split pot
        winner_ = kInvalidPlayer;
      }
    }
  } else if (history_.size() == 3) {
    // Player 2's response to Player 1's bet
    player2_accept_ = (move == ActionType::kCall);
    if (player2_accept_) {
      pot_ += kAnte;
      contributions_[1] += kAnte;
      // Go to showdown
      if (player_values_[0] > player_values_[1]) {
        winner_ = 0;
      } else if (player_values_[1] > player_values_[0]) {
        winner_ = 1;
      } else {
        // Tie - split pot
        winner_ = kInvalidPlayer;
      }
    } else {
      // Player 2 folded, Player 1 wins
      winner_ = 0;
    }
  }
}

std::vector<Action> SimpleBluffState::LegalActions() const {
  if (IsTerminal()) return {};
  if (IsChanceNode()) {
    return {0, 1};  // Coin flip outcomes
  } else {
    return {0, 1};  // Check/Bet for Player 1, Fold/Call for Player 2
  }
}

std::string SimpleBluffState::ActionToString(Player player, Action move) const {
  if (player == kChancePlayerId) {
    return absl::StrCat("Coin:", move);
  } else if (history_.size() == 2) {
    // Player 1's action
    return move == ActionType::kCheck ? "Check" : "Bet";
  } else {
    // Player 2's action
    return move == ActionType::kFold ? "Fold" : "Call";
  }
}

std::string SimpleBluffState::ToString() const {
  std::string str;
  
  // Show coin values if dealt
  if (history_.size() >= 2) {
    absl::StrAppend(&str, "Coins: ", player_values_[0], ",", player_values_[1]);
  }
  
  // Show actions
  if (history_.size() > 2) {
    absl::StrAppend(&str, " P1:", player1_bet_ ? "Bet" : "Check");
    if (history_.size() > 3) {
      absl::StrAppend(&str, " P2:", player2_accept_ ? "Call" : "Fold");
    }
  }
  
  if (IsTerminal()) {
    absl::StrAppend(&str, " Winner:", winner_);
  }
  
  return str;
}

bool SimpleBluffState::IsTerminal() const {
  // Terminal after Player 1 checks (showdown)
  if (history_.size() == 3 && !player1_bet_) {
    return true;
  }
  // Terminal after Player 2 responds to Player 1's bet
  if (history_.size() == 4) {
    return true;
  }
  return false;
}

std::vector<double> SimpleBluffState::Returns() const {
  if (!IsTerminal()) {
    return {0.0, 0.0};
  }

  std::vector<double> returns(2);
  
  if (winner_ == kInvalidPlayer) {
    // Tie - split pot
    returns[0] = 0.0;
    returns[1] = 0.0;
  } else {
    // Winner takes pot minus their contribution
    returns[winner_] = pot_ - contributions_[winner_];
    returns[1 - winner_] = -contributions_[1 - winner_];
  }
  
  return returns;
}

std::string SimpleBluffState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, 2);
  
  std::string result = absl::StrCat(player_values_[player]);
  
  // Add public history that this player can observe
  if (history_.size() > 2) {
    absl::StrAppend(&result, player1_bet_ ? "b" : "c");
  }
  
  return result;
}

std::string SimpleBluffState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, 2);
  
  std::string result;
  
  // Player's own coin value
  if (history_.size() > player) {
    absl::StrAppend(&result, player_values_[player]);
  }
  
  // Public information visible to this player
  if (player == 1 && history_.size() > 2) {
    absl::StrAppend(&result, player1_bet_ ? "b" : "c");
  }
  
  return result;
}

void SimpleBluffState::InformationStateTensor(Player player,
                                              absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, 2);
  
  std::fill(values.begin(), values.end(), 0);
  
  if (player == 0) {
    // Player 1 observation: [player1_value (one-hot, 2 pos), unused (1 pos)]
    if (player_values_[0] != -1) {
      values[player_values_[0]] = 1.0;
    }
    // Position 2 remains 0 (unused for Player 1)
  } else {
    // Player 2 observation: [player2_value (one-hot, 2 pos), player1_bets (1 pos)]
    // First 2 positions for player2_value (one-hot)
    if (player_values_[1] != -1) {
      values[player_values_[1]] = 1.0;
    }
    // Position 2 for whether player1 bet (if player1 has acted)
    if (history_.size() > 2) {
      values[2] = player1_bet_ ? 1.0 : 0.0;
    }
  }
}

void SimpleBluffState::ObservationTensor(Player player,
                                         absl::Span<float> values) const {
  // For this simple game, observation is the same as information state
  InformationStateTensor(player, values);
}

std::unique_ptr<State> SimpleBluffState::Clone() const {
  return std::unique_ptr<State>(new SimpleBluffState(*this));
}

void SimpleBluffState::UndoAction(Player player, Action move) {
  if (history_.size() <= 2) {
    // Undoing coin deals
    player_values_[history_.size() - 1] = -1;
  } else if (history_.size() == 3) {
    // Undoing Player 1's action
    if (player1_bet_) {
      pot_ -= kAnte;
      contributions_[0] -= kAnte;
    }
    player1_bet_ = false;
  } else if (history_.size() == 4) {
    // Undoing Player 2's action
    if (player2_accept_) {
      pot_ -= kAnte;
      contributions_[1] -= kAnte;
    }
    player2_accept_ = false;
  }
  
  winner_ = kInvalidPlayer;
  history_.pop_back();
  --move_number_;
}

std::vector<std::pair<Action, double>> SimpleBluffState::ChanceOutcomes() const {
  SPIEL_CHECK_TRUE(IsChanceNode());
  return {{0, 0.5}, {1, 0.5}};  // Equal probability for each coin outcome
}

std::unique_ptr<State> SimpleBluffState::ResampleFromInfostate(
    int player_id, std::function<double()> rng) const {
  SPIEL_CHECK_GE(player_id, 0);
  SPIEL_CHECK_LT(player_id, 2);
  
  std::unique_ptr<State> state = game_->NewInitialState();
  
  if (player_id == 0) {
    // Keep player 0's coin, resample player 1's coin
    state->ApplyAction(player_values_[0]);  // Keep player 0's coin
    Action other_coin = (rng() < 0.5) ? 0 : 1;  // Resample player 1's coin
    state->ApplyAction(other_coin);
  } else {
    // Keep player 1's coin, resample player 0's coin  
    Action other_coin = (rng() < 0.5) ? 0 : 1;  // Resample player 0's coin
    state->ApplyAction(other_coin);
    state->ApplyAction(player_values_[1]);  // Keep player 1's coin
  }
  
  // Apply all observed public actions (maintaining exact same public history)
  for (int i = 2; i < history_.size(); ++i) {
    state->ApplyAction(history_[i].action);
  }
  
  return state;
}

SimpleBluffGame::SimpleBluffGame(const GameParameters& params)
    : Game(kGameType, params) {}

std::unique_ptr<State> SimpleBluffGame::NewInitialState() const {
  return std::unique_ptr<State>(new SimpleBluffState(shared_from_this()));
}

std::vector<int> SimpleBluffGame::InformationStateTensorShape() const {
  // Player observations: max([player_value(2), player1_bets(1)]) = 3 total
  return {3};
}

std::vector<int> SimpleBluffGame::ObservationTensorShape() const {
  // Same as information state for this simple game
  return {3};
}

}  // namespace simple_bluff
}  // namespace open_spiel