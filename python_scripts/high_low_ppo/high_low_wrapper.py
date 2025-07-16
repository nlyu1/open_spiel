import torch 
import pyspiel
import numpy as np 
from typing import List, Tuple, Optional, Dict, Any
from jaxtyping import Float, Int, Array

# Default configuration for the High Low Trading game
default_config: Dict[str, int] = {
    'steps_per_player': 10,           # Number of trading steps per player
    'max_contracts_per_trade': 2,     # Maximum contracts in a single trade
    'customer_max_size': 5,           # Maximum position size for customers
    'max_contract_value': 30,         # Maximum value a contract can have
    'players': 5                      # Total number of players in the game
}

class HighLowTradingEnv:
    """
    Python wrapper for OpenSpiel's High Low Trading game.
    
    This environment provides a standardized interface for the multi-player trading game
    where players with asymmetric information trade contracts that settle at either
    high or low values based on randomly drawn contract values.
    
    Player Roles:
        - ValueCheaters (2): Know one of the candidate contract values
        - HighLowCheater (1): Knows whether settlement will be "high" or "low"
        - Customers (remaining): Have target positions they want to achieve
    
    Information State Tensor Structure (per player):
        - Game setup & private info (11 elements):
            * Game parameters (5): [steps_per_player, max_contracts_per_trade, 
                                   customer_max_size, max_contract_value, num_players]
            * One-hot player role (3): [is_value_cheater, is_high_low_cheater, is_customer]
            * Player ID encoding (2): [sin(2π*id/players), cos(2π*id/players)]
            * Private information (1): [contract_value | high_low_signal | target_position]
        - Public information (dynamic):
            * Player positions (num_players * 2): [contracts, cash] for each player
            * Historical quotes (num_quotes * 6): [bid_px, ask_px, bid_sz, ask_sz, 
                                                  sin(2π*player_id/players), cos(...)]
    """
    
    def __init__(self, game_config: Dict[str, int] = default_config, seed: int = 0) -> None:
        """
        Initialize the High Low Trading environment.
        
        Args:
            game_config: Dictionary containing game configuration parameters.
                        Keys: 'steps_per_player', 'max_contracts_per_trade', 
                              'customer_max_size', 'max_contract_value', 'players'
            seed: Random seed for reproducible private information generation.
                 Note: Same seed yields identical private information across rollouts.
        """
        self.game = pyspiel.load_game(
            f"high_low_trading({','.join([f'{k}={v}' for k, v in game_config.items()])})"
        )
        self.config = game_config 
        self.num_customers: int = self.config['players'] - 3  # Total players minus 3 cheaters
        self.num_players: int = self.config['players'] 
        self.rng = np.random.default_rng(seed)
        self.reset()
        self.observation_shape = np.array(self.state.information_state_tensor(0)).shape

    def reset(self) -> None:
        """
        Reset the trading environment and randomly initialize private information.
        
        This method:
        1. Creates a new initial game state
        2. Randomly initializes contract values (2 values)
        3. Randomly chooses high/low settlement
        4. Randomly permutes player roles
        5. Assigns random target positions to customers
        """
        self.state = self.game.new_initial_state()

        # Initialize the environment through chance moves:
        # 1. Initialize contract value 1 
        self.state.apply_action(
            self.rng.choice(self.state.legal_actions()))
        # 2. Initialize contract value 2 
        self.state.apply_action(
            self.rng.choice(self.state.legal_actions()))
        # 3. Initialize settlement to high or low 
        self.state.apply_action(
            self.rng.choice(self.state.legal_actions()))
        # 4. Initialize player permutation (assigns roles)
        self.state.apply_action(
            self.rng.choice(self.state.legal_actions()))
        # 5. For each customer, initialize target position 
        for _ in range(self.num_customers):
            self.state.apply_action(
                self.rng.choice(self.state.legal_actions()))
    
    def current_player(self) -> int:
        """
        Get the player whose turn it is to act.
        
        Returns:
            Player ID (0 to num_players-1) or special values:
            - kTerminalPlayerId: Game has ended
            - kChancePlayerId: Chance/environment move
        """
        return self.state.current_player()
    
    def player_observation_tensors(self) -> np.ndarray:
        """
        Get observation tensors for all players.
        
        Returns:
            Observation tensor of shape (num_players, observation_dim). Each tensor contains:
            - Game configuration and private information
            - Current player positions
            - Historical trading quotes
        """
        return np.stack([self.state.information_state_tensor(i) for i in range(self.num_players)])
    
    def player_observation_strings(self) -> List[str]:
        """
        Get human-readable observation strings for all players.
        
        Returns:
            List of observation strings showing each player's private information
            and the public game state from their perspective.
        """
        return [self.state.observation_string(i) for i in range(self.num_players)]
    
    def _structured_to_raw_action(
        self, 
        bid_price: int, 
        bid_size: int, 
        ask_price: int, 
        ask_size: int
    ) -> int:
        """
        Convert structured quote action to raw action ID.
        
        This implements the same logic as ActionManager::StructuredToRawAction
        in action_manager.cc for GamePhase::kPlayerTrading.
        
        Args:
            bid_price: Price at which player is willing to buy (1 to max_contract_value)
            bid_size: Number of contracts to buy (0 to max_contracts_per_trade)
            ask_price: Price at which player is willing to sell (1 to max_contract_value)
            ask_size: Number of contracts to sell (0 to max_contracts_per_trade)
            
        Returns:
            Raw action ID that encodes the structured quote action.
            
        Formula:
            raw_action = adjusted_ask_price + 
                        adjusted_bid_price * max_contract_value +
                        ask_size * max_contract_value^2 +
                        bid_size * (max_contracts_per_trade + 1) * max_contract_value^2
            
            Where adjusted_prices = original_price - 1 (since prices start at 1, not 0)
        """
        # Prices are adjusted by -1 because bidding 0 price is not allowed,
        # but the raw action encoding starts from 0
        adjusted_bid_price = bid_price - 1
        adjusted_ask_price = ask_price - 1
        
        max_contract_value = self.config['max_contract_value']
        max_contracts_per_trade = self.config['max_contracts_per_trade']
        
        # Encode according to the formula in action_manager.cc
        raw_action = (
            adjusted_ask_price + 
            adjusted_bid_price * max_contract_value + 
            ask_size * max_contract_value * max_contract_value + 
            bid_size * (max_contracts_per_trade + 1) * max_contract_value * max_contract_value
        )
        
        return raw_action

    def _raw_to_structured_action(self, raw_action: int) -> Tuple[int, int, int, int]:
        """
        Convert raw action ID back to structured quote action.
        
        This implements the same logic as ActionManager::RawToStructuredAction
        in action_manager.cc for GamePhase::kPlayerTrading.
        
        Args:
            raw_action: Raw action ID to decode
            
        Returns:
            Tuple of (bid_price, bid_size, ask_price, ask_size)
            
        Note:
            This is the inverse of _structured_to_raw_action and can be used
            for debugging or testing purposes.
        """
        max_contract_value = self.config['max_contract_value']
        max_contracts_per_trade = self.config['max_contracts_per_trade']
        
        # Decode using the same logic as in action_manager.cc
        rolling = raw_action
        
        # Extract bid_size (highest order component)
        bid_size_denom = (max_contracts_per_trade + 1) * max_contract_value * max_contract_value
        bid_size = rolling // bid_size_denom
        rolling = rolling % bid_size_denom
        
        # Extract ask_size
        ask_size_denom = max_contract_value * max_contract_value
        ask_size = rolling // ask_size_denom
        rolling = rolling % ask_size_denom
        
        # Extract bid_price (add 1 because prices start at 1, not 0)
        bid_price_denom = max_contract_value
        bid_price = rolling // bid_price_denom + 1
        rolling = rolling % bid_price_denom
        
        # Extract ask_price (add 1 because prices start at 1, not 0)
        ask_price = rolling + 1
        
        return bid_price, bid_size, ask_price, ask_size

    def step(
        self, 
        bid_price: int, 
        ask_price: int, 
        bid_size: int = 0, 
        ask_size: int = 0
    ) -> Tuple[
        List[Float[Array, "observation_dim"]], 
        int, 
        List[float], 
        bool
    ]:
        """
        Execute one trading step in the environment.
        
        Args:
            bid_price: Price at which the current player is willing to buy contracts.
                      Must be > 0 and <= max_contract_value.
            bid_size: Number of contracts the player wants to buy.
                     Must be >= 0 and <= max_contracts_per_trade.
            ask_price: Price at which the current player is willing to sell contracts.
                      Must be > 0 and <= max_contract_value.
            ask_size: Number of contracts the player wants to sell.
                     Must be >= 0 and <= max_contracts_per_trade.
        
        Returns:
            observations: List of observation tensors for all players after the action.
            next_player: ID of the player to act next.
            rewards: Numpy array immediate rewards for all players from this step.
            is_terminated: Whether the game has ended. If True, observations 
                          represent the initial state of a new game.
        
        Raises:
            AssertionError: If the game is already terminal or if action parameters
                           violate game constraints.
        """
        assert not self.state.is_terminal(), "Cannot step in a terminal state"
        assert min(bid_price, ask_price) > 0, "Bid and ask price have to be positive"
        assert min(bid_size, ask_size) >= 0, "Bid and ask sizes have to be nonnegative"
        assert max(bid_price, ask_price) <= self.config['max_contract_value'], \
            "Bid and ask prices have to be less than or equal to max contract value"
        assert max(bid_size, ask_size) <= self.config['max_contracts_per_trade'], \
            "Bid and ask sizes have to be less than or equal to max contracts per trade"
        
        # Convert structured quote action to raw action using the same logic
        # as ActionManager::StructuredToRawAction in action_manager.cc
        raw_action_id = self._structured_to_raw_action(bid_price, bid_size, ask_price, ask_size)
        self.state.apply_action(raw_action_id) 

        rewards = np.array(self.state.rewards())
        is_terminated = self.state.is_terminal()
        
        if is_terminated:
            self.reset()

        next_player = self.state.current_player()
        next_observations = self.player_observation_tensors()
        
        return next_observations, next_player, rewards, is_terminated 

