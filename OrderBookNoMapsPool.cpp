#include <iostream>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <algorithm>
#include <vector>
#include <cassert>
#include <cstring>

using namespace std;

static constexpr int MAX_PRICE =  200000;

struct Order {
    uint64_t id;
    bool isBid;
    int price;
    uint64_t quantity;
    Order* next;
    Order* prev;
};

class OrderPool{
public:
    Order* freeListHead;
    vector<Order*> chunks;
    size_t poolSize;

    OrderPool(size_t initialSize =  100000) : poolSize(initialSize), freeListHead(nullptr){
        allocateChunk(poolSize);
    }
    
    void allocateChunk(size_t size){
        Order* newChunk = new Order[size];
        chunks.push_back(newChunk);
        for(int i=0;i<size-1;i++){
            newChunk[i].next = &newChunk[i+1];
        }
        newChunk[size-1].next = freeListHead;
        freeListHead = &newChunk[0];
    }

    Order* acquire(){
        if(!freeListHead) allocateChunk(poolSize);
        Order* order = freeListHead;
        freeListHead = freeListHead->next;
        order->next = NULL;
        order->prev = NULL;
        return order;
    }

    void release(Order* order){
        order->next = freeListHead;
        freeListHead = order;
    }

    ~OrderPool() {
        for (size_t i = 0; i < chunks.size(); ++i) {
            delete[] chunks[i];
        }
    }
};


class PriceLevel {
public: 
    int price;
    uint64_t totalVolume;
    Order *head, *tail;

    PriceLevel() : price(0), totalVolume(0), head(NULL), tail(NULL) {}

    void add(Order* order, unordered_map<uint64_t, Order*>& Orders) {
        assert(order);
        assert(order->quantity > 0);
        assert(!order->next && !order->prev);

        if (head) {
            tail->next = order;
            order->prev = tail;
            order->next = NULL;
            tail = order;
        } else {
            head = order;
            tail = order;
            order->next = NULL;
            order->prev = NULL;
        }
        totalVolume += order->quantity;
        Orders[order->id] = order;
    }

