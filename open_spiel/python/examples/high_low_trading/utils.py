import math
from enum import Enum
from typing import List, Tuple, Union
from dataclasses import dataclass

# Default parameters matching C++ constants
DEFAULT_STEPS_PER_PLAYER = 100
DEFAULT_MAX_CONTRACTS_PER_TRADE = 5
DEFAULT_CUSTOMER_MAX_SIZE = 5
DEFAULT_MAX_CONTRACT_VALUE = 30
DEFAULT_NUM_PLAYERS = 5

@dataclass
class Config:
    steps_per_player: int = DEFAULT_STEPS_PER_PLAYER
    max_contracts_per_trade: int = DEFAULT_MAX_CONTRACTS_PER_TRADE
    customer_max_size: int = DEFAULT_CUSTOMER_MAX_SIZE
    max_contract_value: int = DEFAULT_MAX_CONTRACT_VALUE
    num_players: int = DEFAULT_NUM_PLAYERS

class GamePhase(Enum):
    CHANCE_VALUE = "kChanceValue"
    CHANCE_HIGH_LOW = "kChanceHighLow"
    CHANCE_PERMUTATION = "kChancePermutation"
    CUSTOMER_SIZE = "kCustomerSize"
    PLAYER_TRADING = "kPlayerTrading"
    TERMINAL = "kTerminal"

class PlayerRole(Enum):
    VALUE_CHEATER = "kValueCheater"
    HIGH_LOW_CHEATER = "kHighLowCheater"
    CUSTOMER = "kCustomer"

@dataclass
class ChanceContractValueAction:
    contract_value: int
    
    def __str__(self):
        return f"Environment settles one piece of contract value to {self.contract_value}"

@dataclass
class ChanceHighLowAction:
    is_high: bool
    
    def __str__(self):
        return f"Environment chooses {'high' if self.is_high else 'low'} contract settlement"

@dataclass
class PlayerQuoteAction:
    bid_size: int
    bid_price: int
    ask_size: int
    ask_price: int
    
    def __str__(self):
        return f"{self.bid_price} @ {self.ask_price} [{self.bid_size} x {self.ask_size}]"

@dataclass
class ChancePermutationAction:
    player_roles: List[PlayerRole]
    permutation: List[int]
    
    def __str__(self):
        result = "Player roles: "
        role_strs = []
        for i, role in enumerate(self.player_roles):
            role_name = role.value.replace("k", "")
            role_strs.append(f"P{i}={role_name}")
        return result + ", ".join(role_strs)

@dataclass
class ChanceCustomerSizeAction:
    customer_size: int
    
    def __str__(self):
        return f"Customer target position: {self.customer_size}"

@dataclass
class PlayerTradingAction:
    bid_size: int
    bid_price: int
    ask_size: int
    ask_price: int
    
    def __str__(self):
        return f"Price {self.bid_price} @ {self.ask_price} | Size {self.bid_size} x {self.ask_size}"

ActionVariant = Union[
    ChanceContractValueAction,
    ChanceHighLowAction,
    PlayerQuoteAction,
    ChancePermutationAction,
    ChanceCustomerSizeAction,
    PlayerTradingAction
]

def factorial(n: int) -> int:
    """Calculate factorial of n."""
    result = 1
    for i in range(1, n + 1):
        result *= i
    return result

def nth_permutation(x: int, n: int) -> List[int]:
    """Generate the x-th permutation of n elements (0-indexed)."""
    # Pre-compute factorials
    fact = [1] * (n + 1)
    for i in range(1, n + 1):
        fact[i] = fact[i - 1] * i
    
    # Generate Lehmer code digits
    lehmer = [0] * n
    for i in range(n - 1, -1, -1):
        lehmer[n - 1 - i] = x // fact[i]
        x %= fact[i]
    
    # Decode Lehmer code into permutation
    pool = list(range(n))
    perm = []
    
    for d in lehmer:
        perm.append(pool[d])
        pool.pop(d)
    
    return perm

