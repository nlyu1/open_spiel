#ifndef OPEN_SPIEL_GAMES_BLACKSCHOLES_H_
#define OPEN_SPIEL_GAMES_BLACKSCHOLES_H_

/*
 * BLACK-SCHOLES OPTION TRADING GAME
 * 
 * This is a single-player sequential game that simulates option trading in a 
 * Black-Scholes market environment. The player makes trading decisions while
 * the environment updates stock prices according to a geometric Brownian motion.
 * 
 * GAME MECHANICS:
 * - The game alternates between player moves (even timesteps) and environment 
 *   moves (odd timesteps)
 * - Player moves: Buy/sell stocks and option contracts
 * - Environment moves: Update stock prices and apply interest to cash holdings
 * - Game terminates after max_time_steps * 2 total moves
 * - Final utility is the total portfolio value at termination
 * 
 * STOCK PRICE DYNAMICS:
 * Stock prices follow a discrete-time geometric Brownian motion:
 * S(t+1) = S(t) * exp((mu + sigma * Z) * delta_t)
 * where Z is +1 or -1 with equal probability (50% each)
 * 
 * PORTFOLIO COMPONENTS:
 * 1. Stock Holdings: Number of shares owned (can be negative for short positions)
 * 2. Cash Holdings: Cash balance (earns interest at risk-free rate)
 * 3. Contract Holdings: Number of European call option contracts (can be negative for short positions)
 * 
 * OPTION CONTRACTS:
 * - European call options with payoff: max(S - strike_price, 0)
 * - Premium must be paid upfront when buying options
 * - Contracts can be bought (positive) or sold/written (negative)
 * 
 * GAME PARAMETERS:
 * 
 * sigma (default: 1.0):
 *   Volatility parameter in the stock price model. Higher values increase 
 *   price fluctuations. Used in the exponential: exp(sigma * Z * delta_t)
 * 
 * mu (default: 0.0):
 *   Drift parameter representing the expected return rate of the stock.
 *   Used in the exponential: exp(mu * delta_t). Positive values indicate
 *   upward price trend, negative values indicate downward trend.
 * 
 * delta_t (default: 0.1):
 *   Time step size for each environment move. Smaller values make the 
 *   price evolution more granular. Affects both stock price updates and
 *   interest rate compounding.
 * 
 * max_time_steps (default: 20):
 *   Maximum number of time periods in the game. The total game length is
 *   max_time_steps * 2 (since each period has both player and environment moves).
 * 
 * max_contracts (default: 100):
 *   Maximum number of option contracts that can be held in either direction.
 *   Range: [-max_contracts, +max_contracts]
 * 
 * max_shares_per_contract (default: 100):
 *   Maximum number of shares that can be traded per contract unit.
 *   Total max shares = max_contracts * max_shares_per_contract
 * 
 * initial_price (default: 1000.0):
 *   Starting stock price at t=0. This is the reference point for all
 *   subsequent price movements.
 * 
 * strike_price (default: 1000.0):
 *   Strike price for the European call options. Options are in-the-money
 *   when stock_price > strike_price, with payoff = stock_price - strike_price.
 * 
 * premium_price (default: 100.0):
 *   Premium paid per option contract. This is the upfront cost when buying
 *   options or the income received when selling/writing options.
 * 
 * interest_rate (default: 0.0):
 *   Risk-free interest rate applied to cash holdings. Cash is compounded
 *   each time step: cash(t+1) = cash(t) * exp(interest_rate * delta_t)
 * 
 * OBSERVATION ENCODING:
 * The state is encoded as a 12-dimensional vector:
 * [stock_holding, cash_holding, contract_holding, strike_price, stock_price, 
 *  premium, delta_t, mu, sigma, interest_rate, t/maxTimeSteps, maxTimeSteps]
 * 
 * ACTION SPACE:
 * On the first timestep: (2*max_shares + 1) * (2*max_contracts + 1) actions
 * On subsequent timesteps: (2*max_shares + 1) actions (stock trades only)
 * Actions encode both stock and contract purchase deltas.
 * 
 * TERMINAL UTILITY:
 * Portfolio value = stock_value + cash + option_payoff
 * where option_payoff = contract_holding * max(0, stock_price - strike_price)
 */

#include <array>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "open_spiel/spiel.h"