    Order* remove(uint64_t id, unordered_map<uint64_t, Order*>& Orders) {
        if (Orders.find(id) == Orders.end()) return nullptr;
        Order* temp = Orders[id];

        assert(temp);
        assert(temp->quantity > 0);

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
    OrderPool& pool;
    // map<int, PriceLevel*, greater<int> > bids;
    // map<int, PriceLevel*> asks;
    PriceLevel* bids[MAX_PRICE];
    PriceLevel* asks[MAX_PRICE];
    int bid ;
    int ask ;
    unordered_map<uint64_t, Order*> Orders;

    

public:
    OrderBook(OrderPool& p) : pool(p), bid(-1), ask(MAX_PRICE) {
        memset(bids, 0, sizeof(bids));
        memset(asks, 0, sizeof(asks));
    }
    void limitOrder(uint64_t id, bool isBid, int price, uint64_t qty) {
        assert(price >= 0 && price < MAX_PRICE);
        assert(qty > 0);
        assert(bid < MAX_PRICE);
        assert(ask >= 0 && ask <= MAX_PRICE);

        if (isBid) {
            while (qty > 0 && ask <= price) {
                while(ask < MAX_PRICE && !asks[ask]){
                    ask++;
                }
                if(ask >= MAX_PRICE) break;

                PriceLevel* bestPrice = asks[ask];
                assert(bestPrice);

                while (qty > 0 && bestPrice->head) {
                    Order* resting = bestPrice->head;
                    assert(resting->quantity > 0);
                    uint64_t tradedQty = min(qty, resting->quantity);
                    bestPrice->totalVolume -= tradedQty;
                    resting->quantity -= tradedQty;
                    qty -= tradedQty;
                    if (resting->quantity == 0) {
                        Order* temp = bestPrice->remove(resting->id, Orders);
                        if(temp) pool.release(temp);
                    }
                }
                if (bestPrice->head == NULL) {
                    asks[ask] = nullptr;
                    delete bestPrice;
                    ask++;
                }
            }
            if (qty > 0) {
                if (!bids[price]) {
                    bids[price] = new PriceLevel();
                    bids[price]->price = price;
                }
                if(price > bid) bid = price;
                Order* newOrder = pool.acquire();
                newOrder->id = id; newOrder->isBid = isBid; newOrder->price = price; 
                newOrder->quantity = qty; newOrder->next = NULL; newOrder->prev = NULL;
                bids[price]->add(newOrder, Orders);
            }
        } else {
            while (qty > 0 && bid >= price) {
                while(bid >=0 && !bids[bid]){
                    bid--;
                }
                if(bid < 0) break;
                
                PriceLevel* bestPrice = bids[bid];
                assert(bestPrice);

                while (qty > 0 && bestPrice->head) {
                    Order* resting = bestPrice->head;
                    assert(resting->quantity > 0);
                    uint64_t tradedQty = min(qty, resting->quantity);
                    bestPrice->totalVolume -= tradedQty;
                    resting->quantity -= tradedQty;
                    qty -= tradedQty;
                    if (resting->quantity == 0) {
                        Order* temp = bestPrice->remove(resting->id, Orders);
                        if(temp) pool.release(temp);
                    }
                }

                if (bestPrice->head == NULL) {
                    bids[bid] = nullptr;
                    delete bestPrice;
                    bid--;
                }
            }
            if (qty > 0) {
                if (!asks[price]) {
                    asks[price] = new PriceLevel();
                    asks[price]->price = price;
                }
                if(price < ask) ask = price;
                Order* newOrder = pool.acquire();
                newOrder->id = id; newOrder->isBid = isBid; newOrder->price = price; 
                newOrder->quantity = qty; newOrder->next = NULL; newOrder->prev = NULL;
                asks[price]->add(newOrder, Orders);
            }
        }
    }

    // void displayBook() {
    //     cout << "\n--- ORDER BOOK ---" << endl;
    //     cout << "ASKS (Sellers):" << endl;
    //     for (map<int, PriceLevel*>::reverse_iterator it = asks.rbegin(); it != asks.rend(); ++it) {
    //         cout << "  Price: " << it->first << " | Volume: " << it->second->totalVolume << endl;
    //     }
    //     cout << "------------------" << endl;
    //     cout << "BIDS (Buyers):" << endl;
    //     for (map<int, PriceLevel*, greater<int> >::iterator it = bids.begin(); it != bids.end(); ++it) {
    //         cout << "  Price: " << it->first << " | Volume: " << it->second->totalVolume << endl;
    //     }
    //     cout << "------------------\n" << endl;
    // }

    void cancelOrder(uint64_t id) {
        unordered_map<uint64_t, Order*>::iterator it = Orders.find(id);
        if (it == Orders.end()) {
            //cout << "Order ID " << id << " not found.\n";
            return;
        }
        Order* orderToCancel = it->second;
        int price = orderToCancel->price;
        bool isBid = orderToCancel->isBid;
        PriceLevel* level = isBid ? bids[price] : asks[price];
        Order*temp = level->remove(id, Orders);
        if(temp) pool.release(temp);
        if (level->head == NULL) {
            if (isBid && price == bid && !bids[price]) {
                while (bid >= 0 && !bids[bid]) --bid;
            }
            if (!isBid && price == ask && !asks[price]) {
                while (ask < MAX_PRICE && !asks[ask]) ++ask;
            }
            if (isBid) bids[price] = nullptr;
            else asks[price] = nullptr;
            delete level;
        }
        //cout << "Order " << id << " canceled successfully.\n";
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


#include <random>
#include <unordered_set>

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
        bool doCancel = prob(rng) < cancelRatio && !liveOrders.empty();

        if (doCancel) {
            int idx = rng() % liveOrders.size();
            uint64_t id = liveOrders[idx];

            events.push_back({EventType::CANCEL, id, false, 0, 0});

            // Remove from liveOrders (swap-pop)
            liveOrders[idx] = liveOrders.back();
            liveOrders.pop_back();
        } else {
            uint64_t id = nextId++;
            bool isBid = (rng() & 1);
            int price = priceDist(rng);
            uint64_t qty = qtyDist(rng);

            events.push_back({EventType::LIMIT, id, isBid, price, qty});
            liveOrders.push_back(id);
        }
    }

    return events;
}

#include <chrono>
#include <iostream>

void runBenchmark(int numEvents) {
    OrderPool pool(numEvents + 1000);
    OrderBook book(pool);

    // Generate workload (NOT TIMED)
    auto workload = generateWorkload(numEvents);

    // Cache warmup
    for (int i = 0; i < 1000; ++i) {
        book.limitOrder(10'000'000 + i, true, 100, 1);
    }

    cout << "Running benchmark with " << numEvents << " events...\n";

    auto start = chrono::steady_clock::now();

    for (const auto& e : workload) {
        if (e.type == EventType::LIMIT) {
            book.limitOrder(e.id, e.isBid, e.price, e.qty);
        } else {
            book.cancelOrder(e.id);
        }
    }

    auto end = chrono::steady_clock::now();

    auto totalNs = chrono::duration_cast<chrono::nanoseconds>(end - start).count();

    double seconds = totalNs / 1e9;
    double throughput = numEvents / seconds;
    double avgLatency = (double)totalNs / numEvents;

    cout << "----- BENCHMARK RESULTS -----\n";
    cout << "Total time: " << seconds << " sec\n";
    cout << "Throughput: " << throughput / 1e6 << " M ops/sec\n";
    cout << "Avg latency: " << avgLatency << " ns/op\n";
}

int main() {
    runBenchmark(1'000'000);
    return 0;
}
