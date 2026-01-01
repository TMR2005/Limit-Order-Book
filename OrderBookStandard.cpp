#include <iostream>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <algorithm>
#include <vector>
#include <random>
#include <chrono>

using namespace std;


struct Order {
    uint64_t id;
    bool isBid;
    int price;
    uint64_t quantity;
    Order* next;
    Order* prev;
};

class PriceLevel {
public: 
    int price;
    uint64_t totalVolume;
    Order *head, *tail;

    PriceLevel() : price(0), totalVolume(0), head(NULL), tail(NULL) {}

    void add(Order* order, unordered_map<uint64_t, Order*>& Orders) {
        if (head) {
            tail->next = order;
            order->prev = tail;
            order->next = NULL;
            tail = order;
        } else {
            head = tail = order;
            order->next = order->prev = NULL;
        }
        totalVolume += order->quantity;
        Orders[order->id] = order;
    }

    Order* remove(uint64_t id, unordered_map<uint64_t, Order*>& Orders) {
        if (Orders.find(id) == Orders.end()) return nullptr;
        Order* temp = Orders[id];

        if (temp->prev) temp->prev->next = temp->next;
        if (temp->next) temp->next->prev = temp->prev;
        if (temp == head) head = temp->next;
        if (temp == tail) tail = temp->prev;

        totalVolume -= temp->quantity;
        Orders.erase(id);
        return temp;
    }
};

class OrderBook {
    map<int, PriceLevel*, greater<int>> bids;
    map<int, PriceLevel*> asks;
    unordered_map<uint64_t, Order*> Orders;

public:
    OrderBook() {} 

    void limitOrder(uint64_t id, bool isBid, int price, uint64_t qty) {
        if (isBid) {
            while (qty > 0 && !asks.empty() && asks.begin()->first <= price) {
                PriceLevel* bestPrice = asks.begin()->second;
                while (qty > 0 && bestPrice->head) {
                    Order* resting = bestPrice->head;
                    uint64_t tradedQty = min(qty, resting->quantity);
                    bestPrice->totalVolume -= tradedQty;
                    resting->quantity -= tradedQty;
                    qty -= tradedQty;
                    
                    if (resting->quantity == 0) {
                        Order* temp = bestPrice->remove(resting->id, Orders);
                        if(temp) delete temp; 
                    }
                }
                if (bestPrice->head == NULL) {
                    asks.erase(asks.begin());
                    delete bestPrice;
                }
            }
            if (qty > 0) {
                if (bids.find(price) == bids.end()) {
                    bids[price] = new PriceLevel();
                    bids[price]->price = price;
                }
                Order* newOrder = new Order();
                newOrder->id = id; newOrder->isBid = isBid; newOrder->price = price; 
                newOrder->quantity = qty; newOrder->next = NULL; newOrder->prev = NULL;
                bids[price]->add(newOrder, Orders);
            }
        } else {
            while (qty > 0 && !bids.empty() && bids.begin()->first >= price) {
                PriceLevel* bestPrice = bids.begin()->second;
                while (qty > 0 && bestPrice->head) {
                    Order* resting = bestPrice->head;
                    uint64_t tradedQty = min(qty, resting->quantity);
                    bestPrice->totalVolume -= tradedQty;
                    resting->quantity -= tradedQty;
                    qty -= tradedQty;
                    
                    if (resting->quantity == 0) {
                        Order* temp = bestPrice->remove(resting->id, Orders);
                        if(temp) delete temp; 
                    }
                }
                if (bestPrice->head == NULL) {
                    bids.erase(bids.begin());
                    delete bestPrice;
                }
            }
            if (qty > 0) {
                if (asks.find(price) == asks.end()) {
                    asks[price] = new PriceLevel();
                    asks[price]->price = price;
                }
                Order* newOrder = new Order();
                newOrder->id = id; newOrder->isBid = isBid; newOrder->price = price; 
                newOrder->quantity = qty; newOrder->next = NULL; newOrder->prev = NULL;
                asks[price]->add(newOrder, Orders);
            }
        }
    }

    void cancelOrder(uint64_t id) {
        auto it = Orders.find(id);
        if (it == Orders.end()) return;
        
        Order* orderToCancel = it->second;
        int price = orderToCancel->price;
        bool isBid = orderToCancel->isBid;
        PriceLevel* level = isBid ? bids[price] : asks[price];
        
        Order* temp = level->remove(id, Orders);
        if(temp) delete temp; 

        if (level->head == NULL) {
            if (isBid) bids.erase(price);
            else asks.erase(price);
            delete level;
        }
    }
};


enum class EventType { LIMIT, CANCEL };
struct Event {
    EventType type;
    uint64_t id;
    bool isBid;
    int price;
    uint64_t qty;
};

vector<Event> generateWorkload(int numEvents, double cancelRatio = 0.2) {
    vector<Event> events;
    events.reserve(numEvents);
    vector<uint64_t> liveOrders;
    liveOrders.reserve(numEvents);
    uint64_t nextId = 1;

    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    std::uniform_int_distribution<int> priceDist(90, 110);
    std::uniform_int_distribution<int> qtyDist(1, 100);

    for (int i = 0; i < numEvents; ++i) {
        if (prob(rng) < cancelRatio && !liveOrders.empty()) {
            int idx = rng() % liveOrders.size();
            events.push_back({EventType::CANCEL, liveOrders[idx], false, 0, 0});
            liveOrders[idx] = liveOrders.back();
            liveOrders.pop_back();
        } else {
            uint64_t id = nextId++;
            events.push_back({
                EventType::LIMIT, 
                id, 
                (rng() & 1) != 0, 
                priceDist(rng), 
                static_cast<uint64_t>(qtyDist(rng)) 
            });
            liveOrders.push_back(id);
        }
    }
    return events;
}

void runBenchmark(int numEvents) {
    OrderBook book; 
    auto workload = generateWorkload(numEvents);

    for (int i = 0; i < 1000; ++i) { book.limitOrder(999999 + i, true, 100, 1); }

    cout << "Running Standard (No-Pool) benchmark with " << numEvents << " events...\n";
    auto start = chrono::steady_clock::now();

    for (const auto& e : workload) {
        if (e.type == EventType::LIMIT) book.limitOrder(e.id, e.isBid, e.price, e.qty);
        else book.cancelOrder(e.id);
    }

    auto end = chrono::steady_clock::now();
    auto totalNs = chrono::duration_cast<chrono::nanoseconds>(end - start).count();

    cout << "----- STANDARD RESULTS -----\n";
    cout << "Throughput: " << (numEvents / (totalNs / 1e9)) / 1e6 << " M ops/sec\n";
    cout << "Avg latency: " << (double)totalNs / numEvents << " ns/op\n";
}

int main() {
    runBenchmark(1000000);
    return 0;
}
