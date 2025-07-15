#include "high_low_trading.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"
#include "market.h"
#include "action_manager.h" 

namespace open_spiel {
namespace high_low_trading {
namespace {

// Default parameters
constexpr double kAnte = 1.0;

// Facts about the game
const GameType kGameType{/*short_name=*/"high_low_trading",
                         /*long_name=*/"High Low Trading",
                         GameType::Dynamics::kSequential,
                         GameType::ChanceMode::kExplicitStochastic,
                         GameType::Information::kImperfectInformation,
                         GameType::Utility::kZeroSum,
                         GameType::RewardModel::kTerminal,
                         /*max_num_players=*/10,
                         /*min_num_players=*/4,
                         /*provides_information_state_string=*/true,
                         /*provides_information_state_tensor=*/true,
                         /*provides_observation_string=*/true,
                         /*provides_observation_tensor=*/true,
                         /*parameter_specification=*/{
                          {"steps_per_player", GameParameter(kDefaultStepsPerPlayer)},
                          {"max_contracts_per_trade", GameParameter(kDefaultMaxContractsPerTrade)},
                          {"customer_max_size", GameParameter(kDefaultCustomerMaxSize)},
                          {"max_contract_value", GameParameter(kDefaultMaxContractValue)},
                          {"num_players", GameParameter(kDefaultNumPlayers)},
                         },
                         /*default_loadable=*/true,
                         /*provides_factored_observation_string=*/false};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new HighLowTradingGame(params));
}
 
REGISTER_SPIEL_GAME(kGameType, Factory);

}  // namespace

HighLowTradingState::HighLowTradingState(std::shared_ptr<const Game> game)
    : State(game),
      contract_values_{ChanceContractValueAction(0), ChanceContractValueAction(0)},
      contract_high_settle_(false),
      player_permutation_(std::vector<PlayerRole>(NumPlayers(), PlayerRole::kCustomer), 
                         std::vector<int>(NumPlayers(), 0)),
      player_target_positions_(NumPlayers(), 0),
      player_positions_(NumPlayers()),
      market_() {
    // Initialize containers
    order_fills_.clear();
    player_quotes_.clear();
}

Player HighLowTradingState::CurrentPlayer() const {
  if (IsTerminal()) {
    return kTerminalPlayerId;
  } 
  int move_number = MoveNumber();
  if (move_number < game_->MaxChanceNodesInHistory()) {
    return kChancePlayerId; 
  } else {
    return (move_number - game_->MaxChanceNodesInHistory()) % NumPlayers(); 
  }
  return kTerminalPlayerId;
}

void HighLowTradingState::DoApplyAction(Action move) {
  int move_number = MoveNumber(); 
  auto action_manager = GetActionManager(); 
  auto game_phase = action_manager.game_phase_of_timestep(move_number); 
  SPIEL_CHECK_LT(move_number, game_->MaxGameLength()); 
  SPIEL_CHECK_LE(move, action_manager.valid_action_range(game_phase).second); 

  auto structured_action = action_manager.RawToStructuredAction(move_number, move); 

  if (move_number < 2) {
    contract_values_[move_number] = std::get<ChanceContractValueAction>(structured_action).contract_value_; 
  } else if (move_number == 2) {
    contract_high_settle_ = std::get<ChanceHighLowAction>(structured_action).is_high_; 
  } else if (move_number == 3) {
    player_permutation_ = std::get<ChancePermutationAction>(structured_action); 
  } else if (move_number < game_->MaxChanceNodesInHistory()) {
    auto target_position = std::get<ChanceCustomerSizeAction>(structured_action).customer_size_; 
    // The post-permuted player id of customer 
    int customer_player_id = player_permutation_.permutation_ [move_number - 4 + 3]; 
    player_target_positions_[customer_player_id] = target_position; 
  } else {
    trade_matching::customerId customer_id = static_cast<trade_matching::customerId>(CurrentPlayer()); 
    auto customer_quote = std::get<PlayerQuoteAction>(structured_action); 
    player_quotes_.push_back(std::make_pair(CurrentPlayer(), customer_quote)); 

    auto fills = market_.AddOrder(trade_matching::OrderEntry(customer_quote.bid_price_, customer_quote.bid_size_, move_number, customer_id, true)); 
    auto ask_fills = market_.AddOrder(trade_matching::OrderEntry(customer_quote.ask_price_, customer_quote.ask_size_, move_number, customer_id, false)); 

    fills.insert(fills.end(), ask_fills.begin(), ask_fills.end()); 
    // Merge fills 
    order_fills_.insert(order_fills_.end(), fills.begin(), fills.end()); 
    // Adjust customer positions based on fills: 
    for (auto fill : fills) {
      if (fill.is_sell_quote) {
        // Customer hit a sell quote (customer is buying)
        player_positions_[fill.customer_id].num_contracts += fill.size; 
        player_positions_[fill.customer_id].cash_balance -= fill.price * fill.size; 
        player_positions_[fill.quoter_id].num_contracts -= fill.size; 
        player_positions_[fill.quoter_id].cash_balance += fill.price * fill.size; 
      } else {
        // Customer hit a buy quote (customer is selling)
        player_positions_[fill.customer_id].num_contracts -= fill.size; 
        player_positions_[fill.customer_id].cash_balance += fill.price * fill.size; 
        player_positions_[fill.quoter_id].num_contracts += fill.size; 
        player_positions_[fill.quoter_id].cash_balance -= fill.price * fill.size; 
      }
    }
  }
}

