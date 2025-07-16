#include "open_spiel/games/high_low_trading/market.h"
#include "open_spiel/spiel_utils.h"
#include <unordered_set>
#include <sstream>
#include <algorithm>

namespace open_spiel {
namespace trade_matching {


void Market::ClearOrders(customerId customer_id) {
    // Clear orders for the given customer from buy orders
    std::vector<OrderEntry> temp_buy_orders;
    while (!buy_orders_.empty()) {
        OrderEntry order = buy_orders_.top();
        buy_orders_.pop();
        if (order.customer_id != customer_id) {
            temp_buy_orders.push_back(order);
        }
    }
    
    // Rebuild buy orders queue without the customer's orders
    for (const auto& order : temp_buy_orders) {
        buy_orders_.push(order);
    }
    
    // Clear orders for the given customer from sell orders
    std::vector<OrderEntry> temp_sell_orders;
    while (!sell_orders_.empty()) {
        OrderEntry order = sell_orders_.top();
        sell_orders_.pop();
        if (order.customer_id != customer_id) {
            temp_sell_orders.push_back(order);
        }
    }
    
    // Rebuild sell orders queue without the customer's orders
    for (const auto& order : temp_sell_orders) {
        sell_orders_.push(order);
    }
}

std::vector<OrderFillEntry> Market::AddOrder(OrderEntry order) {
    // Ignore 0-sized orders. 
    if (order.size == 0) {
        return {};
    }
    if (order.is_bid) {
        buy_orders_.push(order);
    } else {
        sell_orders_.push(order);
    }
    return MatchOrders(); 
}

std::vector<customerId> Market::GetCustomers() {
    std::unordered_set<customerId> customer_set;
    
    // Get customers from buy orders
    OrderQueue temp_buy_orders = buy_orders_;
    while (!temp_buy_orders.empty()) {
        OrderEntry order = temp_buy_orders.top();
        temp_buy_orders.pop();
        customer_set.insert(order.customer_id);
    }
    
    // Get customers from sell orders
    OrderQueue temp_sell_orders = sell_orders_;
    while (!temp_sell_orders.empty()) {
        OrderEntry order = temp_sell_orders.top();
        temp_sell_orders.pop();
        customer_set.insert(order.customer_id);
    }
    
    // Convert set to vector
    std::vector<customerId> customers;
    customers.reserve(customer_set.size());
    for (const auto& customer_id : customer_set) {
        customers.push_back(customer_id);
    }
    
    return customers;
}

std::vector<OrderFillEntry> Market::MatchOrders() {
    std::vector<OrderFillEntry> trades; 
    while (!buy_orders_.empty() && !sell_orders_.empty()) {
        OrderEntry buy_order = buy_orders_.top();
        OrderEntry sell_order = sell_orders_.top();
        
        // If top orders don't cross, stop matching
        if (buy_order.price < sell_order.price) {
            break; 
        }
        
        // Orders cross, so pop them now
        buy_orders_.pop();
        sell_orders_.pop();
        
        // If top orders cross, execute the trade. 
        if (buy_order.tid == sell_order.tid) {
            SpielFatalError("Matched orders cannot have the same tid.");
        }
        
        bool is_sell_quote = buy_order.tid > sell_order.tid; 
        // If the ask is the quote which is crossed, execute at ask price. 
        double trade_price = is_sell_quote ? sell_order.price : buy_order.price; 
        uint64_t trade_size = std::min(buy_order.size, sell_order.size); 
        uint64_t tid = is_sell_quote ? sell_order.tid : buy_order.tid; 
        uint64_t quote_size = is_sell_quote ? sell_order.size : buy_order.size; 
        customerId quoter_id = is_sell_quote ? sell_order.customer_id : buy_order.customer_id; 
        customerId customer_id = is_sell_quote ? buy_order.customer_id : sell_order.customer_id; 
        uint64_t quote_tid = is_sell_quote ? sell_order.tid : buy_order.tid; 
        
        trades.push_back(OrderFillEntry(trade_price, trade_size, tid, quote_size, quoter_id, customer_id, quote_tid, is_sell_quote));
        
        // Calculate remaining orders and push back if necessary 
        uint64_t remaining_sell_size = sell_order.size - trade_size; 
        uint64_t remaining_buy_size = buy_order.size - trade_size; 

        if (remaining_sell_size > 0) {
            sell_orders_.push(OrderEntry(sell_order.price, remaining_sell_size, sell_order.tid, sell_order.customer_id, false));
        }
        if (remaining_buy_size > 0) {
            buy_orders_.push(OrderEntry(buy_order.price, remaining_buy_size, buy_order.tid, buy_order.customer_id, true));
        }
    }
    return trades; 
}

std::vector<OrderEntry> Market::GetOrders(customerId customer_id) {
    std::vector<OrderEntry> customer_orders;
    
    // Get orders from buy orders
    OrderQueue temp_buy_orders = buy_orders_;
    while (!temp_buy_orders.empty()) {
        OrderEntry order = temp_buy_orders.top();
        temp_buy_orders.pop();
        if (order.customer_id == customer_id) {
            customer_orders.push_back(order);
        }
    }
    
    // Get orders from sell orders
    OrderQueue temp_sell_orders = sell_orders_;
    while (!temp_sell_orders.empty()) {
        OrderEntry order = temp_sell_orders.top();
        temp_sell_orders.pop();
        if (order.customer_id == customer_id) {
            customer_orders.push_back(order);
        }
    }
    
    return customer_orders;
}

std::string OrderEntry::ToString() const {
    // Specification: "sz {size} @ px {price}   id={user id}"
    std::ostringstream oss;
    oss << "sz " << size << " @ px " << price << "   id=" << customer_id << " @ t=" << tid;
    return oss.str();
}

std::string OrderFillEntry::ToString() const {
    std::ostringstream oss;
    oss << "sz " << size << " @ px " << price << " on t=" << tid 
        << ". User " << customer_id << " crossed with user " << quoter_id 
        << "'s quote sz " << quote_size << " @ px " << price;
    return oss.str();
}

std::string Market::ToString() const{
    // Specification: 
    // #### Sell orders (number of sell orders)#### 
    // Highest sell order: to_string() of that order 
    // .... 
    // Lowest sell order: to_string() of that order 
    // #############################
    // #### Buy orders (number) #### 
    // Highest buy order: to_string() of that order 
    // ....
    // Lowest buy order: to_string() of that order 
    // #############################
    
    std::ostringstream oss;
    
    // Extract all sell orders
    std::vector<OrderEntry> sell_orders_vec;
    OrderQueue temp_sell_orders = sell_orders_;
    while (!temp_sell_orders.empty()) {
        sell_orders_vec.push_back(temp_sell_orders.top());
        temp_sell_orders.pop();
    }
    
    // For sell orders, we need to reverse the order since priority queue gives us lowest price first
    // but we want highest to lowest price
    std::reverse(sell_orders_vec.begin(), sell_orders_vec.end());
    
    // Display sell orders
    oss << "####### " << sell_orders_vec.size() << " sell orders #######\n";
    for (const auto& order : sell_orders_vec) {
        oss << order.ToString() << "\n";
    }
    oss << "#############################\n";
    
    // Extract all buy orders
    std::vector<OrderEntry> buy_orders_vec;
    OrderQueue temp_buy_orders = buy_orders_;
    while (!temp_buy_orders.empty()) {
        buy_orders_vec.push_back(temp_buy_orders.top());
        temp_buy_orders.pop();
    }
    
    // For buy orders, the priority queue already gives us highest price first, which is what we want
    
    // Display buy orders
    oss << "####### " << buy_orders_vec.size() << " buy orders #######\n";
    for (const auto& order : buy_orders_vec) {
        oss << order.ToString() << "\n";
    }
    oss << "#############################";
    
    return oss.str();
}

}  // namespace trade_matching
}  // namespace open_spiel

// Stream operator for std::vector<OrderFillEntry>
std::ostream& operator<<(std::ostream& os, const std::vector<open_spiel::trade_matching::OrderFillEntry>& trades) {
    os << "############# Trade entries #############\n";
    for (size_t i = 0; i < trades.size(); ++i) {
        os << (i + 1) << ". " << trades[i].ToString() << "\n";
    }
    os << "#########################################\n";
    return os;
}
