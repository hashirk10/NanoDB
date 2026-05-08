# NanoDB — Mini Database Engine

**Course:** CS-4002 Applied Programming | MS-CS Spring 2026 | FAST-NUCES Islamabad

**GitHub Repository:** *(add your repo link here after pushing)*

---

## Architecture Overview

NanoDB is a from-scratch mini relational database engine in C++17. All standard library containers (STL) are **strictly forbidden** — every data structure is custom-built.

| Component | Implementation | Complexity |
|-----------|---------------|------------|
| Buffer Pool | Fixed array + Doubly Linked List (LRU) + HashMap | O(1) eviction |
| System Catalog | Custom HashMap (djb2 hash, linear probing) | O(1) lookup |
| Indexer | AVL Tree (self-balancing) | O(log N) search |
| Query Parser | Shunting Yard (custom Stack) | O(N) tokens |
| Priority Queue | Min-Heap | O(log N) enqueue/dequeue |
| Join Optimizer | Graph + Kruskal's MST | O(E log E) |

---

## How to Build

### Prerequisites
- **Linux / macOS / WSL on Windows**
- `g++` with C++17 support (`g++ --version` should show ≥ 7.0)
- `Python 3` (for data generation only)

### Step 1 — Generate the TPC-H Dataset
```bash
python3 scripts/generate_data.py
```
This creates `data/customer.tbl` (20,000 rows), `data/orders.tbl` (30,000 rows), and `data/lineitem.tbl` (50,000 rows).

### Step 2 — Compile
```bash
make
```
This produces two binaries:
- `nanodb` — full demo with all 7 test cases
- `test_runner` — automated workload runner (reads `queries.txt`)

### Step 3 — Run the Main Demo
```bash
make run
```

### Step 4 — Run the Automated Test Runner
```bash
make run_tests
```
This reads `queries.txt` (50 queries) and executes each one, logging everything to `nanodb_execution.log`.

---

## Test Cases

| Case | What It Tests |
|------|--------------|
| **A** | Complex WHERE: `(c_acctbal > 5000 AND c_mktsegment == "BUILDING") OR c_nationkey == 15` — prints Postfix expression |
| **B** | Sequential scan vs AVL Tree index scan — prints execution times |
| **C** | 3-table JOIN (customer → orders → lineitem) via MST optimizer |
| **D** | Memory stress test: 50-page buffer pool, 5,000 records, prints eviction count |
| **E** | Priority queue: admin UPDATE intercepts 50 user SELECT queries |
| **F** | Arithmetic in WHERE: `(o_totalprice * 1.5) > 100000 AND (o_custkey % 2 == 0)` |
| **G** | Durability: insert 5 rows, flush, query after simulated restart |

---

## Memory Check (Valgrind)
```bash
make valgrind
```

---

## Log Format

The engine writes detailed logs to `nanodb_execution.log`:

```
[HH:MM:SS] [LOG] Page 42 evicted via LRU, written to disk (table=2)
[HH:MM:SS] [LOG] Infix "c_acctbal > 5000" converted to Postfix "c_acctbal 5000 >"
[HH:MM:SS] [LOG] Multi-table join routed via MST: customer -> orders -> lineitem
```

---

## Project Structure

```
NanoDB/
├── src/
│   ├── utils/
│   │   ├── Logger.h          — centralized logging
│   │   ├── CustomArray.h     — dynamic array (no std::vector)
│   │   ├── DoublyLinkedList.h— O(1) LRU list
│   │   ├── CustomStack.h     — expression parser stack
│   │   ├── HashMap.h         — O(1) hash map (djb2 + linear probe)
│   │   └── PriorityQueue.h   — min-heap for query scheduling
│   ├── memory/
│   │   ├── Page.h            — 4096-byte page
│   │   ├── DiskManager.h     — binary file I/O
│   │   └── BufferPool.h      — LRU buffer pool
│   ├── schema/
│   │   ├── Value.h           — polymorphic INT/FLOAT/STRING
│   │   ├── Column.h + Row.h  — schema and record types
│   │   ├── Table.h           — table with AVL index
│   │   └── SystemCatalog.h   — hash map of tables
│   ├── parser/
│   │   ├── Token.h           — token definitions
│   │   ├── Lexer.h           — tokenizer
│   │   ├── ExpressionParser.h— Shunting Yard + evaluator
│   │   └── QueryParser.h     — full query parser
│   ├── index/
│   │   └── AVLTree.h         — self-balancing BST
│   ├── optimizer/
│   │   └── Graph.h           — graph + Kruskal's MST
│   ├── executor/
│   │   └── QueryExecutor.h   — query execution engine
│   ├── NanoDB.h              — main engine (owns all components)
│   └── main.cpp              — entry point + 7 test cases
├── scripts/
│   └── generate_data.py      — TPC-H data generator
├── queries.txt               — 50-query workload file
├── test_runner.cpp           — automated test runner
├── Makefile
├── README.md
└── .gitignore
```
