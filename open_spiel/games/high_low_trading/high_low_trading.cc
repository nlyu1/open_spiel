#include "high_low_trading.h"

#include <algorithm>
#include <array>
#include <cmath>
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
                          {"players", GameParameter(kDefaultNumPlayers)},
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
  
  // Debug output to understand the issue
  auto [min_action, max_action] = action_manager.valid_action_range(game_phase);
  // std::cout << "move_number=" << move_number << ", game_phase=" << game_phase 
  //   << ", range=[" << min_action << "," << max_action << "]" << std::endl; 
  if (max_action < 0) {
    SpielFatalError(absl::StrCat(
        "Invalid action range for move_number=", move_number, 
        ", game_phase=", static_cast<int>(game_phase),
        ", range=[", min_action, ",", max_action, "]",
        ", num_players=", GetGame()->GetNumPlayers(),
        ", max_contract_value=", GetGame()->GetMaxContractValue(),
        ", customer_max_size=", GetGame()->GetCustomerMaxSize()));
  }
  
  SPIEL_CHECK_LE(move, max_action); 

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
    int customer_player_id = player_permutation_.permutation_[move_number - 4 + 3]; 
    player_target_positions_[customer_player_id] = target_position; 
  } else {
    trade_matching::customerId customer_id = static_cast<trade_matching::customerId>(CurrentPlayer()); 
    auto customer_quote = std::get<PlayerQuoteAction>(structured_action); 
    player_quotes_.push_back(std::make_pair(CurrentPlayer(), customer_quote)); 

    auto fills = market_.AddOrder(trade_matching::OrderEntry(customer_quote.bid_price_, customer_quote.bid_size_, 2 * move_number, customer_id, true)); 
    auto ask_fills = market_.AddOrder(trade_matching::OrderEntry(customer_quote.ask_price_, customer_quote.ask_size_, 2 * move_number + 1, customer_id, false)); 

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

std::string HighLowTradingState::PublicInformationString() const {
  std::string result; 
  
  absl::StrAppend(&result, "********** Game Configuration **********\n"); 
  absl::StrAppend(&result, "Steps per player: ", GetGame()->GetStepsPerPlayer(), "\n");
  absl::StrAppend(&result, "Max contracts per trade: ", GetGame()->GetMaxContractsPerTrade(), "\n");
  absl::StrAppend(&result, "Customer max size: ", GetGame()->GetCustomerMaxSize(), "\n");
  absl::StrAppend(&result, "Max contract value: ", GetGame()->GetMaxContractValue(), "\n");
  absl::StrAppend(&result, "Number of players: ", GetGame()->GetNumPlayers(), "\n");
  absl::StrAppend(&result, "****************************************\n\n"); 
  
  absl::StrAppend(&result, "********** Quote & Fills **********\n"); 
  for (auto quote : player_quotes_) {
    absl::StrAppend(&result, "Player ", quote.first, " quote: ", quote.second.ToString(), "\n"); 
  }
  for (auto fill : order_fills_) {
    absl::StrAppend(&result, "Order fill: ", fill.ToString(), "\n"); 
  }
  absl::StrAppend(&result, "***********************************\n\n"); 

  absl::StrAppend(&result, "********** Player Positions **********\n"); 
  for (int i = 0; i < NumPlayers(); ++i) {
    absl::StrAppend(&result, "Player ", i, " position: ", player_positions_[i].ToString(), "\n"); 
  }
  absl::StrAppend(&result, "**************************************\n\n"); 

  absl::StrAppend(&result, "********** Current Market **********\n"); 
  absl::StrAppend(&result, market_.ToString(), "\n"); 
  return result; 
}

std::string HighLowTradingState::ToString() const {
  std::string result; 
  absl::StrAppend(&result, "********** Game setup **********\n"); 
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
  absl::StrAppend(&result, "********************************\n\n"); 
  absl::StrAppend(&result, PublicInformationString()); 
  return result; 
}

bool HighLowTradingState::IsTerminal() const {
  return MoveNumber() >= game_->MaxGameLength(); 
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
  // Assert that the player and move match the top element of history
  SPIEL_CHECK_FALSE(history_.empty());
  SPIEL_CHECK_EQ(history_.back().player, player);
  SPIEL_CHECK_EQ(history_.back().action, move);
  
  // Save the history without the last action
  std::vector<PlayerAction> saved_history = history_;
  saved_history.pop_back();
  
  // Reset state to initial values
  contract_values_ = {ChanceContractValueAction(0), ChanceContractValueAction(0)};
  contract_high_settle_ = ChanceHighLowAction(false);
  player_permutation_ = ChancePermutationAction(
      std::vector<PlayerRole>(NumPlayers(), PlayerRole::kCustomer), 
      std::vector<int>(NumPlayers(), 0));
  player_target_positions_.assign(NumPlayers(), 0);
  player_positions_.assign(NumPlayers(), PlayerPosition());
  market_ = trade_matching::Market();
  order_fills_.clear();
  player_quotes_.clear();
  
  // Reset base state tracking
  history_.clear();
  move_number_ = 0;
  
  // Replay all actions from saved history
  for (const auto& player_action : saved_history) {
    DoApplyAction(player_action.action);
    history_.push_back(player_action);
    ++move_number_;
  }
}

std::vector<std::pair<Action, double>> HighLowTradingState::ChanceOutcomes() const {
  SPIEL_CHECK_TRUE(IsChanceNode());
  auto [min_action, max_action] = GetActionManager().valid_action_range(
      GetActionManager().game_phase_of_timestep(MoveNumber()));
  std::vector<std::pair<Action, double>> outcomes;
  int num_actions = max_action - min_action + 1;
  double prob = 1.0 / num_actions;
  for (int action = min_action; action <= max_action; ++action) {
    outcomes.push_back({action, prob});
  }
  return outcomes;
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
  // See `high_low_trading.h` for what each entry means 
  return {11 + GetStepsPerPlayer() * GetNumPlayers() * 6 + GetNumPlayers() * 2};
}

void HighLowTradingState::InformationStateTensor(Player player,
  absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, GetGame()->NumPlayers());
  
  std::fill(values.begin(), values.end(), 0);
  
  int offset = 0;
  
  // 1. Game setup (5): [num_steps, max_contracts_per_trade, customer_max_size, max_contract_value, players]
  values[offset++] = static_cast<float>(GetGame()->GetStepsPerPlayer());
  values[offset++] = static_cast<float>(GetGame()->GetMaxContractsPerTrade());
  values[offset++] = static_cast<float>(GetGame()->GetCustomerMaxSize());
  values[offset++] = static_cast<float>(GetGame()->GetMaxContractValue());
  values[offset++] = static_cast<float>(GetGame()->GetNumPlayers());
  
  // 2. One-hot player role (3): [is_value, is_maxmin, is_customer]
  if (MoveNumber() >= GetGame()->MaxChanceNodesInHistory()) {
    PlayerRole my_role = player_permutation_.player_roles_[player];
    switch (my_role) {
      case PlayerRole::kValueCheater:
        values[offset] = 1.0f; // is_value
        break;
      case PlayerRole::kHighLowCheater:
        values[offset + 1] = 1.0f; // is_maxmin (high/low cheater)
        break;
      case PlayerRole::kCustomer:
        values[offset + 2] = 1.0f; // is_customer
        break;
    }
  }
  offset += 3;
  
  // 3. Player id (2): [sin(2 pi player_id / players), cos(2 pi player_id / players)]
  double angle = 2.0 * M_PI * player / GetGame()->GetNumPlayers();
  values[offset++] = static_cast<float>(std::sin(angle));
  values[offset++] = static_cast<float>(std::cos(angle));
  
  // 4. Private information (1): [contract value, max / min, customer target size]
  if (MoveNumber() >= GetGame()->MaxChanceNodesInHistory()) {
    PlayerRole my_role = player_permutation_.player_roles_[player];
    
    // Find the permutation id for this player
    int permutation_id = -1;
    for (int j = 0; j < static_cast<int>(player_permutation_.permutation_.size()); ++j) {
      if (player_permutation_.permutation_[j] == player) {
        permutation_id = j;
        break;
      }
    }
    
    switch (my_role) {
      case PlayerRole::kValueCheater:
        // ValueCheaters know their specific contract value
        if (permutation_id >= 0 && permutation_id <= 1) {
          values[offset] = static_cast<float>(contract_values_[permutation_id].contract_value_);
        }
        break;
      case PlayerRole::kHighLowCheater:
        // HighLowCheaters know if settlement is high (1) or low (-1)
        values[offset] = contract_high_settle_.is_high_ ? 1.0f : -1.0f;
        break;
      case PlayerRole::kCustomer:
        // Customers know their target position
        values[offset] = static_cast<float>(player_target_positions_[player]);
        break;
    }
  }
  offset += 1;
  
  // 5. Positions (num_players, 2): [num_contracts, cash_position] - fixed length first
  int num_players = GetGame()->GetNumPlayers();
  for (int p = 0; p < num_players; ++p) {
    values[offset++] = static_cast<float>(player_positions_[p].num_contracts);
    values[offset++] = static_cast<float>(player_positions_[p].cash_balance);
  }
  
  // 6. Quotes: [bid_px, ask_px, bid_sz, ask_sz, *player_id] - fill remaining space
  for (int quote_idx = 0; quote_idx < static_cast<int>(player_quotes_.size()); ++quote_idx) {
    const auto& quote_pair = player_quotes_[quote_idx];
    int acting_player = quote_pair.first;
    const auto& quote = quote_pair.second;
    
    // Assert we have enough space (InformationStateTensorShape should guarantee this)
    SPIEL_CHECK_LE(offset + 6, static_cast<int>(values.size()));
    
    values[offset++] = static_cast<float>(quote.bid_price_);
    values[offset++] = static_cast<float>(quote.ask_price_);
    values[offset++] = static_cast<float>(quote.bid_size_);
    values[offset++] = static_cast<float>(quote.ask_size_);
    
    // Player id as sin/cos
    double p_angle = 2.0 * M_PI * acting_player / num_players;
    values[offset++] = static_cast<float>(std::sin(p_angle));
    values[offset++] = static_cast<float>(std::cos(p_angle));
  }
}

