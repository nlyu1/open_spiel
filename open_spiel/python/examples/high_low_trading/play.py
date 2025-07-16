"""Interactive High Low Trading game example with human players."""

import random
import sys
import os

# Add the parent directory to Python path to import pyspiel
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
import pyspiel

# Import our action manager utilities
from utils import (
    Config, ActionManager, GamePhase, PlayerRole
)
 

def print_game_state_info(state, action_manager):
    """Print detailed game state information."""
    print("\n=== GAME STATE INFO ===")
    print(f"State: {state}")
    print(f"Move number: {len(state.history())}")
    
    if len(state.history()) >= 1:
        print(f"Contract value 1: Available after move 0")
    if len(state.history()) >= 2:
        print(f"Contract value 2: Available after move 1") 
    if len(state.history()) >= 3:
        print(f"High/Low choice: Available after move 2")
    if len(state.history()) >= 4:
        print(f"Player roles: Available after move 3")
    if len(state.history()) >= 8:  # 4 + num_players
        print(f"Customer target positions: Available after moves 4-7")
    
    print("=" * 23)


def print_observations(state, game, action_manager):
    """Print current observations for all players."""
    print("\n=== PLAYER OBSERVATIONS ===")
    
    for player in range(game.num_players()):
        break 
        # Get observation string and tensor
        obs_string = state.observation_string(player)
        obs_tensor = state.observation_tensor(player)
        info_string = state.information_state_string(player)
        info_tensor = state.information_state_tensor(player)
        
        print(f"Player {player}:")
        print(f"  Observation string: '{obs_string}'")
        print(f"  Observation tensor: {obs_tensor}")
        print(f"  Information state string: '{info_string}'")
        print(f"  Information state tensor: {info_tensor}")
    print("=" * 28)


def show_quote_examples(action_manager):
    """Show examples of how to create quotes using the ActionManager interface."""
    print("\n--- Quote Examples (using ActionManager.player_quote_move) ---")
    examples = [
        (10, 1, 11, 1, "Tight spread around expected value"),
        (8, 2, 12, 2, "Wider spread, larger size"),
        (1, 0, 30, 1, "Only offering to sell (no bid)"),
        (15, 3, 16, 0, "Only offering to buy (no ask)"),
    ]
    
    for bid_px, bid_sz, ask_px, ask_sz, description in examples:
        try:
            raw_action = action_manager.player_quote_move(bid_px, bid_sz, ask_px, ask_sz)
            print(f"  action_manager.player_quote_move({bid_px}, {bid_sz}, {ask_px}, {ask_sz}) -> {raw_action}")
            print(f"    # {description}")
        except ValueError as e:
            print(f"  Invalid quote ({bid_px}, {bid_sz}, {ask_px}, {ask_sz}): {e}")
    print("--- End Examples ---\n")


def get_human_action(state, player, action_manager):
    """Get action input from human player."""
    legal_actions = state.legal_actions()
    
    print(f"\nPlayer {player}, it's your turn!")
    
    # Show quote examples for trading phase
    move_number = len(state.history())
    phase = action_manager.game_phase_of_timestep(move_number)
    if phase == GamePhase.PLAYER_TRADING:
        show_quote_examples(action_manager)
    
    print("Legal actions:")
    
    # Show structured action interpretations
    for i, action in enumerate(legal_actions):
        action_str = state.action_to_string(player, action)
        try:
            structured_action = action_manager.raw_to_structured_action(phase, action)
            print(f"  {i}: {action_str} -> {structured_action}")
        except:
            print(f"  {i}: {action_str}")
    
    while True:
        try:
            choice = input(f"Enter your choice (0-{len(legal_actions)-1}): ").strip()
            choice_idx = int(choice)
            if 0 <= choice_idx < len(legal_actions):
                return legal_actions[choice_idx]
            else:
                print(f"Invalid choice. Please enter a number between 0 and {len(legal_actions)-1}")
        except ValueError:
            print("Invalid input. Please enter a number.")
        except KeyboardInterrupt:
            print("\nGame interrupted by user.")
            return None


