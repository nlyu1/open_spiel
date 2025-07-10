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

# %%

game_params = {
  "max_time_steps": 2, 
  "max_contracts": 2, 
  "max_shares_per_contract": 10 
}
max_shares = game_params['max_contracts'] * game_params['max_shares_per_contract']
game = pyspiel.load_game("black_scholes", game_params)
print('Hello')
# %%
states, actions = [], []
t = 0 

state = game.new_initial_state()

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
    num_contracts = input(f"t=0, Number of <={game_params['max_contracts']} contracts: ")
    num_shares = input(f"t=0, Number of <={max_shares} shares: ")
    action_id = int(num_contracts) * max_shares + int(num_shares)
    state.apply_action(action_id)
  else:
    action_id = int(input(f"t={t}, Number of shares to buy: "))
    state.apply_action(action_id)
  action_string = state.action_to_string(player, action_id)
  print(f"Player {player}, action: {action_string}")
  t += 1 

print('Final state')
print(state.observation_string(0))
returns = state.returns()
for pid in range(game.num_players()):
  print("Utility for player {} is {}".format(pid, returns[pid]))
# %%
