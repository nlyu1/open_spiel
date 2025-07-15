// Copyright 2021 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Sample usage: build/examples/market_example

#include <iostream>
#include <memory>

#include "open_spiel/games/high_low_trading/market.h"
#include "open_spiel/spiel_utils.h"

int main(int argc, char** argv) {
  std::cout << "=== Market Example ===\n" << std::endl;

  // Create the market
  open_spiel::trade_matching::Market market;
  
  std::cout << "Initial market state:" << std::endl;
  std::cout << market.ToString() << std::endl << std::endl;

  // Step 1: Place user 0's sell order at price 10, size 2
  std::cout << "Step 1: User 0 places sell order at price 10, size 2" << std::endl;
  open_spiel::trade_matching::OrderEntry sell_order(
      10.0,  // price
      2,     // size
      1,     // tid (transaction id)
      0,     // customer_id (user 0)
      false  // is_sell_quote = false (sell order)
  );
  
  auto trades1 = market.AddOrder(sell_order);
  std::cout << "Number of trades executed: " << trades1.size() << std::endl;
  if (!trades1.empty()) {
    std::cout << trades1;
  }
  
  std::cout << "\nMarket state after step 1:" << std::endl;
  std::cout << market.ToString() << std::endl << std::endl;

  // Step 2: Place user 0's buy order at price 8, size 1
  std::cout << "Step 2: User 0 places buy order at price 8, size 1" << std::endl;
  open_spiel::trade_matching::OrderEntry buy_order1(
      8.0,  // price
      1,    // size
      2,    // tid
      0,    // customer_id (user 0)
      true  // is_sell_quote = true (buy order)
  );
  
  auto trades2 = market.AddOrder(buy_order1);
  std::cout << "Number of trades executed: " << trades2.size() << std::endl;
  if (!trades2.empty()) {
    std::cout << trades2;
  }
  
  std::cout << "\nMarket state after step 2:" << std::endl;
  std::cout << market.ToString() << std::endl << std::endl;

  // Step 3: Place user 1's buy order at price 9, size 2
  std::cout << "Step 3: User 1 places buy order at price 11, size 3" << std::endl;
  open_spiel::trade_matching::OrderEntry buy_order2(
      11.0,  // price
      3,    // size
      3,    // tid
      1,    // customer_id (user 1)
      true  // is_sell_quote = true (buy order)
  );
  
  auto trades3 = market.AddOrder(buy_order2);
  std::cout << "Number of trades executed: " << trades3.size() << std::endl;
  if (!trades3.empty()) {
    std::cout << trades3;
  }
  
  std::cout << "\nMarket state after step 3:" << std::endl;
  std::cout << market.ToString() << std::endl << std::endl;

  // Step 4: Place user 1's buy order at price 9, size 2
  std::cout << "Step 4: User 0 places sell order at price 7, size 10" << std::endl;
  open_spiel::trade_matching::OrderEntry sell_order2(
      7.0,  // price
      10,    // size
      4,    // tid
      0,    // customer_id (user 0)
      false  // is_sell_quote = false (sell order)
  );
  
  auto trades4 = market.AddOrder(sell_order2);
  std::cout << "Number of trades executed: " << trades4.size() << std::endl;
  if (!trades4.empty()) {
    std::cout << trades4;
  }
  
  std::cout << "\nMarket state after step 4:" << std::endl;
  std::cout << market.ToString() << std::endl << std::endl;

  // Display GetCustomers and GetOrders information
  std::cout << "=== Market Analysis ===" << std::endl;
  
  auto customers = market.GetCustomers();
  std::cout << "Active customers (" << customers.size() << "):" << std::endl;
  for (const auto& customer : customers) {
    std::cout << "  Customer ID: " << customer << std::endl;
    
    auto orders = market.GetOrders(customer);
    std::cout << "    Orders (" << orders.size() << "):" << std::endl;
    for (const auto& order : orders) {
      std::cout << "      " << order.ToString() << std::endl;
    }
  }
  
  std::cout << "\n=== Example Complete ===" << std::endl;
  
  return 0;
} 