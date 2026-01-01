\# Limit-Order-Book

\## 🚀 High-Performance Limit Order Book (C++)

A low-latency, single-threaded limit order book / matching engine written in modern C++, focused on data-structure choice, memory locality, and allocator performance.

Built to explore how real exchange engines optimize for throughput and nanosecond-level latency.

\## ✨ Featuresx

Price-time priority matching (FIFO at each price level)

Supports limit orders and cancellations

Array-indexed price levels for O(1) best bid / ask access

Custom object pool to eliminate allocator overhead

Cache-friendly linked lists per price level

Deterministic synthetic workload generator

Microbenchmarking with throughput & latency metrics

\## 🧠 Design Overview

Order Book Structure

Price Levels

Fixed-size arrays (PriceLevel\* bids\[MAX\_PRICE\], asks\[MAX\_PRICE\])

Avoids std::map pointer chasing

Per-Price FIFO Queue

Doubly linked list of orders

Preserves time priority

Order Index

unordered\_map for O(1) cancels

Best Bid / Ask Tracking

Integer pointers updated incrementally

No tree traversal

Memory Management

Custom OrderPool

Chunk-based allocation

Free-list reuse

No new/delete on hot path

Improves cache locality and reduces allocator stalls

📊 Benchmark Results

Tested on 1,000,000 events (mixed limit + cancel workload)

ImplementationThroughputAvg Latency

Map + No Pool~6.1 M ops/sec~162 ns

Map + Pool~7.6 M ops/sec~130 ns

Array + Pool~9.8 M ops/sec~101 ns

Replacing std::map with array-indexed price levels improved throughput by ~30%.

Adding a custom object pool eliminated allocator overhead for an additional ~25%.

Total improvement: ~60%