std::string HighLowTradingState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, GetGame()->NumPlayers());
  
  std::string result;
  
  // Add player's role information
  absl::StrAppend(&result, "********** Private Information **********\n");

  int permutation_id = -1; 
  for (int j = 0; j < player_permutation_.permutation_.size(); ++j) {
    if (player_permutation_.permutation_[j] == player) {
      permutation_id = j; 
      break; 
    }
  }
  SPIEL_CHECK_NE(permutation_id, -1);  
  
  // Check if we're past the permutation phase
  if (MoveNumber() >= GetGame()->MaxChanceNodesInHistory()) {
    PlayerRole my_role = player_permutation_.player_roles_[player];
    
    // Show the player their role
    std::string role_name;
    switch (my_role) {
      case PlayerRole::kValueCheater:
        role_name = "ValueCheater";
        break;
      case PlayerRole::kHighLowCheater:
        role_name = "HighLowCheater";
        break;
      case PlayerRole::kCustomer:
        role_name = "Customer";
        break;
    }
    absl::StrAppend(&result, "My role: ", role_name, "\n");
    
    // Add private information based on role
    switch (my_role) {
      case PlayerRole::kValueCheater:
        // ValueCheaters know the contract values
        SPIEL_CHECK_GE(permutation_id, 0); 
        SPIEL_CHECK_LE(permutation_id, 1); 
        absl::StrAppend(&result, "Candidate contract value: ", 
                         contract_values_[permutation_id].contract_value_, "\n");
        break;
        
      case PlayerRole::kHighLowCheater:
        SPIEL_CHECK_EQ(permutation_id, 2); 
        // HighLowCheaters know which settlement (high or low) will be chosen
          absl::StrAppend(&result, "Settlement will be: ", 
                         contract_high_settle_.is_high_ ? "High" : "Low", "\n");
        break;
        
      case PlayerRole::kCustomer:
        // Customers know their target position
        auto target_position = player_target_positions_[player];
        if (target_position != 0) {
          absl::StrAppend(&result, "My target position: ", target_position, "\n");
        } else {
          absl::StrAppend(&result, "Not supposed to happen. Customer target position should not be 0 \n"); 
          // SpielFatalError("Not supposed to happen. Customer roles should be assigned"); 
        }
        break;
    }
    // Start with public information that all players can see
    absl::StrAppend(&result, PublicInformationString());
    
  } else {
    absl::StrAppend(&result, "Private info pending...\n");
  }
  
  absl::StrAppend(&result, "***************************\n");
  
  return result;
}

// Observations are exactly the info states. Preserve Markov condition. 
std::vector<int> HighLowTradingGame::ObservationTensorShape() const {
  return InformationStateTensorShape(); 
}

std::string HighLowTradingState::ObservationString(Player player) const {
  return InformationStateString(player); 
}

void HighLowTradingState::ObservationTensor(Player player,
  absl::Span<float> values) const {
  return InformationStateTensor(player, values); 
}

HighLowTradingGame::HighLowTradingGame(const GameParameters& params)
    : Game(kGameType, params),
      action_manager_(Config(
        ParameterValue<int>("steps_per_player"), 
        ParameterValue<int>("max_contracts_per_trade"),
        ParameterValue<int>("customer_max_size"),
        ParameterValue<int>("max_contract_value"),
        ParameterValue<int>("players")
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