class HighLowTradingEnvVector:
    """
    Vectorized wrapper for multiple HighLowTradingEnv instances.
    
    This class manages multiple parallel environments for efficient batch training.
    Each environment uses a different seed to ensure diverse experiences.
    
    Args:
        game_config: Configuration dictionary for all environments
        num_envs: Number of parallel environments to create
        seed: Base seed; environments will use seeds [seed, seed+1, ..., seed+num_envs-1]
    """
    
    def __init__(self, game_config: Dict[str, int] = default_config, num_envs: int = 1, seed: int = 0) -> None:
        """
        Initialize vectorized environment with multiple parallel instances.
        
        Args:
            game_config: Dictionary containing game configuration parameters
            num_envs: Number of parallel environments to create
            seed: Base random seed; each environment gets seed + env_index
        """
        self.num_envs = num_envs
        self.envs = [HighLowTradingEnv(game_config, seed + i) for i in range(num_envs)]
        self.num_players = self.envs[0].num_players
        self.observation_shape = self.envs[0].observation_shape
    
    def reset(self) -> None:
        """
        Reset all environments in the vector.
        """
        for env in self.envs:
            env.reset()
    
    def current_player(self) -> np.ndarray:
        """
        Get the current player for each environment.
        
        Returns:
            Array of shape (num_envs,) containing current player IDs for each environment
        """
        return np.array([env.current_player() for env in self.envs])
    
    def step(
        self, 
        bid_prices: np.ndarray, 
        ask_prices: np.ndarray, 
        bid_sizes: Optional[np.ndarray] = None, 
        ask_sizes: Optional[np.ndarray] = None
    ) -> Tuple[
        List[np.ndarray], 
        np.ndarray, 
        np.ndarray, 
        np.ndarray
    ]:
        """
        Execute vectorized trading actions across all environments.
        
        Args:
            bid_prices: Array of shape (num_envs,) with bid prices for each environment
            ask_prices: Array of shape (num_envs,) with ask prices for each environment  
            bid_sizes: Array of shape (num_envs,) with bid sizes for each environment (default: zeros)
            ask_sizes: Array of shape (num_envs,) with ask sizes for each environment (default: zeros)
        
        Returns:
            observations: List of arrays, where observations[player_idx] has shape 
                         (num_envs, observation_dim) containing observations for 
                         player_idx across all environments
            next_players: Array of shape (num_envs,) with next player IDs for each environment
            rewards: Array of shape (num_envs, num_players) with rewards for each 
                    environment and player
            is_terminated: Array of shape (num_envs,) with termination status for each environment
        """
        # Default to zero sizes if not provided
        if bid_sizes is None:
            bid_sizes = np.zeros(self.num_envs, dtype=int)
        if ask_sizes is None:
            ask_sizes = np.zeros(self.num_envs, dtype=int)
            
        # Validate input shapes
        assert bid_prices.shape == (self.num_envs,), f"bid_prices must have shape ({self.num_envs},), got {bid_prices.shape}"
        assert ask_prices.shape == (self.num_envs,), f"ask_prices must have shape ({self.num_envs},), got {ask_prices.shape}"
        assert bid_sizes.shape == (self.num_envs,), f"bid_sizes must have shape ({self.num_envs},), got {bid_sizes.shape}"
        assert ask_sizes.shape == (self.num_envs,), f"ask_sizes must have shape ({self.num_envs},), got {ask_sizes.shape}"
        
        # Apply different actions to each environment
        results = [
            env.step(
                bid_price=int(bid_prices[i]), 
                ask_price=int(ask_prices[i]), 
                bid_size=int(bid_sizes[i]), 
                ask_size=int(ask_sizes[i])
            ) 
            for i, env in enumerate(self.envs)
        ]
        
        # Unpack results
        all_observations, all_next_players, all_rewards, all_terminated = zip(*results)
        
        # Batch observations: convert from list of (list of arrays) to (list of batched arrays)
        # all_observations[env_idx][player_idx] -> batched_observations[player_idx][env_idx]
        batched_observations = []
        for player_idx in range(self.num_players):
            player_obs_batch = np.stack([all_observations[env_idx][player_idx] for env_idx in range(self.num_envs)])
            batched_observations.append(player_obs_batch)
        
        # Batch other outputs
        batched_next_players = np.array(all_next_players)
        batched_rewards = np.array(all_rewards)
        batched_terminated = np.array(all_terminated)
        
        return batched_observations, batched_next_players, batched_rewards, batched_terminated 
    
    def player_observation_tensors(self) -> np.ndarray:
        """
        Get observation tensors for all players.
        
        Returns:
            Observation tensor of shape (num_envs, num_players, observation_dim). Each tensor contains:
            - Game configuration and private information
            - Current player positions
            - Historical trading quotes
        """
        return np.stack([self.envs[i].player_observation_tensors() for i in range(self.num_envs)])
    

