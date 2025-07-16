// Copyright 2019 DeepMind Technologies Limited
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

// ================================================================================
// HIGH LOW TRADING GAME - MARKET SYSTEM
// ================================================================================
//
// OVERVIEW:
// Implements a continuous double auction market for matching buy and sell orders
// in the High Low Trading game. Orders are matched immediately when they cross
// (buy_price >= sell_price) using a price-time priority system.
//
// CORE COMPONENTS:
//
// 1. OrderEntry: Represents a single order in the market
//    - price: Bid/ask price for the contract
//    - size: Number of contracts being bought/sold  
//    - tid: Unique transaction ID for time priority
//    - customer_id: Player who placed the order
//    - is_bid: true for buy orders, false for sell orders
//
// 2. OrderFillEntry: Records the details of executed trades
//    - Contains all trade execution details including:
//      - Trade price, size, and timing information
//      - IDs of both trading parties (quoter and taker)
//      - Whether the trade crossed a buy or sell quote
//
// 3. Market: Core matching engine with the following features:
//    - Maintains separate priority queues for buy and sell orders
//    - Buy orders prioritized by highest price first, then by time
//    - Sell orders prioritized by lowest price first, then by time
//    - Immediate matching when orders cross
//    - Zero-size orders are ignored
//
// MATCHING ALGORITHM:
// 1. When a new order arrives, it's added to the appropriate queue
// 2. The system checks if the best buy price >= best sell price
// 3. If they cross, orders are matched at the quote price (not taker price)
// 4. Partial fills are supported - remaining size stays in the book
// 5. Process continues until no more matches are possible
//
// PRICE PRIORITY:
// - Buy orders: Higher prices have priority (max-heap behavior)
// - Sell orders: Lower prices have priority (min-heap behavior)  
// - Time priority: Earlier orders (lower tid) break price ties
//
// USAGE PATTERN:
// 1. Players submit orders via AddOrder()
// 2. Market immediately attempts to match against existing orders
// 3. Any fills are returned as OrderFillEntry objects
// 4. Unfilled portions remain in the order book
// 5. Market state can be queried for display/analysis
//
// TRADE EXECUTION:
// - Trade price is always the price of the resting order (quote)
// - Trade size is minimum of the two crossing orders
// - Both parties' positions and cash are updated outside this module

#pragma once 

#include <string>
#include <vector>
#include <queue>
#include <cstdint>
#include <iostream>
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace trade_matching {

typedef uint64_t customerId; 

class OrderEntry {
    public: 
        OrderEntry(double price, uint64_t size, uint64_t tid, customerId customer_id, bool is_bid) : price(price), size(size), tid(tid), customer_id(customer_id), is_bid(is_bid) {} 
        double price; 
        uint64_t size; 
        uint64_t tid; 
        customerId customer_id; 
        bool is_bid; 
    
        std::string ToString() const; 
};

class OrderFillEntry {
    public: 
        OrderFillEntry(double price, uint64_t size, uint64_t tid, uint64_t quote_size, customerId quoter_id, customerId customer_id, uint64_t quote_tid, bool is_sell_quote) : price(price), size(size), tid(tid), quote_size(quote_size), quoter_id(quoter_id), customer_id(customer_id), quote_tid(quote_tid), is_sell_quote(is_sell_quote) {} 

        double price; 
        uint64_t size; 
        bool is_sell_quote; 
        uint64_t tid; 
        uint64_t quote_size; 
        customerId quoter_id; 
        customerId customer_id; 
        uint64_t quote_tid; 
    
        std::string ToString() const; 
};

// We want a priority heap with min-price buy order or max-price sell order. 
struct OrderEntryLess {
bool operator()(const OrderEntry& a, const OrderEntry& b) const { 
    if (!(a.is_bid == b.is_bid)) {
    open_spiel::SpielFatalError("Cannot compare buy and sell orders.");
    }
    return a.is_bid? a.price < b.price : a.price > b.price; 
}
};
    
typedef std::priority_queue<OrderEntry, std::vector<OrderEntry>, OrderEntryLess> OrderQueue;
class Market {
    public: 
        Market() {}

        void ClearOrders(customerId customer_id); 
        std::vector<OrderFillEntry> AddOrder(OrderEntry order); 

        std::vector<customerId> GetCustomers(); 
        std::vector<OrderEntry> GetOrders(customerId customer_id); 

        std::string ToString() const; 

    private:
        // Checks the current stack & returns any matched orders
        std::vector<OrderFillEntry> MatchOrders(); 
        OrderQueue buy_orders_; 
        OrderQueue sell_orders_; 
};

}  // namespace trade_matching
}  // namespace open_spiel

// Stream operator for std::vector<OrderFillEntry>
std::ostream& operator<<(std::ostream& os, const std::vector<open_spiel::trade_matching::OrderFillEntry>& trades);