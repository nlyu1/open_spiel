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

#include "open_spiel/games/black_scholes/black_scholes.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
#include <numeric>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/abseil-cpp/absl/types/span.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/observer.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_globals.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace black_scholes {

// Facts about the game
const GameType kGameType{
    /*short_name=*/"black_scholes",
    /*long_name=*/"BlackScholes",
    GameType::Dynamics::kSequential,
    GameType::ChanceMode::kExplicitStochastic,
    GameType::Information::kPerfectInformation,
    GameType::Utility::kGeneralSum, 
    GameType::RewardModel::kTerminal,
    /*min_num_players=*/1,
    /*max_num_players=*/1,
    /*provides_information_state_string=*/false,
    /*provides_information_state_tensor=*/false,
    /*provides_observation_string=*/true,
    /*provides_observation_tensor=*/true,
    /*parameter_specification=*/
    {
      {"sigma", GameParameter(kDefaultSigma)},
      {"mu", GameParameter(kDefaultMu)},
      {"delta_t", GameParameter(kDefaultDeltaT)},
      {"max_time_steps", GameParameter(kDefaultMaxTimeSteps)},
      {"max_contracts", GameParameter(kDefaultMaxContracts)},
      {"max_shares_per_contract", GameParameter(kDefaultMaxSharesPerContract)},
      {"initial_price", GameParameter(kDefaultInitialPrice)},
      {"strike_price", GameParameter(kDefaultStrikePrice)}, 
      {"premium_price", GameParameter(kDefaultPremium)}, 
      {"interest_rate", GameParameter(kDefaultInterestRate)}, 
    }
};

static std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new BlackScholesGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

RegisterSingleTensorObserver single_tensor(kGameType.short_name);

std::string BlackScholesState::ActionToString(Player player,
                                            Action move_id) const {
    if (player == kChancePlayerId) {
      switch (move_id) {
        case 0: 
          return absl::StrCat("Stock moved down to ", stock_price_);
        case 1:
          return absl::StrCat("Stock moved up to ", stock_price_);
        default:
          SpielFatalError("Invalid chance outcome");
      }
    } else {
      auto [stock_delta, contract_delta] = game_->convert_action_to_deltas(move_id);
      return absl::StrCat("Bought ", stock_delta, " stock, ", contract_delta, " option");
    }
}

std::string BlackScholesState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return ToString();
}

void BlackScholesState::ObservationTensor(Player player,
                                        absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  SPIEL_CHECK_EQ(values.size(), kStateEncodingSize);
  
  auto value_it = values.begin();
  
  // [stock_holding, cash_holding, contract_holding] + [strike_price, stock_price, premium] + [delta_t, mu, sigma, interest_rate] + [t/maxTimeSteps, maxTimeSteps]
  *value_it++ = portfolio_.stock_holding_;
  *value_it++ = portfolio_.cash_holding_;
  *value_it++ = portfolio_.contract_holding_;

  *value_it++ = game_->GetStrikePrice();
  *value_it++ = stock_price_;
  *value_it++ = game_->GetPremium();                                

  *value_it++ = game_->GetDeltaT();
  *value_it++ = game_->GetMu();
  *value_it++ = game_->GetSigma();
  *value_it++ = game_->GetInterestRate();

  *value_it++ = timestep_ / game_->GetMaxTimeSteps(); 
  *value_it++ = game_->GetMaxTimeSteps();
  
  SPIEL_CHECK_EQ(value_it, values.end());
}

BlackScholesState::BlackScholesState(std::shared_ptr<const Game> game)
    : State(game), timestep_(0), stock_price_(0.0), portfolio_() {
  game_ = static_cast<const BlackScholesGame*>(game.get()); 
  stock_price_ = game_->GetInitialPrice(); 
}

Player BlackScholesState::CurrentPlayer() const {
  SPIEL_CHECK_GE(timestep_, 0); 
  SPIEL_CHECK_LE(timestep_, game_->MaxGameLength()); 
  if (IsTerminal()) {
    return kTerminalPlayerId; 
  } else if (timestep_ % 2 == 1) {
    return kChancePlayerId; 
  } else {
    return 0; 
  }
}