std::vector<Action> HighLowTradingState::LegalActions() const {
  if (IsTerminal()) return {}; 
  auto [min_action, max_action] = GetActionManager().valid_action_range(GetActionManager().game_phase_of_timestep(MoveNumber())); 
  std::vector<Action> actions; 
  for (int action = min_action; action <= max_action; ++action) {
    actions.push_back(action); 
  }
  return actions; 
}

std::string PlayerPosition::ToString() const {
  return absl::StrCat("[", num_contracts, " contracts, ", cash_balance, " cash]"); 
}

std::string HighLowTradingState::ActionToString(Player player, Action move) const {
  auto structured_action = GetActionManager().RawToStructuredAction(MoveNumber(), move); 
  return absl::StrCat("Player ", player, " ", ActionVariantToString(structured_action)); 
}

const HighLowTradingGame* HighLowTradingState::GetGame() const {
  return static_cast<const HighLowTradingGame*>(game_.get()); 
}

const ActionManager& HighLowTradingState::GetActionManager() const {
  return GetGame()->GetActionManager();
}

std::string HighLowTradingState::ToString() const {
  std::string result; 
  absl::StrAppend(&result, "Contract values: ", contract_values_[0].contract_value_, ", ", contract_values_[1].contract_value_, "\n"); 
  absl::StrAppend(&result, "Contract high settle: ", contract_high_settle_.is_high_ ? "High" : "Low", "\n"); 
  absl::StrAppend(&result, "Player permutation: ", player_permutation_.ToString(), "\n"); 
  for (int i = 0; i < NumPlayers(); ++i) {
    auto target_position = player_target_positions_[i]; 
    if (target_position == 0) {
      absl::StrAppend(&result, "Player ", i, " target position: No requirement\n"); 
    } else {
      absl::StrAppend(&result, "Player ", i, " target position: ", target_position, "\n"); 
    }
  }
  for (auto quote : player_quotes_) {
    absl::StrAppend(&result, "Player ", quote.first, " quote: ", quote.second.ToString(), "\n"); 
  }
  for (auto fill : order_fills_) {
    absl::StrAppend(&result, "Order fill: ", fill.ToString(), "\n"); 
  }
  for (int i = 0; i < NumPlayers(); ++i) {
    absl::StrAppend(&result, "Player ", i, " position: ", player_positions_[i].ToString(), "\n"); 
  }
  return result; 
}

bool HighLowTradingState::IsTerminal() const {
  return MoveNumber() > game_->MaxGameLength(); 
}

int HighLowTradingState::GetContractValue() const {
  SPIEL_CHECK_GE(MoveNumber(), 3); 
  if (contract_high_settle_.is_high_) {
    return std::max(contract_values_[1].contract_value_, contract_values_[0].contract_value_); 
  } else {
    return std::min(contract_values_[1].contract_value_, contract_values_[0].contract_value_); 
  }
}

std::vector<double> HighLowTradingState::Returns() const {
  std::vector<double> returns; 
  double contract_value = GetContractValue(); 
  for (int j=0; j<NumPlayers(); ++j) {
    double player_return = player_positions_[j].cash_balance + player_positions_[j].num_contracts * contract_value; 
    if (player_target_positions_[j] != 0) {
      // Customers are additionally evaluated on whether they're able to obtain their target position. 
      int position_diff = player_target_positions_[j] - player_positions_[j].num_contracts; 
      player_return -= (position_diff > 0 ? position_diff : -position_diff) * GetGame()->GetMaxContractValue(); 
    }
    returns.push_back(player_return); 
  }
  return returns; 
}

