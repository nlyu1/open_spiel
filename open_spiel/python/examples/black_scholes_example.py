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

"""Interactive Black-Scholes option trading game example."""

import random
import sys
import os
import argparse

# Add the parent directory to Python path to import pyspiel
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
import pyspiel


def print_game_info(game):
    """Print basic information about the game."""
    print(f"=== {game.get_type().long_name} ===")
    print(f"Number of players: {game.num_players()}")
    print(f"Utility range: [{game.min_utility():.2f}, {game.max_utility():.2f}]")
    print(f"Max game length: {game.max_game_length()}")
    print()


def print_portfolio_state(state, game):
    """Print current portfolio and market state."""
    obs_tensor = state.observation_tensor(0)
    
    # Parse observation tensor based on encoding from black_scholes.h:
    # [stock_holding, cash_holding, contract_holding, strike_price, stock_price, 
    #  premium, delta_t, mu, sigma, interest_rate, t/maxTimeSteps, maxTimeSteps]
    stock_holding = obs_tensor[0]
    cash_holding = obs_tensor[1]
    contract_holding = obs_tensor[2]
    strike_price = obs_tensor[3]
    stock_price = obs_tensor[4]
    premium = obs_tensor[5]
    delta_t = obs_tensor[6]
    mu = obs_tensor[7]
    sigma = obs_tensor[8]
    interest_rate = obs_tensor[9]
    time_progress = obs_tensor[10]
    max_time_steps = obs_tensor[11]
    
    print("=== CURRENT PORTFOLIO & MARKET STATE ===")
    print(f"Time: {time_progress:.2f} / {max_time_steps:.0f}")
    print(f"Stock Price: ${stock_price:.2f}")
    print(f"Strike Price: ${strike_price:.2f}")
    print(f"Option Premium: ${premium:.2f}")
    print()
    print("Portfolio:")
    print(f"  Stock Holdings: {stock_holding:.0f} shares")
    print(f"  Cash Holdings: ${cash_holding:.2f}")
    print(f"  Contract Holdings: {contract_holding:.0f} options")
    
    # Calculate current portfolio value
    option_value = max(0, stock_price - strike_price) * contract_holding
    portfolio_value = stock_holding * stock_price + cash_holding + option_value
    print(f"  Total Portfolio Value: ${portfolio_value:.2f}")
    print()
    
    print("Market Parameters:")
    print(f"  Drift (μ): {mu:.3f}")
    print(f"  Volatility (σ): {sigma:.3f}")
    print(f"  Time Step (Δt): {delta_t:.3f}")
    print(f"  Interest Rate: {interest_rate:.3f}")
    print("=" * 42)


def explain_action_space(state, game, is_first_move):
    """Explain the available actions to the user."""
    print("\n=== TRADING OPTIONS ===")
    
    if is_first_move:
        print("FIRST MOVE: You can trade both stocks AND options")
        print("Actions encode: (stock_delta, contract_delta)")
        print("- Stock delta: how many shares to buy/sell")
        print("- Contract delta: how many option contracts to buy/sell")
        print("- Positive = buy, negative = sell")
    else:
        print("SUBSEQUENT MOVE: You can only trade stocks")
        print("Actions encode: (stock_delta, 0)")
        print("- Stock delta: how many shares to buy/sell")
        print("- Positive = buy, negative = sell")
    
    print("\nNote: Buying requires cash, selling provides cash")
    print("Option contracts have upfront premium cost")
    print("=" * 30)