void BlackScholesState::DoApplyAction(Action move) {
  if (IsChanceNode()) {
    SPIEL_CHECK_EQ(timestep_ % 2, 1); 
    double multiplier = std::exp((
      (game_->GetSigma()) * ((move == 1) ? 1 : -1) + (game_->GetMu())
    ) * game_->GetDeltaT()); 
    double interest_rate_multiplier = std::exp(game_->GetInterestRate() * game_->GetDeltaT()); 

    stock_price_ = stock_price_ * multiplier; 
    portfolio_ = Portfolio(
      portfolio_.stock_holding_, 
      portfolio_.cash_holding_ * interest_rate_multiplier, 
      portfolio_.contract_holding_
    ); 
  } else {
    // Player move
    SPIEL_CHECK_EQ(timestep_ % 2, 0); 
    SPIEL_CHECK_EQ(CurrentPlayer(), 0); 
    auto [stock_delta, contract_delta] = game_->convert_action_to_deltas(move);
    
    // Manually construct new portfolio based on the deltas
    int new_stock_holding = portfolio_.stock_holding_ + stock_delta;
    double cash_delta = -stock_delta * stock_price_ - contract_delta * game_->GetPremium();
    double new_cash_holding = portfolio_.cash_holding_ + cash_delta;
    double new_contract_holding = portfolio_.contract_holding_ + contract_delta;
    
    portfolio_ = Portfolio(new_stock_holding, new_cash_holding, new_contract_holding);
  }
  timestep_++; 
}

void BlackScholesState::UndoAction(int player, Action action) {
  if (player == kChancePlayerId) {
    // Undo environment move: reverse stock price and interest rate changes
    double multiplier = std::exp((
      (game_->GetSigma()) * ((action == 1) ? 1 : -1) + (game_->GetMu())
    ) * game_->GetDeltaT()); 
    double interest_rate_multiplier = std::exp(game_->GetInterestRate() * game_->GetDeltaT()); 
    
    stock_price_ = stock_price_ / multiplier; 
    portfolio_ = Portfolio(
      portfolio_.stock_holding_, 
      portfolio_.cash_holding_ / interest_rate_multiplier, 
      portfolio_.contract_holding_
    ); 
  } else {
    // Undo player move: reverse portfolio changes
    auto [stock_delta, contract_delta] = game_->convert_action_to_deltas(action);
    
    // Reverse the portfolio changes
    int new_stock_holding = portfolio_.stock_holding_ - stock_delta;
    double cash_delta = stock_delta * stock_price_ + contract_delta * game_->GetPremium();
    double new_cash_holding = portfolio_.cash_holding_ + cash_delta;
    double new_contract_holding = portfolio_.contract_holding_ - contract_delta;
    
    portfolio_ = Portfolio(new_stock_holding, new_cash_holding, new_contract_holding);
  }
  
  timestep_--;
}

std::vector<Action> BlackScholesState::LegalActions() const {
  if (IsChanceNode()) return LegalChanceOutcomes();
  if (IsTerminal()) return {};
  // Player moves on even timesteps, environment moves on odd timesteps
  SPIEL_CHECK_EQ(timestep_ % 2, 0); 

  // On the first step, player picks both option stock
  int max_contracts = game_->GetMaxContracts(); 
  int max_shares = game_->GetMaxSharesPerContract() * max_contracts; 
  int max_action = (timestep_ == 0) ? (2 * max_shares + 1) * (2 * max_contracts + 1) : (2 * max_shares + 1); 
  std::vector<Action> actions(max_action + 1);
  std::iota(actions.begin(), actions.end(), 0);
  return actions;
}

std::vector<std::pair<Action, double>> BlackScholesState::ChanceOutcomes() const {
  SPIEL_CHECK_TRUE(IsChanceNode());
  // Two equally likely outcomes: down (0) and up (1)
  return {{0, 0.5}, {1, 0.5}};
}