def permutation_rank(perm: List[int]) -> int:
    """Calculate the rank of a permutation."""
    n = len(perm)
    
    # Factorial table
    fact = [1] * (n + 1)
    for i in range(1, n + 1):
        fact[i] = fact[i - 1] * i
    
    # Build mutable pool
    pool = list(range(n))
    
    # Accumulate rank
    rank = 0
    for i in range(n):
        idx = pool.index(perm[i])
        rank += idx * fact[n - 1 - i]
        pool.pop(idx)
    
    return rank

class ActionManager:
    def __init__(self, config: Config):
        self.config = config
        if config.num_players < 4:
            raise ValueError("Number of players must be at least 4")
    
    # Convenience methods for creating raw actions
    def chance_value_move(self, candidate_contract_value: int) -> int:
        """Create raw action for chance contract value move."""
        if candidate_contract_value < 1 or candidate_contract_value > self.config.max_contract_value:
            raise ValueError(f"Contract value must be between 1 and {self.config.max_contract_value}")
        structured_action = ChanceContractValueAction(candidate_contract_value)
        return self.structured_to_raw_action(GamePhase.CHANCE_VALUE, structured_action)
    
    def chance_high_low_move(self, set_to_high: bool = True) -> int:
        """Create raw action for chance high/low move."""
        structured_action = ChanceHighLowAction(set_to_high)
        return self.structured_to_raw_action(GamePhase.CHANCE_HIGH_LOW, structured_action)
    
    def chance_customer_size(self, target_customer_size: int) -> int:
        """Create raw action for chance customer size move."""
        if target_customer_size == 0:
            raise ValueError("Customer size cannot be 0")
        if abs(target_customer_size) > self.config.customer_max_size:
            raise ValueError(f"Customer size must be between -{self.config.customer_max_size} and {self.config.customer_max_size}, excluding 0")
        structured_action = ChanceCustomerSizeAction(target_customer_size)
        return self.structured_to_raw_action(GamePhase.CUSTOMER_SIZE, structured_action)
    
    def chance_permutation_move(self, permutation: List[int]) -> int:
        """Create raw action for chance permutation move."""
        if len(permutation) != self.config.num_players:
            raise ValueError(f"Permutation must have exactly {self.config.num_players} elements")
        if sorted(permutation) != list(range(self.config.num_players)):
            raise ValueError("Permutation must be a valid permutation of player indices")
        
        # Create player roles based on permutation
        player_roles = []
        for i in range(self.config.num_players):
            perm_id = permutation[i]
            if perm_id == 0 or perm_id == 1:
                player_roles.append(PlayerRole.VALUE_CHEATER)
            elif perm_id == 2:
                player_roles.append(PlayerRole.HIGH_LOW_CHEATER)
            else:
                player_roles.append(PlayerRole.CUSTOMER)
        
        structured_action = ChancePermutationAction(player_roles, permutation)
        return self.structured_to_raw_action(GamePhase.CHANCE_PERMUTATION, structured_action)
    
    def player_quote_move(self, bid_px: int, bid_sz: int, ask_px: int, ask_sz: int) -> int:
        """Create raw action for player trading quote move."""
        # Validate inputs
        if bid_px < 1 or bid_px > self.config.max_contract_value:
            raise ValueError(f"Bid price must be between 1 and {self.config.max_contract_value}")
        if ask_px < 1 or ask_px > self.config.max_contract_value:
            raise ValueError(f"Ask price must be between 1 and {self.config.max_contract_value}")
        if bid_sz < 0 or bid_sz > self.config.max_contracts_per_trade:
            raise ValueError(f"Bid size must be between 0 and {self.config.max_contracts_per_trade}")
        if ask_sz < 0 or ask_sz > self.config.max_contracts_per_trade:
            raise ValueError(f"Ask size must be between 0 and {self.config.max_contracts_per_trade}")
        
        structured_action = PlayerTradingAction(bid_sz, bid_px, ask_sz, ask_px)
        return self.structured_to_raw_action(GamePhase.PLAYER_TRADING, structured_action)
    
    def chance_trivial_permutation_move(self) -> int:
        """Create raw action for trivial permutation (identity permutation)."""
        trivial_permutation = list(range(self.config.num_players))
        return self.chance_permutation_move(trivial_permutation)
    
    def game_phase_of_timestep(self, timestep: int) -> GamePhase:
        """Determine game phase from timestep."""
        if timestep < 0:
            raise ValueError(f"Invalid timestep: {timestep}")
        elif timestep < 2:
            return GamePhase.CHANCE_VALUE
        elif timestep == 2:
            return GamePhase.CHANCE_HIGH_LOW
        elif timestep == 3:
            return GamePhase.CHANCE_PERMUTATION
        elif timestep < 4 + self.config.num_players:
            return GamePhase.CUSTOMER_SIZE
        elif timestep < 4 + self.config.num_players + self.config.steps_per_player * self.config.num_players:
            return GamePhase.PLAYER_TRADING
        else:
            return GamePhase.TERMINAL
    
    def valid_action_range(self, phase: GamePhase) -> Tuple[int, int]:
        """Get valid action range for a game phase."""
        if phase == GamePhase.CHANCE_VALUE:
            return (0, self.config.max_contract_value - 1)
        elif phase == GamePhase.CHANCE_HIGH_LOW:
            return (0, 1)
        elif phase == GamePhase.CHANCE_PERMUTATION:
            return (0, factorial(self.config.num_players) - 1)
        elif phase == GamePhase.CUSTOMER_SIZE:
            return (0, 2 * self.config.customer_max_size)
        elif phase == GamePhase.PLAYER_TRADING:
            return (0, (self.config.max_contracts_per_trade + 1) * 
                      (self.config.max_contracts_per_trade + 1) * 
                      self.config.max_contract_value * 
                      self.config.max_contract_value - 1)
        else:
            raise ValueError("Invalid terminal phase for action range")
    
    def raw_to_structured_action(self, phase: GamePhase, raw_action: int) -> ActionVariant:
        """Convert raw action to structured action."""
        min_range, max_range = self.valid_action_range(phase)
        if raw_action < min_range or raw_action > max_range:
            raise ValueError(f"Invalid raw action: {raw_action}")
        
        if phase == GamePhase.CHANCE_VALUE:
            return ChanceContractValueAction(raw_action + 1)
        
        elif phase == GamePhase.CHANCE_HIGH_LOW:
            return ChanceHighLowAction(raw_action == 1)
        
        elif phase == GamePhase.CHANCE_PERMUTATION:
            perm = nth_permutation(raw_action, self.config.num_players)
            player_roles = []
            for i in range(self.config.num_players):
                perm_id = perm[i]
                if perm_id == 0 or perm_id == 1:
                    player_roles.append(PlayerRole.VALUE_CHEATER)
                elif perm_id == 2:
                    player_roles.append(PlayerRole.HIGH_LOW_CHEATER)
                else:
                    player_roles.append(PlayerRole.CUSTOMER)
            return ChancePermutationAction(player_roles, perm)
        
        elif phase == GamePhase.CUSTOMER_SIZE:
            customer_size = raw_action - self.config.customer_max_size
            if customer_size >= 0:
                customer_size += 1
            return ChanceCustomerSizeAction(customer_size)
        
        elif phase == GamePhase.PLAYER_TRADING:
            rolling = raw_action
            bid_size_denom = ((self.config.max_contracts_per_trade + 1) * 
                            self.config.max_contract_value * 
                            self.config.max_contract_value)
            bid_size = rolling // bid_size_denom
            rolling = rolling % bid_size_denom
            
            ask_size_denom = self.config.max_contract_value * self.config.max_contract_value
            ask_size = rolling // ask_size_denom
            rolling = rolling % ask_size_denom
            
            bid_price_denom = self.config.max_contract_value
            bid_price = rolling // bid_price_denom + 1
            rolling = rolling % bid_price_denom
            ask_price = rolling + 1
            
            return PlayerTradingAction(bid_size, bid_price, ask_size, ask_price)
        
        else:
            raise ValueError("Invalid terminal phase for action conversion")
    
    def raw_to_structured_action_timestep(self, timestep: int, raw_action: int) -> ActionVariant:
        """Convert raw action to structured action using timestep."""
        return self.raw_to_structured_action(self.game_phase_of_timestep(timestep), raw_action)
    
    def structured_to_raw_action(self, phase: GamePhase, structured_action: ActionVariant) -> int:
        """Convert structured action to raw action."""
        if phase == GamePhase.CHANCE_VALUE:
            return structured_action.contract_value - 1
        
        elif phase == GamePhase.CHANCE_HIGH_LOW:
            return 1 if structured_action.is_high else 0
        
        elif phase == GamePhase.CHANCE_PERMUTATION:
            return permutation_rank(structured_action.permutation)
        
        elif phase == GamePhase.CUSTOMER_SIZE:
            customer_size = structured_action.customer_size
            adjusted_size = customer_size - 1 if customer_size > 0 else customer_size
            return adjusted_size + self.config.customer_max_size
        
        elif phase == GamePhase.PLAYER_TRADING:
            bid_size = structured_action.bid_size
            adjusted_bid_price = structured_action.bid_price - 1
            ask_size = structured_action.ask_size
            adjusted_ask_price = structured_action.ask_price - 1
            
            return (adjusted_ask_price + 
                   adjusted_bid_price * self.config.max_contract_value +
                   ask_size * self.config.max_contract_value * self.config.max_contract_value +
                   bid_size * (self.config.max_contracts_per_trade + 1) * 
                   self.config.max_contract_value * self.config.max_contract_value)
        
        else:
            raise ValueError("Invalid terminal phase for action conversion")