def setup_predetermined_game(game):
    """Set up the game with predetermined chance events as specified."""
    state = game.new_initial_state()
    action_manager = ActionManager(Config(
        steps_per_player=2,
        max_contracts_per_trade=5,
        customer_max_size=5,
        max_contract_value=20,
        num_players=4
    ))
    
    print("=== SETTING UP PREDETERMINED GAME ===")
    print("Let me examine what actions the game expects...")
    
    # Let's work with what the game actually provides
    chance_moves_made = 0
    target_moves = [
        (15, "contract value 15"),
        (5, "contract value 5"), 
        (False, "low settlement"),
        ([0, 1, 2, 3], "trivial permutation"),
        (-3, "customer size -3")
    ]
    
    while chance_moves_made < len(target_moves) and state.is_chance_node():
        legal_actions = state.legal_actions()
        move_number = len(state.history())
        print(f"\nMove {move_number}: {len(legal_actions)} legal actions available")
        
        target_value, description = target_moves[chance_moves_made]
        
        try:
            phase = action_manager.game_phase_of_timestep(move_number)
            print(f"Expected phase: {phase}")
            
            # Try to use our ActionManager methods
            if chance_moves_made == 0:  # First contract value
                desired_action = action_manager.chance_value_move(target_value)
            elif chance_moves_made == 1:  # Second contract value
                desired_action = action_manager.chance_value_move(target_value)
            elif chance_moves_made == 2:  # High/low choice
                desired_action = action_manager.chance_high_low_move(set_to_high=target_value)
            elif chance_moves_made == 3:  # Permutation
                desired_action = action_manager.chance_permutation_move(target_value)
            elif chance_moves_made == 4:  # Customer size
                desired_action = action_manager.chance_customer_size(target_value)
                
            if desired_action in legal_actions:
                print(f"✓ Using ActionManager method for {description}: action {desired_action}")
                state.apply_action(desired_action)
                chance_moves_made += 1
            else:
                print(f"✗ ActionManager method gave action {desired_action}, but not in legal actions")
                # Fallback: just pick the first legal action
                print(f"  Using fallback: action {legal_actions[0]}")
                state.apply_action(legal_actions[0])
                chance_moves_made += 1
                
        except Exception as e:
            print(f"✗ ActionManager method failed: {e}")
            # Fallback: just pick the first legal action  
            print(f"  Using fallback: action {legal_actions[0]}")
            state.apply_action(legal_actions[0])
            chance_moves_made += 1
    
    print("\n=== CHANCE SETUP COMPLETE ===")
    print("Note: Some actions may not match intended setup due to game/ActionManager mismatch")
    print("The game will proceed with whatever chance events were actually applied.")
    
    return state, action_manager


def main():
    print("=== HIGH LOW TRADING INTERACTIVE GAME ===")
    print("Players will trade contracts with predetermined setup:")
    print("- Contract values: 10 and 20, settling to minimum (10)")
    print("- 4 players: 2 value cheaters, 1 high/low cheater, 1 customer") 
    print("- Customer target: -3 contracts")
    print("- 2 trading steps per player")
    print()
    
    # Load the high_low_trading game with our custom parameters
    game_params = {
        "steps_per_player": 2,
        "max_contracts_per_trade": 5,
        "customer_max_size": 5,
        "max_contract_value": 30,
        "players": 4
    }
    
    game = pyspiel.load_game("high_low_trading", game_params)
    print(f"Loaded game: {game.get_type().long_name}")
    print(f"Number of players: {game.num_players()}")
    print(f"Utility range: [{game.min_utility()}, {game.max_utility()}]")
    print()

    # Set up the predetermined game state
    state, action_manager = setup_predetermined_game(game)
    
    print("\n=== STARTING PLAYER TRADING PHASE ===")
    
    # Main game loop for player actions
    while not state.is_terminal():
        print_game_state_info(state, action_manager)
        
        if state.is_chance_node():
            # This shouldn't happen in our predetermined setup, but handle it just in case
            print("Unexpected chance node encountered!")
            outcomes = state.chance_outcomes()
            action_list, prob_list = zip(*outcomes)
            action = random.choices(action_list, weights=prob_list)[0]
            state.apply_action(action)
        else:
            # Decision node - get human input
            current_player = state.current_player()
            
            # Show observations before the player makes their decision
            print_observations(state, game, action_manager)
            
            # Get human action
            action = get_human_action(state, current_player, action_manager)
            if action is None:  # User interrupted
                return
                
            action_string = state.action_to_string(current_player, action)
            print(f"Player {current_player} chose: {action_string}")
            state.apply_action(action)

    # Game finished
    print(f"\n=== GAME OVER ===")
    print(f"Final state: {state}")
    
    # Show final observations
    print_observations(state, game, action_manager)
    
    # Show final payoffs
    returns = state.returns()
    print("\n=== FINAL PAYOFFS ===")
    for player in range(game.num_players()):
        print(f"Player {player} utility: {returns[player]}")
    
    # Determine winner
    max_return = max(returns)
    winners = [i for i, r in enumerate(returns) if r == max_return]
    
    if len(winners) == 1:
        print(f"Player {winners[0]} wins!")
    else:
        print(f"Tie between players: {winners}")
    
    print("\nThanks for playing High Low Trading!")


if __name__ == "__main__":
    main() 