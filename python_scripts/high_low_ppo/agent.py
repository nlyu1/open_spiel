import numpy as np
import torch
import torch.nn as nn
from high_low_wrapper import * 
from torch.distributions.categorical import Categorical

def layer_init(layer, std=np.sqrt(2), bias_const=0.0):
    torch.nn.init.orthogonal_(layer.weight, std)
    torch.nn.init.constant_(layer.bias, bias_const)
    return layer

class ResidualBlock(nn.Module):
    def __init__(self, features: int):
        super().__init__()
        self.fc1 = layer_init(nn.Linear(features, features))
        self.ln = nn.LayerNorm(features)
        self.act = nn.GELU()

    def forward(self, x): 
        return x + self.act(self.ln(self.fc1(x)))       

class HighLowModel(nn.Module):
    def __init__(self, high_low_config: Dict[str, int]=default_config, device: str='cuda'):
        super().__init__()
        self.input_length = (
            11 + high_low_config['steps_per_player'] * high_low_config['players'] * 6 
            + high_low_config['players'] * 2
        )
        self.device = device
        self.backbone = nn.Sequential(
            layer_init(nn.Linear(self.input_length, 512)),
            nn.LayerNorm(512),
            nn.GELU(),
            ResidualBlock(512), 
            ResidualBlock(512), 
            ResidualBlock(512), 
        )
        # We factor the big action into independent components. They're related by the latent state
        self.actors = {
            'bid_price': layer_init(nn.Linear(512, high_low_config['max_contract_value'])),
            'bid_size': layer_init(nn.Linear(512, 1 + high_low_config['max_contracts_per_trade'])),
            'ask_price': layer_init(nn.Linear(512, high_low_config['max_contract_value'])),
            'ask_size': layer_init(nn.Linear(512, 1 + high_low_config['max_contracts_per_trade'])),
        }
        for k in self.actors.keys():
            self.register_module(k+'_actor', self.actors[k])
        self.critic = layer_init(nn.Linear(512, 1), std=1)

    def sample_actions(self, obs: np.ndarray):
        """
        Evaluation mode: given batch of observations (batch_size, observation_dim), return (batch_size) of (bid_price, ask_price, bid_size, ask_size)   
        Input and outputs are all in numpy. Evaluated without gradients in eval mode 
        """
        self.eval()
        with torch.no_grad():
            obs = torch.tensor(obs).to(self.device).float()
            latent = self.backbone(obs)
            action_keys = list(self.actors.keys())
            probs = {
                k: Categorical(logits=self.actors[k](latent)) for k in action_keys
            }
            actions = {k: probs[k].sample().cpu().numpy() for k in action_keys}
        self.train()
        return actions['bid_price'], actions['ask_price'], actions['bid_size'], actions['ask_size'] 

    def get_value(self, x):
        latent = self.backbone(x) 
        return self.critic(latent).squeeze(-1) 

    def get_action_and_value(self, x): 
        latent = self.backbone(x) 
        values = self.critic(latent).squeeze(-1) 
        action_keys = list(self.actors.keys())
        print(values.shape, latent.shape)
        probs = {
            k: Categorical(logits=self.actors[k](latent)) for k in action_keys
        }
        actions = {k: probs[k].sample() for k in action_keys}
        log_probs = sum(probs[k].log_prob(actions[k]) for k in action_keys)
        entropies = sum(probs[k].entropy() for k in action_keys)

        return actions, log_probs, entropies, values

if __name__ == '__main__':
    game_config: Dict[str, int] = {
        'steps_per_player': 10,           # Number of trading steps per player
        'max_contracts_per_trade': 2,     # Maximum contracts in a single trade
        'customer_max_size': 5,           # Maximum position size for customers
        'max_contract_value': 30,         # Maximum value a contract can have
        'players': 5                      # Total number of players in the game
    }

    env = HighLowTradingEnvVector(game_config, num_envs=128, seed=0)
    obs = env.player_observation_tensors()
    print(obs.shape)
    model = HighLowModel(game_config).cuda()
    inputs = torch.tensor(obs[:, 0]).cuda().float() 
    actions, log_probs, entropies, values = model.get_action_and_value(inputs)
    print(log_probs.shape)
    print(entropies.shape)
    print(values.shape)
    # The 0-th player's action
    bid_price, ask_price, bid_size, ask_size = model.sample_actions(obs[:, 0])
    print(bid_price.shape)
    print(ask_price.shape)
    print(bid_size.shape)
    print(ask_size.shape)