def test_action_manager_interface():
    """Test the new ActionManager interface methods."""
    print("Testing ActionManager interface...")
    
    # Create config for 4-player game
    config = Config(
        steps_per_player=2,
        max_contracts_per_trade=5,
        customer_max_size=5,
        max_contract_value=30,
        num_players=4
    )
    
    action_manager = ActionManager(config)
    
    # Test chance value moves
    print("\n1. Testing chance value moves:")
    for value in [10, 20]:
        raw_action = action_manager.chance_value_move(value)
        print(f"   Contract value {value} -> raw action {raw_action}")
    
    # Test chance high/low moves
    print("\n2. Testing chance high/low moves:")
    high_action = action_manager.chance_high_low_move(set_to_high=True)
    low_action = action_manager.chance_high_low_move(set_to_high=False)
    print(f"   High settlement -> raw action {high_action}")
    print(f"   Low settlement -> raw action {low_action}")
    
    # Test chance permutation moves
    print("\n3. Testing chance permutation moves:")
    trivial_perm_action = action_manager.chance_trivial_permutation_move()
    custom_perm_action = action_manager.chance_permutation_move([0, 1, 2, 3])
    print(f"   Trivial permutation [0,1,2,3] -> raw action {trivial_perm_action}")
    print(f"   Custom permutation [0,1,2,3] -> raw action {custom_perm_action}")
    
    # Test chance customer size moves
    print("\n4. Testing chance customer size moves:")
    for size in [-3, -1, 1, 3]:
        raw_action = action_manager.chance_customer_size(size)
        print(f"   Customer size {size} -> raw action {raw_action}")
    
    # Test player quote moves
    print("\n5. Testing player quote moves:")
    examples = [
        (10, 1, 11, 1),  # Bid 10@1, Ask 11@1
        (15, 2, 16, 2),  # Bid 15@2, Ask 16@2
        (5, 0, 25, 3),   # Bid 5@0, Ask 25@3 (no bid size)
    ]
    
    for bid_px, bid_sz, ask_px, ask_sz in examples:
        raw_action = action_manager.player_quote_move(bid_px, bid_sz, ask_px, ask_sz)
        print(f"   Quote {bid_px}@{bid_sz} / {ask_px}@{ask_sz} -> raw action {raw_action}")
    
    print("\nActionManager interface test completed successfully!")


if __name__ == "__main__":
    test_action_manager_interface()