class SinglePlayerHighLowTradingEnvVector:
    def __init__(self, game_config: Dict[str, int] = default_config, num_envs: int = 1, seed: int = 0, agents: List = None):
        # Creates a single environment from player 0's POV, fixing other agents

        self.vector_env = HighLowTradingEnvVector(game_config, num_envs, seed)
        # Assumes that agents have `sample_actions` method which accepts batch of observations and returns sampled actions
        self.agents = agents 
        self.num_other_agents = game_config['players'] - 1
        assert len(agents) == self.num_other_agents, "Number of agents must match number of other agents"

    def reset(self):
        self.vector_env.reset() 

    def current_observation(self) -> np.ndarray:
        """
        Returns:
            Observation tensor of shape (num_envs, observation_dim)
        """
        return self.vector_env.player_observation_tensors()[:, 0]

    def step(self, bid_prices: np.ndarray, ask_prices: np.ndarray, bid_sizes: np.ndarray, ask_sizes: np.ndarray):
        """
        Returns:
            next_obs: Observation tensor of shape (num_envs, observation_dim)
            rewards: Rewards tensor of shape (num_envs,)
            is_terminated: Termination tensor of shape (num_envs,)
        """
        # Assert that current player starts 
        for env in self.vector_env.envs:
            assert env.current_player() == 0, "Environment must start with player 0"
        
        next_obs, next_player, rewards, is_terminated = self.vector_env.step(bid_prices, ask_prices, bid_sizes, ask_sizes)
        assert not is_terminated[0], "Game must not be terminated on player 0's turn"

        while next_player[0] != 0 and not is_terminated[0]: 
            next_player = next_player[0]
            assert (next_player.std() < 1e-3), "Current player must be the same across all environments"
            assert (is_terminated.std() < 1e-3), "Game must be terminated across all environments"

            npc_bid_prices, npc_ask_prices, npc_bid_sizes, npc_ask_sizes = self.agents[next_player](next_obs[:, next_player])
            next_obs, next_player, env_step_rewards, is_terminated = self.vector_env.step(npc_bid_prices, npc_ask_prices, npc_bid_sizes, npc_ask_sizes)
            rewards += env_step_rewards

        if is_terminated[0]: 
            self.reset()
            next_obs = self.vector_env.player_observation_tensors()

        return next_obs[:, 0], rewards[:, 0], is_terminated