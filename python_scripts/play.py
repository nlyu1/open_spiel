# %% 
import random
import pyspiel
import numpy as np
# %%
game = pyspiel.load_game("simple_match")
state = game.new_initial_state()

states, actions = [], []
while not state.is_terminal():
  legal_actions = state.legal_actions()
  if state.is_chance_node():
    # Sample a chance event outcome.
    outcomes_with_probs = state.chance_outcomes()
    action_list, prob_list = zip(*outcomes_with_probs)
    action = np.random.choice(action_list, p=prob_list)
    state.apply_action(action)
  else:
    # The algorithm can pick an action based on an observation (fully observable
    # games) or an information state (information available for that player)
    # We arbitrarily select the first available action as an example.
    action = legal_actions[0]
    state.apply_action(action)
  states.append(state)
  actions.append(action) 

# %% IInteractive play 

game_params = {
  "max_time_steps": 2, 
  "max_contracts": 2, 
  "max_shares_per_contract": 10, 
  "initial_price": 1000., 
  "strike_price": 1000., 
  "premium_price": 100., 
  "mu": 0.0, 
  "sigma": 1.0, 
  "delta_t": 0.1, 
  "interest_rate": 0.0, 
}
max_shares = game_params['max_contracts'] * game_params['max_shares_per_contract']
game = pyspiel.load_game("black_scholes", game_params)

def delta_to_action(delta_contracts, delta_shares, game_params):
  max_contracts = game_params['max_contracts']
  max_shares = game_params['max_shares_per_contract'] * max_contracts
  assert (delta_shares >= -max_shares) and (delta_shares <= max_shares)
  assert (delta_contracts >= -max_contracts) and (delta_contracts <= max_contracts)
  # (0, -1, 1, -2, 2 ...) <= (0, 1, 2, 3, 4, 5)
  contract_num = delta_contracts*2 if delta_contracts >= 0 else delta_contracts * -2 - 1
  action_id = contract_num * (2 * max_shares + 1) + delta_shares + max_shares
  return int(action_id)

# %% Sample trajectory
states, actions = [], []
t = 0 

state = game.new_initial_state()
print(game_params)
while not state.is_terminal():
  legal_actions = state.legal_actions()
  player = state.current_player()
  print(state.observation_string(0))
  if state.is_chance_node():
    outcomes = state.chance_outcomes()
    num_actions = len(outcomes)
    action_list, prob_list = zip(*outcomes)
    action_id = np.random.choice(action_list, p=prob_list)
    print(f"Chance node: got {num_actions} outcomes and sampled {action_id}")
    state.apply_action(action_id)
  elif t == 0: 
    num_contracts = int(input(f"t=0, Number of <={game_params['max_contracts']} contracts: "))
    num_shares = int(input(f"t=0, Number of <={max_shares} shares: "))
    action_id = delta_to_action(num_contracts, num_shares, game_params)
    state.apply_action(action_id)
  else:
    num_shares = int(input(f"t={t}, Number of shares to buy: "))
    action_id = delta_to_action(0, num_shares, game_params)
    state.apply_action(action_id)
  action_string = state.action_to_string(player, action_id)
  print(f"Player {player}, action: {action_string}")
  t += 1 

print('Final state')
print(state.observation_string(0))
returns = state.returns()
for pid in range(game.num_players()):
  print("Utility for player {} is {}".format(pid, returns[pid]))
  
# %% Undo action test 
num_steps = 0 
game_params = {
  "max_time_steps": 20, 
  "max_contracts": 10, 
  "max_shares_per_contract": 3, 
  "initial_price": 1000., 
  "strike_price": 1000., 
  "premium_price": 100., 
  "mu": 0.0, 
  "sigma": 1.0, 
  "delta_t": 0.1, 
  "interest_rate": 0.0, 
}
max_contracts = game_params['max_contracts']
max_shares = game_params['max_contracts'] * game_params['max_shares_per_contract']
game = pyspiel.load_game("black_scholes", game_params)

state = game.new_initial_state()
print('Initial state:', state.observation_string(0))
action_id = delta_to_action(delta_contracts=-2, delta_shares=20, game_params=game_params)
print(f"Action: {action_id}")
state.apply_action(action_id)
print(f"Action: {state.action_to_string(0, action_id)}")
print('State after action:', state.observation_string(0))
env_action_id = 0 
state.apply_action(env_action_id)
print(f"Action: {state.action_to_string(-1, env_action_id)}")
print('State after action:', state.observation_string(0))
sec_action_id = delta_to_action(delta_contracts=0, delta_shares=-30, game_params=game_params)
state.apply_action(sec_action_id)
print(f"Action: {state.action_to_string(0, sec_action_id)}")
print('State after action:', state.observation_string(0))


state.undo_action(0, sec_action_id)
print('State after undo:', state.observation_string(0))
state.undo_action(-1, env_action_id)
print('State after undo:', state.observation_string(0))
state.undo_action(0, action_id)
print('State after undo:', state.observation_string(0))

# %% Speed test 
num_steps = 0 
game_params = {
  "max_time_steps": 20, 
  "max_contracts": 10, 
  "max_shares_per_contract": 3, 
  "initial_price": 1000., 
  "strike_price": 1000., 
  "premium_price": 100., 
  "mu": 0.0, 
  "sigma": 1.0, 
  "delta_t": 0.1, 
  "interest_rate": 0.0, 
}
max_contracts = game_params['max_contracts']
max_shares = game_params['max_contracts'] * game_params['max_shares_per_contract']
game = pyspiel.load_game("black_scholes", game_params)

total_steps = int(1e3) 
from time import time 
start_time = time()
while num_steps <= total_steps:
  state = game.new_initial_state()
  while not state.is_terminal():
    legal_actions = state.legal_actions()
    player = state.current_player()
    # print(state.observation_string(0))
    if state.is_chance_node():
      outcomes = state.chance_outcomes()
      num_actions = len(outcomes)
      action_list, prob_list = zip(*outcomes)
      action_id = np.random.choice(action_list, p=prob_list)
      # print(f"Chance node: got {num_actions} outcomes and sampled {action_id}")
      state.apply_action(action_id)
    else:
      action_list = state.legal_actions()
      action_id = np.random.choice(action_list)
      # stock_delta = action_id % (2 * max_shares + 1) - max_shares
      # contract_num = action_id // (2 * max_shares + 1)
      # contract_delta = (contract_num / 2) if (contract_num % 2 == 0) else -(contract_num + 1) / 2
      # print(f"[Player] predicted delta: stock={stock_delta}, contract={contract_delta}")
      # print(f"[Player] actual {state.action_to_string(player, action_id)}")
      state.apply_action(action_id) 
    num_steps += 1 

duration = time() - start_time
print(f"Average steps per second: {num_steps / duration}")
# %%
