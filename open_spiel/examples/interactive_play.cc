#include <iostream>
#include <memory>
#include "open_spiel/spiel.h"
#include "open_spiel/games/high_low_trading/high_low_trading.h"
#include "open_spiel/games/high_low_trading/action_manager.h"

using namespace open_spiel;
using namespace open_spiel::high_low_trading;

int main() {
    std::cout << "=== HIGH LOW TRADING INTERACTIVE SETUP ===" << std::endl;
    // Local variables for easy modification
    int steps_per_player = 2; 
    int max_contracts_per_trade = 2;
    int customer_max_size = 3;
    int max_contract_value = 30;
    
    int num_players = 4;
    int contract_value_1 = 5;
    int contract_value_2 = 25;
    bool settle_high = true;
    int permutation_raw_id = 21;
    
    // 1. Create game with specified parameters
    GameParameters params;
    params["players"] = GameParameter(num_players);
    params["steps_per_player"] = GameParameter(steps_per_player);
    params["max_contracts_per_trade"] = GameParameter(max_contracts_per_trade);
    params["customer_max_size"] = GameParameter(customer_max_size);
    params["max_contract_value"] = GameParameter(max_contract_value);
    
    auto game = LoadGame("high_low_trading", params);
    auto state = game->NewInitialState();
    
    std::cout << "Game created with parameters:" << std::endl;
    std::cout << "- Players: " << num_players << std::endl;
    std::cout << "- Steps per player: " << steps_per_player << std::endl;
    std::cout << "- Max contracts per trade: " << max_contracts_per_trade << std::endl;
    std::cout << "- Customer max size: " << customer_max_size << std::endl;
    std::cout << "- Max contract value: " << max_contract_value << std::endl;
    std::cout << std::endl;
    
    // 2. Create explicit Config and ActionManager for easy customization
    Config config(steps_per_player, max_contracts_per_trade, customer_max_size, max_contract_value, num_players);
    ActionManager action_manager(config);
    
    std::cout << "Setting up game state..." << std::endl;
    std::cout << "Contract values: " << contract_value_1 << ", " << contract_value_2 << std::endl;
    std::cout << "Settlement: " << (settle_high ? "HIGH" : "LOW") << std::endl;
    std::cout << "Permutation raw_id: " << permutation_raw_id << std::endl;
    std::cout << std::endl;
    
    // 3. Apply first contract value    
    std::cout << "Move " << state->MoveNumber() << ": Setting first contract value..." << std::endl;
    ChanceContractValueAction contract_action_1(contract_value_1);
    Action raw_action_1 = action_manager.StructuredToRawAction(GamePhase::kChanceValue, contract_action_1);
    std::cout << "Contract value " << contract_value_1 << " -> raw action: " << raw_action_1 << std::endl;
    state->ApplyAction(raw_action_1);
    
    // 4. Apply second contract value 
    std::cout << "Move " << state->MoveNumber() << ": Setting second contract value..." << std::endl;
    ChanceContractValueAction contract_action_2(contract_value_2);
    Action raw_action_2 = action_manager.StructuredToRawAction(GamePhase::kChanceValue, contract_action_2);
    std::cout << "Contract value " << contract_value_2 << " -> raw action: " << raw_action_2 << std::endl;
    state->ApplyAction(raw_action_2);
    
    // 5. Apply settlement choice (high)
    std::cout << "Move " << state->MoveNumber() << ": Setting settlement choice..." << std::endl;
    ChanceHighLowAction settlement_action(settle_high);
    Action raw_action_settlement = action_manager.StructuredToRawAction(GamePhase::kChanceHighLow, settlement_action);
    std::cout << "Settlement " << (settle_high ? "HIGH" : "LOW") << " -> raw action: " << raw_action_settlement << std::endl;
    state->ApplyAction(raw_action_settlement);
    
    // 6. Apply permutation (raw_id = 0)
    std::cout << "Move " << state->MoveNumber() << ": Setting player role permutation..." << std::endl;
    ActionVariant permutation_variant = action_manager.RawToStructuredAction(GamePhase::kChancePermutation, permutation_raw_id);
    std::cout << "Permutation raw_id " << permutation_raw_id << " -> applying directly" << std::endl;
    state->ApplyAction(permutation_raw_id);
    
    // 7. Set customer target position to 2
    std::cout << "Move " << state->MoveNumber() << ": Setting customer target position..." << std::endl;
    int customer_target_size = 2;
    ChanceCustomerSizeAction customer_size_action(customer_target_size);
    Action raw_action_customer = action_manager.StructuredToRawAction(GamePhase::kCustomerSize, customer_size_action);
    std::cout << "Customer target size " << customer_target_size << " -> raw action: " << raw_action_customer << std::endl;
    state->ApplyAction(raw_action_customer);
    
    // 8. Print current game state after setup
    std::cout << std::endl << "=== GAME STATE AFTER SETUP ===" << std::endl;
    std::cout << state->ToString() << std::endl;
    
    // 9. Show each player's private information state
    std::cout << std::endl << "=== PLAYER PRIVATE INFORMATION ===" << std::endl;
    for (int player_id = 0; player_id < num_players; ++player_id) {
        std::cout << std::endl << "--- Player " << player_id << " Information State ---" << std::endl;
        std::cout << state->InformationStateString(player_id) << std::endl;
    }
    
    // 10. Interactive player trading rounds
    std::cout << std::endl << "=== STARTING INTERACTIVE PLAYER TRADING ===" << std::endl;
    
    while (!state->IsTerminal() && !state->IsChanceNode()) {
        Player current_player = state->CurrentPlayer();
        std::cout << std::endl << "--- Move " << state->MoveNumber() << " ---" << std::endl;
        std::cout << "You are player " << current_player << std::endl;
        
        // Get user input for quote
        int bid_px, ask_px, bid_sz, ask_sz;
        std::cout << "Input (bid_px, ask_px, bid_sz, ask_sz): ";
        std::cin >> bid_px >> ask_px >> bid_sz >> ask_sz;
        
        // Validate input ranges
        if (bid_px < 1 || bid_px > max_contract_value || 
            ask_px < 1 || ask_px > max_contract_value ||
            bid_sz < 0 || bid_sz > max_contracts_per_trade ||
            ask_sz < 0 || ask_sz > max_contracts_per_trade) {
            std::cout << "Invalid input! Ranges:" << std::endl;
            std::cout << "  Prices: [1, " << max_contract_value << "]" << std::endl;
            std::cout << "  Sizes: [0, " << max_contracts_per_trade << "]" << std::endl;
            continue;
        }
        
        // Create and apply the trading action
        PlayerQuoteAction quote_action(bid_sz, bid_px, ask_sz, ask_px);
        Action raw_quote_action = action_manager.StructuredToRawAction(GamePhase::kPlayerTrading, quote_action);
        
        std::cout << "Quote " << bid_px << "@" << ask_px << " size " << bid_sz << "x" << ask_sz 
                  << " -> raw action: " << raw_quote_action << std::endl;
        
        state->ApplyAction(raw_quote_action);
        
        // Show updated state
        std::cout << "Updated game state:" << std::endl;
        std::cout << state->ToString() << std::endl;
    }
    
    // 10. Final results
    std::cout << std::endl << "=== GAME COMPLETED ===" << std::endl;
    std::cout << "Final game state:" << std::endl;
    std::cout << state->ToString() << std::endl;
    
    if (state->IsTerminal()) {
        auto returns = state->Returns();
        std::cout << std::endl << "=== FINAL RETURNS ===" << std::endl;
        for (int i = 0; i < returns.size(); ++i) {
            std::cout << "Player " << i << ": " << returns[i] << std::endl;
        }
    }
    
    return 0;
}