namespace open_spiel {
namespace black_scholes {

inline constexpr const int kNumPlayers = 1;

// Observation encoding: 
// [stock_holding, cash_holding, contract_holding] + [strike_price, stock_price, premium] + [delta_t, mu, sigma, interest_rate] + [t, maxTimeSteps]
inline constexpr const int kStateEncodingSize = 12;
class BlackScholesGame;

// Exponential is calculated as (t<MaxTimeSteps) * deltaT * (sigma Z + mu) 
// max_shares = max_shares_per_contract * max_contracts. 
// Max_shares and max_shares_per_contract are both inclusive 
inline constexpr double kDefaultDeltaT = 0.1; 
inline constexpr int kDefaultMaxTimeSteps = 20; 
inline constexpr double kDefaultSigma = 1.0; 
inline constexpr double kDefaultMu = 0.0;
inline constexpr double kDefaultStrikePrice = 1000.0;
inline constexpr double kDefaultPremium = 100.0; 

inline constexpr int kDefaultMaxContracts = 100;
inline constexpr int kDefaultMaxSharesPerContract = 100;
inline constexpr double kDefaultInitialPrice = 1000; 
inline constexpr double kDefaultInterestRate = 0.0; 

// EV of exp[\sigma Z + \mu] = exp(\mu + \sigma^2/2)
// Var of exp[\sigma Z + \mu] = exp(2\mu + \sigma^2) * (exp(\sigma^2) - 1)

// Rules of the Black-Scholes game: 
// Inputs: mu=1., sigma, strike price, mvp=0.01, delta_t=0.01, maxTimeSteps=20
// At t=0, price of stock is 1000. Agent sees (mu, \sigma, strike price, t=0, stock_holding=0, cash_holding=0, contract_holding=0,S_0=1000)
//    Agent makes decision to buy [-maxSharesPerContract*maxContracts, maxSharesPerContract*maxContracts] shares of stock and [-maxContracts, maxContracts] of contracts at strike price. Cash is used to finance stock. Everything is reported in mvp using ints. 
// For each time step t, environment samples Z\sim Ber(p): 
//    - Stock price is multiplied by exp(mu + sigma * Z)
// Environment reports: (mu, \sigma, strike price, t=0, stock_holding=0, cash_holding=0, contract_holding=0,S_0=1000). 
// Players move on timestep % 2 == 0. Environment moves on timestep % 2 == 1

class Portfolio {
  public:
    Portfolio(int stock_holding=0, double cash_holding=0.0, double contract_holding=0.0)
      : stock_holding_(stock_holding), cash_holding_(cash_holding), contract_holding_(contract_holding) {}
  double evaluate_payout(double stock_price, double strike_price, double premium) const;
  std::string ToString() const; 
  // Finance stock by selling cash; stock_delta can be negative 
  Portfolio finance_stock(float stock_delta, float stock_price) const; 

  int stock_holding_; 
  double cash_holding_; 
  double contract_holding_; 
}; 

class BlackScholesState : public State {
 public:
  BlackScholesState(const BlackScholesState&) = default;
  BlackScholesState(std::shared_ptr<const Game> game);

  Player CurrentPlayer() const override;
  void UndoAction(Player player, Action action) override;
  std::vector<Action> LegalActions() const override;
  std::string ActionToString(Player player, Action move_id) const override;
  std::vector<std::pair<Action, double>> ChanceOutcomes() const override;
  std::string ToString() const override;
  bool IsTerminal() const override;
  std::vector<double> Returns() const override;
  std::string ObservationString(Player player) const override;
  void ObservationTensor(Player player,
                         absl::Span<float> values) const override;
  std::unique_ptr<State> Clone() const override;

 protected:
  void DoApplyAction(Action move_id) override;

 private:
  const BlackScholesGame* game_; 
  int timestep_; 
  double stock_price_; 
  Portfolio portfolio_; 
};

class BlackScholesGame : public Game {
 public:
  explicit BlackScholesGame(const GameParameters& params);

  int NumDistinctActions() const override; 

  std::unique_ptr<State> NewInitialState() const override {
    return std::unique_ptr<State>(new BlackScholesState(shared_from_this()));
  }

  // Up and down for each node 
  int MaxChanceOutcomes() const override { return 2; }

  // Two moves (environment, player) per timestep; 
  int MaxGameLength() const override { return maxTimeSteps_ * 2; }

  // Number of chance moves by nature: there's 1 at the beginning and one up / down move for each intermediate node. 
  int MaxChanceNodesInHistory() const override { return MaxGameLength(); }

  int NumPlayers() const override { return kNumPlayers; }
  double MinUtility() const override { return -MaxUtility(); }
  absl::optional<double> UtilitySum() const override { return 0; }
  double MaxUtility() const override;

  std::vector<int> ObservationTensorShape() const override {
    return {kStateEncodingSize};
  }

  // Getter methods for accessing private members
  double GetSigma() const { return sigma_; }
  double GetMu() const { return mu_; }
  double GetDeltaT() const { return deltaT_; }
  double GetStrikePrice() const { return strike_price_; }
  int GetMaxTimeSteps() const { return maxTimeSteps_; }
  int GetMaxContracts() const { return maxContracts_; }
  int GetMaxSharesPerContract() const { return maxSharesPerContract_; }
  int GetMaxShares() const { return maxShares_; }
  double GetInitialPrice() const { return initialPrice_; }
  double GetPremium() const { return premium_; }
  double GetInterestRate() const { return interest_rate_; }
  std::pair<int, int> convert_action_to_deltas(Action action_id) const; 

  private:
    double deltaT_; 
    double strike_price_;  
    int maxTimeSteps_; 
    double sigma_; 
    double mu_;
    int maxContracts_;
    int maxSharesPerContract_;
    int maxShares_;
    double initialPrice_;
    double premium_; 
    double interest_rate_; 
};

}  // namespace black_scholes
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_BLACKSCHOLES_H_
