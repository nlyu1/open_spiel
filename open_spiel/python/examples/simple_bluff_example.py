#!/usr/bin/env python3
# Copyright 2019 DeepMind Technologies Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Interactive Simple Bluff game example with human players."""

import random
import sys
import os

# Add the parent directory to Python path to import pyspiel
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
import pyspiel


def print_observations(state, game):
    """Print current observations for both players."""
    print("\n=== PLAYER OBSERVATIONS ===")
    
    for player in range(game.num_players()):
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


def get_human_action(state, player):
    """Get action input from human player."""
    legal_actions = state.legal_actions()
    
    print(f"\nPlayer {player}, it's your turn!")
    print("Legal actions:")
    for i, action in enumerate(legal_actions):
        action_str = state.action_to_string(player, action)
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


def main():
    print("=== SIMPLE BLUFF INTERACTIVE GAME ===")
    print("Two players will receive random coins (0 or 1).")
    print("Player 1 can Check or Bet. If Player 1 bets, Player 2 can Fold or Call.")
    print("Higher coin wins in showdown. Equal coins = tie.")
    print()
    
    # Load the simple_bluff game
    game = pyspiel.load_game("simple_bluff")
    print(f"Loaded game: {game.get_type().long_name}")
    print(f"Number of players: {game.num_players()}")
    print(f"Utility range: [{game.min_utility()}, {game.max_utility()}]")
    print()

    # Create the initial state
    state = game.new_initial_state()
    print("=== GAME START ===")
    print(f"Initial state: {state}")
    
    # Main game loop
    while not state.is_terminal():
        print(f"\nCurrent state: {state}")
        
        if state.is_chance_node():
            # Handle chance node (coin dealing)
            outcomes = state.chance_outcomes()
            print(f"Chance node: dealing coin to player...")
            
            # Sample outcome
            action_list, prob_list = zip(*outcomes)
            action = random.choices(action_list, weights=prob_list)[0]
            
            # Determine which player is getting the coin
            history_size = len(state.history())
            player_getting_coin = history_size  # First coin goes to player 0, second to player 1
            
            coin_value = action
            print(f"Player {player_getting_coin} receives coin: {coin_value}")
            state.apply_action(action)
            
        else:
            # Decision node - get human input
            current_player = state.current_player()
            
            # Show observations before the player makes their decision
            print_observations(state, game)
            
            # Get human action
            action = get_human_action(state, current_player)
            if action is None:  # User interrupted
                return
                
            action_string = state.action_to_string(current_player, action)
            print(f"Player {current_player} chose: {action_string}")
            state.apply_action(action)

    # Game finished
    print(f"\n=== GAME OVER ===")
    print(f"Final state: {state}")
    
    # Show final observations
    print_observations(state, game)
    
    # Show final payoffs
    returns = state.returns()
    print("\n=== FINAL PAYOFFS ===")
    for player in range(game.num_players()):
        print(f"Player {player} utility: {returns[player]}")
    
    # Determine winner
    if returns[0] > returns[1]:
        print("Player 0 wins!")
    elif returns[1] > returns[0]:
        print("Player 1 wins!")
    else:
        print("It's a tie!")
    
    print("\nThanks for playing Simple Bluff!")


if __name__ == "__main__":
    main() 