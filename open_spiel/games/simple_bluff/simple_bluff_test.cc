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

#include "open_spiel/spiel.h"
#include "open_spiel/tests/basic_tests.h"

namespace open_spiel {
namespace simple_bluff {
namespace {

namespace testing = open_spiel::testing;

void BasicSimpleBluffTests() {
  testing::LoadGameTest("simple_bluff");
  testing::RandomSimTest(*LoadGame("simple_bluff"), 100);
}

void TestSpecificGame() {
  auto game = LoadGame("simple_bluff");
  auto state = game->NewInitialState();
  
  // Test initial state
  SPIEL_CHECK_TRUE(state->IsChanceNode());
  SPIEL_CHECK_EQ(state->CurrentPlayer(), kChancePlayerId);
  
  // Deal coin to player 0
  state->ApplyAction(1);  // Player 0 gets coin value 1
  SPIEL_CHECK_TRUE(state->IsChanceNode());
  
  // Deal coin to player 1  
  state->ApplyAction(0);  // Player 1 gets coin value 0
  SPIEL_CHECK_FALSE(state->IsChanceNode());
  SPIEL_CHECK_EQ(state->CurrentPlayer(), 0);  // Player 1's turn
  
  // Player 1 bets
  state->ApplyAction(1);  // Bet
  SPIEL_CHECK_EQ(state->CurrentPlayer(), 1);  // Player 2's turn
  
  // Player 2 calls
  state->ApplyAction(1);  // Call
  SPIEL_CHECK_TRUE(state->IsTerminal());
  
  // Check returns - Player 0 has higher coin (1 > 0) and should win
  auto returns = state->Returns();
  SPIEL_CHECK_GT(returns[0], 0);  // Player 0 wins
  SPIEL_CHECK_LT(returns[1], 0);  // Player 1 loses
  SPIEL_CHECK_EQ(returns[0] + returns[1], 0);  // Zero sum
}

void TestFoldScenario() {
  auto game = LoadGame("simple_bluff");
  auto state = game->NewInitialState();
  
  // Deal coins
  state->ApplyAction(0);  // Player 0 gets coin value 0
  state->ApplyAction(1);  // Player 1 gets coin value 1
  
  // Player 1 bets
  state->ApplyAction(1);  // Bet
  
  // Player 2 folds
  state->ApplyAction(0);  // Fold
  SPIEL_CHECK_TRUE(state->IsTerminal());
  
  // Player 0 should win despite having lower coin value
  auto returns = state->Returns();
  SPIEL_CHECK_GT(returns[0], 0);  // Player 0 wins
  SPIEL_CHECK_LT(returns[1], 0);  // Player 1 loses
}

void TestCheckScenario() {
  auto game = LoadGame("simple_bluff");
  auto state = game->NewInitialState();
  
  // Deal coins
  state->ApplyAction(0);  // Player 0 gets coin value 0
  state->ApplyAction(1);  // Player 1 gets coin value 1
  
  // Player 1 checks
  state->ApplyAction(0);  // Check
  SPIEL_CHECK_TRUE(state->IsTerminal());
  
  // Should go to showdown - Player 1 wins with higher coin
  auto returns = state->Returns();
  SPIEL_CHECK_LT(returns[0], 0);  // Player 0 loses
  SPIEL_CHECK_GT(returns[1], 0);  // Player 1 wins
}

}  // namespace
}  // namespace simple_bluff
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::simple_bluff::BasicSimpleBluffTests();
  open_spiel::simple_bluff::TestSpecificGame();
  open_spiel::simple_bluff::TestFoldScenario();
  open_spiel::simple_bluff::TestCheckScenario();
  return 0;
} 