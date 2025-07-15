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