std::string BlackScholesState::ToString() const {
  std::string s = "\n";
  absl::StrAppend(
    &s, "[t=", timestep_, ", ", 
    "stock_px=", stock_price_, ", ", 
    "portfolio=", portfolio_.ToString(), 
  "]; ");
  return s;
}

bool BlackScholesState::IsTerminal() const {
  return timestep_ == game_->MaxGameLength(); 
}

std::vector<double> BlackScholesState::Returns() const {
  return {portfolio_.evaluate_payout(stock_price_, game_->GetStrikePrice(), game_->GetPremium())};
}

std::unique_ptr<State> BlackScholesState::Clone() const {
  return std::unique_ptr<State>(new BlackScholesState(*this));
}

BlackScholesGame::BlackScholesGame(const GameParameters& params) 
    : Game(kGameType, params),
      sigma_(ParameterValue<double>("sigma")),
      mu_(ParameterValue<double>("mu")),
      deltaT_(ParameterValue<double>("delta_t")),
      strike_price_(ParameterValue<double>("strike_price")),
      maxTimeSteps_(ParameterValue<int>("max_time_steps")),
      maxContracts_(ParameterValue<int>("max_contracts")),
      maxSharesPerContract_(ParameterValue<int>("max_shares_per_contract")), 
      initialPrice_(ParameterValue<double>("initial_price")),
      premium_(ParameterValue<double>("premium_price")),
      interest_rate_(ParameterValue<double>("interest_rate")) {
  maxShares_ = maxSharesPerContract_ * maxContracts_; 
}

double BlackScholesGame::MaxUtility() const {
  double true_mu = mu_ * deltaT_ * maxTimeSteps_; 
  double true_sigma = sigma_ * deltaT_ * maxTimeSteps_; 
  double stock_mu = std::exp(true_mu + true_sigma * true_sigma / 2); 
  double stock_sigma = std::exp(2 * true_mu + true_sigma * true_sigma) * (
    std::exp(true_sigma * true_sigma) - 1); 
  return maxShares_ * maxContracts_ * (stock_mu + 3 * stock_sigma) * 10; 
}

std::string Portfolio::ToString() const {
  return absl::StrCat(
      "(stock=", absl::StrFormat("%.3f", stock_holding_),
      ", cash=", absl::StrFormat("%.3f", cash_holding_),
      ", contract=", absl::StrFormat("%.3f", contract_holding_), ")");
}

Portfolio Portfolio::finance_stock(float stock_delta, float stock_price) const {
  return Portfolio(stock_holding_ + stock_delta, cash_holding_ - stock_delta * stock_price, contract_holding_);
}

double Portfolio::evaluate_payout(double stock_price, double strike_price, double premium) const {
  // Calculate total portfolio value: stock value + cash + option payoff
  double stock_value = stock_holding_ * stock_price;
  double option_payoff = contract_holding_ * std::max(0.0, stock_price - strike_price); 
  return stock_value + cash_holding_ + option_payoff;
}

std::pair<int, int> BlackScholesGame::convert_action_to_deltas(Action action_id) const {
  int num_shares_purchased = action_id % (2 * maxShares_ + 1) - maxShares_; 
  int contract_rawnum = action_id / (2 * maxShares_ + 1); 
  // We use the mapping (0, -1, 1, -2, 2 ...) -> (0, 1, 2, 3, 4, 5)
  int num_contracts_purchased = (contract_rawnum % 2 == 0) ? (contract_rawnum / 2) : -(contract_rawnum + 1) / 2; 
  SPIEL_CHECK_LE(num_shares_purchased, maxShares_); 
  SPIEL_CHECK_LE(num_contracts_purchased, maxContracts_); 
  SPIEL_CHECK_GE(num_shares_purchased, -maxShares_); 
  SPIEL_CHECK_GE(num_contracts_purchased, -maxContracts_); 
  return std::make_pair(num_shares_purchased, num_contracts_purchased);
}

int BlackScholesGame::NumDistinctActions() const {
  return maxShares_ * maxContracts_; 
}

}  // namespace black_scholes
}  // namespace open_spiel
