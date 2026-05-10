#include "NanoDB.h"
#include "utils/Logger.h"
#include "memory/BufferPool.h"
#include <cstdio>
#include <cstring>
#include <ctime>

// print a divider before each test
static void printTestHeader(const char* title) {
    printf("\n\n--- %s ---\n", title);
    Logger::log("=== %s ===", title);
}

// Test A: check that the parser handles nested AND/OR correctly
static void testCaseA(NanoDB& db) {
    printTestHeader("TEST CASE A: Parser & Expression Evaluator");
    printf("Input: SELECT customer WHERE (c_acctbal > 5000 AND c_mktsegment == \"BUILDING\") OR c_nationkey == 15\n\n");
    Logger::log("Test Case A: complex infix expression with nested parentheses");
    db.execute("SELECT customer WHERE (c_acctbal > 5000 AND c_mktsegment == \"BUILDING\") OR c_nationkey == 15");
}

// Test B: time sequential scan vs avl index scan on same key
static void testCaseB(NanoDB& db) {
    printTestHeader("TEST CASE B: Index Optimizer (Sequential vs AVL Tree)");

    Table* tbl = db.getTable("customer");
    if (!tbl) { printf("[ERROR] customer table not found\n"); return; }

    int testKey = 1;
    if (tbl->numRows() > 0) {
        Row* first = tbl->getRow(0);
        if (first && first->get(0)) testKey = (int)first->get(0)->toDouble();
    }

    printf("Searching for c_custkey = %d across %d records\n\n", testKey, tbl->numRows());

    printf("Pass 1: Sequential Array Scan\n");
    clock_t s1 = clock();
    int found = -1;
    for (int i = 0; i < tbl->numRows(); i++) {
        Row* r = tbl->getRow(i);
        if (r && r->get(0) && (int)r->get(0)->toDouble() == testKey) {
            found = i; break;
        }
    }
    clock_t e1 = clock();
    double seqMs = 1000.0*(e1-s1)/CLOCKS_PER_SEC;
    Logger::log("Sequential scan: %d rows, found at idx=%d, time=%.6f ms", tbl->numRows(), found, seqMs);
    printf("  Sequential: scanned %d rows, found at idx=%d | Time: %.6f ms\n\n", tbl->numRows(), found, seqMs);

    printf("Pass 2: AVL Tree Index Scan\n");
    clock_t s2 = clock();
    int idxFound = tbl->indexScan(testKey);
    clock_t e2 = clock();
    double idxMs = 1000.0*(e2-s2)/CLOCKS_PER_SEC;
    Logger::log("Index scan: AVL height=%d, found at idx=%d, time=%.6f ms", tbl->getIndex().height(), idxFound, idxMs);
    printf("  Index scan: AVL height=%d, found at idx=%d | Time: %.6f ms\n\n",
           tbl->getIndex().height(), idxFound, idxMs);

    printf("  Speedup comparison: Sequential=%.6f ms  vs  Index=%.6f ms\n\n", seqMs, idxMs);
}

// Test C: 3 table join, MST should pick customer -> orders -> lineitem
static void testCaseC(NanoDB& db) {
    printTestHeader("TEST CASE C: Join Optimizer (MST via Kruskal's)");
    printf("Executing: customer JOIN orders JOIN lineitem\n\n");
    Logger::log("Test Case C: 3-table join with MST optimization");
    db.execute("JOIN customer orders lineitem");
}

static void testCaseD(NanoDB& db) {
    printTestHeader("TEST CASE D: Memory Stress Test (LRU Eviction)");
    printf("Buffer pool restricted to 50 pages, scanning 5000 lineitem records\n\n");
    Logger::log("Test Case D: stress test with 50 pages, 5000 records");
    db.runMemoryStressTest("lineitem", 5000);
}

// Test E: dump 50 user queries then sneak in an admin query at priority 0
// admin should jump to front of the min-heap
static void testCaseE(NanoDB& db) {
    printTestHeader("TEST CASE E: Priority Queue Concurrency");
    printf("Flooding queue with 50 user SELECT queries + 1 admin UPDATE\n\n");
    Logger::log("Test Case E: priority queue - admin intercepts user queries");

    for (int i = 0; i < 25; i++) {
        char q[128];
        snprintf(q, sizeof(q), "SELECT customer");
        db.enqueue(q, 1);
    }

    db.enqueue("UPDATE customer SET c_acctbal = 9999.99 WHERE c_custkey == 1", 0);
    Logger::log("Admin UPDATE injected into queue at priority 0");

    for (int i = 0; i < 25; i++) {
        db.enqueue("SELECT orders", 1);
    }

    printf("Queue loaded: 50 user queries + 1 admin UPDATE\n");
    printf("Admin UPDATE should execute FIRST:\n\n");

    db.processQueue();
}