def get_user_action(state, game):
    """Get trading decision from user."""
    legal_actions = state.legal_actions()
    is_first_move = (state.move_number() == 0)
    
    print_portfolio_state(state, game)
    explain_action_space(state, game, is_first_move)
    
    print(f"\nYou have {len(legal_actions)} available actions (0 to {len(legal_actions)-1})")
    
    # Show a few example actions (safely)
    print("\nExample actions:")
    examples = [0, len(legal_actions)//4, len(legal_actions)//2, 
               3*len(legal_actions)//4, len(legal_actions)-1]
    
    for i, action_idx in enumerate(examples[:5]):  # Show up to 5 examples
        if action_idx < len(legal_actions):
            action = legal_actions[action_idx]
            try:
                action_str = state.action_to_string(0, action)
                print(f"  {action_idx}: {action_str}")
            except Exception as e:
                print(f"  {action_idx}: Action {action} (unable to decode: {str(e)})")
    
    print("\nEnter 'h' for help with action encoding")
    
    while True:
        try:
            user_input = input(f"\nChoose action (0-{len(legal_actions)-1}) or 'h' for help: ").strip()
            
            if user_input.lower() == 'h':
                print_action_help(game, is_first_move)
                continue
                
            choice_idx = int(user_input)
            if 0 <= choice_idx < len(legal_actions):
                return legal_actions[choice_idx]
            else:
                print(f"Invalid choice. Please enter a number between 0 and {len(legal_actions)-1}")
        except ValueError:
            print("Invalid input. Please enter a number or 'h' for help.")
        except KeyboardInterrupt:
            print("\nGame interrupted by user.")
            return None


def print_action_help(game, is_first_move):
    """Print detailed help about action encoding."""
    print("\n=== ACTION ENCODING HELP ===")
    
    # We need to get these from the game parameters
    # Since we can't directly access the game's private members from Python,
    # we'll use reasonable defaults based on the game's design
    max_contracts = 100  # Default from black_scholes.h
    max_shares_per_contract = 100  # Default from black_scholes.h
    max_shares = max_contracts * max_shares_per_contract
    
    print(f"Max shares per trade: ±{max_shares}")
    print(f"Max contracts per trade: ±{max_contracts}")
    
    if is_first_move:
        print("\nAction format: Each action encodes (stock_delta, contract_delta)")
        print("Action calculation:")
        print("  stock_delta = (action % (2 * max_shares + 1)) - max_shares")
        print("  contract_delta uses a special encoding for the remaining part")
        print("\nExamples:")
        print("  Action 0: Usually represents (0 shares, 0 contracts)")
        print("  Middle actions: Mix of stock and contract trades")
        print("  Higher actions: Larger position changes")
    else:
        print("\nAction format: Each action encodes (stock_delta, 0)")
        print("Only stock trading allowed on subsequent moves")
        print("Action calculation:")
        print("  stock_delta = action - max_shares")
        print("\nExamples:")
        print(f"  Action 0: Sell {max_shares} shares")
        print(f"  Action {max_shares}: Buy/sell 0 shares (no trade)")
        print(f"  Action {2*max_shares}: Buy {max_shares} shares")
    
    print("=" * 30)


def main():
    parser = argparse.ArgumentParser(description='Interactive Black-Scholes Option Trading Game')
    parser.add_argument('--max_time_steps', type=int, default=2, 
                       help='Number of time periods (default: 2)')
    parser.add_argument('--interest_rate', type=float, default=0.0,
                       help='Risk-free interest rate (default: 0.0)')
    parser.add_argument('--sigma', type=float, default=1.0,
                       help='Volatility parameter (default: 1.0)')
    parser.add_argument('--mu', type=float, default=0.0,
                       help='Drift parameter (default: 0.0)')
    parser.add_argument('--delta_t', type=float, default=0.1,
                       help='Time step size (default: 0.1)')
    parser.add_argument('--initial_price', type=float, default=1000.0,
                       help='Initial stock price (default: 1000.0)')
    parser.add_argument('--strike_price', type=float, default=1000.0,
                       help='Option strike price (default: 1000.0)')
    parser.add_argument('--premium_price', type=float, default=100.0,
                       help='Option premium price (default: 100.0)')
    parser.add_argument('--max_contracts', type=int, default=100,
                       help='Maximum contracts tradeable (default: 100)')
    parser.add_argument('--max_shares_per_contract', type=int, default=100,
                       help='Maximum shares per contract (default: 100)')
    
    args = parser.parse_args()
    
    print("=== BLACK-SCHOLES OPTION TRADING SIMULATION ===")
    print("You are a trader in a Black-Scholes market environment.")
    print("Make strategic decisions to buy/sell stocks and option contracts.")
    print("Goal: Maximize your portfolio value by game end.")
    print()
    
    # Build game string with parameters
    game_params = [
        f"max_time_steps={args.max_time_steps}",
        f"interest_rate={args.interest_rate}",
        f"sigma={args.sigma}",
        f"mu={args.mu}",
        f"delta_t={args.delta_t}",
        f"initial_price={args.initial_price}",
        f"strike_price={args.strike_price}",
        f"premium_price={args.premium_price}",
        f"max_contracts={args.max_contracts}",
        f"max_shares_per_contract={args.max_shares_per_contract}"
    ]
    game_string = f"black_scholes({','.join(game_params)})"
    
    print(f"Loading game: {game_string}")
    game = pyspiel.load_game(game_string)
    print_game_info(game)
    
    # Create initial state
    state = game.new_initial_state()
    print("=== GAME START ===")
    
    # Main game loop
    while not state.is_terminal():
        print(f"\nMove {state.move_number()}: {state}")
        
        if state.is_chance_node():
            # Handle environment move (stock price update)
            print("🎲 ENVIRONMENT MOVE: Stock price updating...")
            outcomes = state.chance_outcomes()
            
            # Sample outcome
            action_list, prob_list = zip(*outcomes)
            action = random.choices(action_list, weights=prob_list)[0]
            
            # Show what happened
            if action == 0:
                print("📉 Stock price moved DOWN")
            else:
                print("📈 Stock price moved UP")
                
            state.apply_action(action)
            
        else:
            # Player decision node
            print("💼 YOUR TURN: Make a trading decision")
            
            action = get_user_action(state, game)
            if action is None:  # User interrupted
                return
            
            action_string = state.action_to_string(0, action)
            print(f"\n✅ You chose: {action_string}")
            state.apply_action(action)

    # Game finished
    print(f"\n=== GAME OVER ===")
    print(f"Final state: {state}")
    
    # Show final portfolio value
    print_portfolio_state(state, game)
    
    final_return = state.returns()[0]
    print(f"\n🏆 FINAL PORTFOLIO VALUE: ${final_return:.2f}")
    
    if final_return > 0:
        print("🎉 Congratulations! You made a profit!")
    elif final_return == 0:
        print("😐 You broke even.")
    else:
        print("😔 You incurred a loss. Better luck next time!")
    
    print("\nThank you for playing Black-Scholes Option Trading!")


if __name__ == "__main__":
    main() 