std::unique_ptr<State> HighLowTradingState::Clone() const {
  return std::unique_ptr<State>(new HighLowTradingState(*this));
}

void HighLowTradingState::UndoAction(Player player, Action move) {
  // if (history_.size() <= 2) {
  //   // Undoing coin deals
  //   player_values_[history_.size() - 1] = -1;
  // } else if (history_.size() == 3) {
  //   // Undoing Player 1's action
  //   if (player1_bet_) {
  //     pot_ -= kAnte;
  //     contributions_[0] -= kAnte;
  //   }
  //   player1_bet_ = false;
  // } else if (history_.size() == 4) {
  //   // Undoing Player 2's action
  //   if (player2_accept_) {
  //     pot_ -= kAnte;
  //     contributions_[1] -= kAnte;
  //   }
  //   player2_accept_ = false;
  // }
  
  // winner_ = kInvalidPlayer;
  history_.pop_back();
  --move_number_;
}

std::vector<std::pair<Action, double>> HighLowTradingState::ChanceOutcomes() const {
  SPIEL_CHECK_TRUE(IsChanceNode());
  return {{0, 0.5}, {1, 0.5}};  // Equal probability for each coin outcome
}

std::unique_ptr<State> HighLowTradingState::ResampleFromInfostate(
    int player_id, std::function<double()> rng) const {
  std::unique_ptr<State> state = game_->NewInitialState();
  
  // // Deal the same coin to the resampling player
  // state->ApplyAction(player_values_[0]);
  // state->ApplyAction(player_values_[1]);
  
  // // Sample the opponent's coin
  // if (player_id == 0) {
  //   // Keep player 0's coin, resample player 1's
  //   state = game_->NewInitialState();
  //   state->ApplyAction(player_values_[0]);
  //   Action other_coin = (rng() < 0.5) ? 0 : 1;
  //   state->ApplyAction(other_coin);
  // } else {
  //   // Keep player 1's coin, resample player 0's
  //   state = game_->NewInitialState();
  //   Action other_coin = (rng() < 0.5) ? 0 : 1;
  //   state->ApplyAction(other_coin);
  //   state->ApplyAction(player_values_[1]);
  // }
  
  // // Apply the observed public actions
  // for (int i = 2; i < history_.size(); ++i) {
  //   state->ApplyAction(history_[i].action);
  // }
  return state;
}

std::vector<int> HighLowTradingGame::InformationStateTensorShape() const {
  return {3};
}

std::vector<int> HighLowTradingGame::ObservationTensorShape() const {
  // Same as information state for this simple game
  return {3};
}

void HighLowTradingState::ObservationTensor(Player player,
  absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, GetGame()->NumPlayers());
  
  std::fill(values.begin(), values.end(), 0);
}

void HighLowTradingState::InformationStateTensor(Player player,
  absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, GetGame()->NumPlayers());
  
  std::fill(values.begin(), values.end(), 0);
}

std::string HighLowTradingState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, 2);
  
  std::string result = absl::StrCat("placeholder");
  return result; 
}

std::string HighLowTradingState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, GetGame()->NumPlayers());
  
  std::string result;  
  return result;
}

HighLowTradingGame::HighLowTradingGame(const GameParameters& params)
    : Game(kGameType, params),
      action_manager_(Config(
        ParameterValue<int>("steps_per_player"), 
        ParameterValue<int>("max_contracts_per_trade"),
        ParameterValue<int>("customer_max_size"),
        ParameterValue<int>("max_contract_value"),
        ParameterValue<int>("num_players")
      )) {
}

std::unique_ptr<State> HighLowTradingGame::NewInitialState() const {
  return std::unique_ptr<State>(new HighLowTradingState(shared_from_this()));
}

int HighLowTradingGame::MaxChanceOutcomes() const {
  return std::max({
    GetActionManager().valid_action_range(GamePhase::kChanceValue).second + 1, 
    GetActionManager().valid_action_range(GamePhase::kChanceHighLow).second + 1, 
    GetActionManager().valid_action_range(GamePhase::kChancePermutation).second + 1, 
    GetActionManager().valid_action_range(GamePhase::kCustomerSize).second + 1, 
  }) + 1; 
}

int HighLowTradingGame::NumDistinctActions() const {
  return GetActionManager().valid_action_range(GamePhase::kPlayerTrading).second + 1; 
}

}  // namespace high_low_trading
}  // namespace open_spiel