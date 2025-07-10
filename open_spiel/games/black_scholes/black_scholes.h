#ifndef OPEN_SPIEL_GAMES_BLACKSCHOLES_H_
#define OPEN_SPIEL_GAMES_BLACKSCHOLES_H_

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
//   [delta_t, mu, sigma, strike_price, maxTimeSteps, t, stock_holding, cash_holding, contract_holding, stock_price]
inline constexpr const int kStateEncodingSize = 10;
class BlackScholesGame;

// We count in pennies 
inline constexpr double kDefaultDeltaT = 0.01; 
inline constexpr double kDefaultMvp = 0.01; 
inline constexpr int kDefaultMaxTimeSteps = 20; 
inline constexpr double kDefaultSigma = 1.0; 
inline constexpr double kDefaultMu = 0.0;
inline constexpr double kDefaultStrikePrice = 1000.0;

inline constexpr int kDefaultMaxContracts = 100;
inline constexpr int kDefaultMaxSharesPerContract = 100;
inline constexpr double kDefaultInitialPrice = 1000; 

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
  double evaluate_payout(double stock_price, double strike_price) const;
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
};

}  // namespace black_scholes
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_BLACKSCHOLES_H_