static void testCaseF(NanoDB& db) {
    printTestHeader("TEST CASE F: Deep Expression Tree (Arithmetic + Precedence)");
    const char* q = "SELECT orders WHERE ((o_totalprice * 1.5) > 100000 AND (o_custkey % 2 == 0)) OR (o_orderstatus != \"O\")";
    printf("Input: %s\n\n", q);
    Logger::log("Test Case F: arithmetic operators * %% != with precedence");
    db.execute(q);
}

static void testCaseG_write(NanoDB& db) {
    printTestHeader("TEST CASE G: Durability (Write Phase)");
    printf("Inserting 5 new records into customer table...\n\n");
    Logger::log("Test Case G: inserting 5 rows for persistence test");

    db.execute("INSERT customer 99001 \"Persistence Test 1\" \"123 Test St\" 5 \"555-0001\" 1111.11 \"BUILDING\" \"Test record 1\"");
    db.execute("INSERT customer 99002 \"Persistence Test 2\" \"456 Test Ave\" 10 \"555-0002\" 2222.22 \"AUTOMOBILE\" \"Test record 2\"");
    db.execute("INSERT customer 99003 \"Persistence Test 3\" \"789 Test Blvd\" 15 \"555-0003\" 3333.33 \"MACHINERY\" \"Test record 3\"");
    db.execute("INSERT customer 99004 \"Persistence Test 4\" \"321 Test Rd\" 20 \"555-0004\" 4444.44 \"HOUSEHOLD\" \"Test record 4\"");
    db.execute("INSERT customer 99005 \"Persistence Test 5\" \"654 Test Ln\" 25 \"555-0005\" 5555.55 \"FURNITURE\" \"Test record 5\"");

    db.flush();
    Logger::log("5 persistence test rows inserted and flushed to disk");
    printf("All 5 rows flushed to disk. Simulating shutdown...\n\n");
}

static void testCaseG_verify(NanoDB& db) {
    printf("\n  [After simulated restart] Querying for persistence test rows:\n\n");
    Logger::log("Test Case G: verifying persistence after simulated restart");
    db.execute("SELECT customer WHERE c_custkey >= 99001");
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    printf("NanoDB - Mini Database Engine (CS-4002)\n");
    printf("FAST-NUCES Islamabad | MS-CS Spring 2026\n\n");

    Logger::log("NanoDB starting...");

    // main run: full 1000 page pool
    {
        NanoDB db(1000);
        db.initialize();
        db.loadData();

        Table* cust = db.getTable("customer");
        Table* ord  = db.getTable("orders");
        Table* li   = db.getTable("lineitem");

        if (!cust || cust->numRows() == 0) {
            printf("\n[WARNING] No data loaded. Run scripts/generate_data.py first.\n\n");
            Logger::log("No data found - please run generate_data.py");
        }

        Logger::log("Tables loaded: customer=%d orders=%d lineitem=%d",
                    cust ? cust->numRows() : 0,
                    ord  ? ord->numRows()  : 0,
                    li   ? li->numRows()   : 0);

        testCaseA(db);
        testCaseB(db);
        testCaseC(db);
        testCaseF(db);
        testCaseG_write(db);
        testCaseG_verify(db);
    }

    // test D needs a smaller pool to force evictions
    {
        printf("\n[Reinitializing with 50-page pool for stress test...]\n");
        NanoDB db50(50);
        db50.initialize();
        db50.loadData();
        testCaseD(db50);
    }

    // test E: separate instance so queue is clean
    {
        NanoDB dbPQ(200);
        dbPQ.initialize();
        dbPQ.loadData();
        testCaseE(dbPQ);
    }

    printf("\nAll test cases complete. See nanodb_execution.log for full trace.\n\n");
    Logger::log("All test cases complete");

    return 0;
}
