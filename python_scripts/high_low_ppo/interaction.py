# %% 

import numpy as np 
import torch 
from typing import Dict, List, Tuple, Optional, Any
import sys 
import os

from high_low_wrapper import * 
from agent import * 
# %%
seed = 2025
torch.manual_seed(seed)
np.random.seed(seed)

game_config: Dict[str, int] = {
    'steps_per_player': 5,           # Number of trading steps per player
    'max_contracts_per_trade': 2,     # Maximum contracts in a single trade
    'customer_max_size': 5,           # Maximum position size for customers
    'max_contract_value': 30,         # Maximum value a contract can have
    'players': 5                      # Total number of players in the game
}

npc_agents = [
    HighLowModel(game_config).cuda()
    for _ in range(game_config['players'] - 1)
]
actor_agent = HighLowModel(game_config).cuda()

env = SinglePlayerHighLowTradingEnvVector(game_config, num_envs=8, seed=0, agents=npc_agents)

input_obs = env.current_observation()
bid_price, ask_price, bid_size, ask_size = actor_agent.sample_actions(input_obs)
print(bid_price)
print(ask_price)
print(bid_size)
print(ask_size)
# %%