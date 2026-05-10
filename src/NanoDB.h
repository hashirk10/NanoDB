#pragma once
#include "schema/Table.h"
#include "schema/SystemCatalog.h"
#include "memory/DiskManager.h"
#include "memory/BufferPool.h"
#include "executor/QueryExecutor.h"
#include "utils/Logger.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>


// NanoDB engine - owns all components and manages lifecycle

class NanoDB {
    DiskManager*   disk;
    BufferPool*    pool;
    SystemCatalog* catalog;
    QueryExecutor* executor;

    Table* customerTable;
    Table* ordersTable;
    Table* lineitemTable;

    int poolSize;

    // schema setup helpers
    void setupCustomerSchema() {
        int tid = catalog->nextTableId();
        disk->openTable("customer");
        customerTable = new Table("customer", tid, pool);
        customerTable->addColumn(Column("c_custkey",    ValueType::INT,    true));
        customerTable->addColumn(Column("c_name",       ValueType::STRING, false));
        customerTable->addColumn(Column("c_address",    ValueType::STRING, false));
        customerTable->addColumn(Column("c_nationkey",  ValueType::INT,    false));
        customerTable->addColumn(Column("c_phone",      ValueType::STRING, false));
        customerTable->addColumn(Column("c_acctbal",    ValueType::FLOAT,  false));
        customerTable->addColumn(Column("c_mktsegment", ValueType::STRING, false));
        customerTable->addColumn(Column("c_comment",    ValueType::STRING, false));
        catalog->registerTable(customerTable);
    }

    void setupOrdersSchema() {
        int tid = catalog->nextTableId();
        disk->openTable("orders");
        ordersTable = new Table("orders", tid, pool);
        ordersTable->addColumn(Column("o_orderkey",    ValueType::INT,    true));
        ordersTable->addColumn(Column("o_custkey",     ValueType::INT,    false));
        ordersTable->addColumn(Column("o_orderstatus", ValueType::STRING, false));
        ordersTable->addColumn(Column("o_totalprice",  ValueType::FLOAT,  false));
        ordersTable->addColumn(Column("o_orderdate",   ValueType::STRING, false));
        ordersTable->addColumn(Column("o_orderpriority",ValueType::STRING,false));
        ordersTable->addColumn(Column("o_clerk",       ValueType::STRING, false));
        ordersTable->addColumn(Column("o_shippriority",ValueType::INT,    false));
        ordersTable->addColumn(Column("o_comment",     ValueType::STRING, false));
        catalog->registerTable(ordersTable);
    }

    void setupLineitemSchema() {
        int tid = catalog->nextTableId();
        disk->openTable("lineitem");
        lineitemTable = new Table("lineitem", tid, pool);
        lineitemTable->addColumn(Column("l_orderkey",     ValueType::INT,   true));
        lineitemTable->addColumn(Column("l_partkey",      ValueType::INT,   false));
        lineitemTable->addColumn(Column("l_suppkey",      ValueType::INT,   false));
        lineitemTable->addColumn(Column("l_linenumber",   ValueType::INT,   false));
        lineitemTable->addColumn(Column("l_quantity",     ValueType::FLOAT, false));
        lineitemTable->addColumn(Column("l_extendedprice",ValueType::FLOAT, false));
        lineitemTable->addColumn(Column("l_discount",     ValueType::FLOAT, false));
        lineitemTable->addColumn(Column("l_tax",          ValueType::FLOAT, false));
        lineitemTable->addColumn(Column("l_returnflag",   ValueType::STRING,false));
        lineitemTable->addColumn(Column("l_linestatus",   ValueType::STRING,false));
        lineitemTable->addColumn(Column("l_shipdate",     ValueType::STRING,false));
        catalog->registerTable(lineitemTable);
    }

    // parse and load .tbl files into tables
    void loadCustomerData(const char* path) {
        FILE* f = fopen(path, "r");
        if (!f) { Logger::log("Customer data file not found: %s (use generate_data.py)", path); return; }
        char line[1024];
        int count = 0;
        while (fgets(line, sizeof(line), f)) {
            // Format: c_custkey|c_name|c_address|c_nationkey|c_phone|c_acctbal|c_mktsegment|c_comment
            int    custkey, nationkey;
            char   name[64], address[64], phone[32], mktseg[32], comment[256];
            float  acctbal;
            if (sscanf(line, "%d|%63[^|]|%63[^|]|%d|%31[^|]|%f|%31[^|]|%255[^\n]",
                       &custkey, name, address, &nationkey, phone, &acctbal, mktseg, comment) >= 7) {
                Row* row = new Row(8);
                row->set(0, new IntValue(custkey));
                row->set(1, new StringValue(name));
                row->set(2, new StringValue(address));
                row->set(3, new IntValue(nationkey));
                row->set(4, new StringValue(phone));
                row->set(5, new FloatValue(acctbal));
                row->set(6, new StringValue(mktseg));
                row->set(7, new StringValue(comment));
                customerTable->insertRow(row);
                count++;
            }
        }
        fclose(f);
        Logger::log("Loaded %d rows into customer", count);
    }

    void loadOrdersData(const char* path) {
        FILE* f = fopen(path, "r");
        if (!f) { Logger::log("Orders data file not found: %s", path); return; }
        char line[1024];
        int count = 0;
        while (fgets(line, sizeof(line), f)) {
            int   orderkey, custkey, shipprio;
            char  status[8], date[32], priority[32], clerk[32], comment[256];
            float totalprice;
            if (sscanf(line, "%d|%d|%7[^|]|%f|%31[^|]|%31[^|]|%31[^|]|%d|%255[^\n]",
                       &orderkey, &custkey, status, &totalprice, date, priority, clerk, &shipprio, comment) >= 8) {
                Row* row = new Row(9);
                row->set(0, new IntValue(orderkey));
                row->set(1, new IntValue(custkey));
                row->set(2, new StringValue(status));
                row->set(3, new FloatValue(totalprice));
                row->set(4, new StringValue(date));
                row->set(5, new StringValue(priority));
                row->set(6, new StringValue(clerk));
                row->set(7, new IntValue(shipprio));
                row->set(8, new StringValue(comment));
                ordersTable->insertRow(row);
                count++;
            }
        }
        fclose(f);
        Logger::log("Loaded %d rows into orders", count);
    }

    void loadLineitemData(const char* path) {
        FILE* f = fopen(path, "r");
        if (!f) { Logger::log("Lineitem data file not found: %s", path); return; }
        char line[1024];
        int count = 0;
        while (fgets(line, sizeof(line), f)) {
            int   orderkey, partkey, suppkey, linenum;
            float qty, extprice, discount, tax;
            char  retflag[4], linestatus[4], shipdate[32];
            if (sscanf(line, "%d|%d|%d|%d|%f|%f|%f|%f|%3[^|]|%3[^|]|%31[^|\n]",
                       &orderkey, &partkey, &suppkey, &linenum,
                       &qty, &extprice, &discount, &tax,
                       retflag, linestatus, shipdate) >= 10) {
                Row* row = new Row(11);
                row->set(0,  new IntValue(orderkey));
                row->set(1,  new IntValue(partkey));
                row->set(2,  new IntValue(suppkey));
                row->set(3,  new IntValue(linenum));
                row->set(4,  new FloatValue(qty));
                row->set(5,  new FloatValue(extprice));
                row->set(6,  new FloatValue(discount));
                row->set(7,  new FloatValue(tax));
                row->set(8,  new StringValue(retflag));
                row->set(9,  new StringValue(linestatus));
                row->set(10, new StringValue(shipdate));
                lineitemTable->insertRow(row);
                count++;
            }
        }
        fclose(f);
        Logger::log("Loaded %d rows into lineitem", count);
    }

public:
    NanoDB(int bufferPoolSize = 1000) : poolSize(bufferPoolSize) {
        // Create data dir
        system("mkdir -p data");

        disk     = new DiskManager("data");
        pool     = new BufferPool(disk, bufferPoolSize);
        catalog  = new SystemCatalog();
        executor = new QueryExecutor(catalog, pool);

        customerTable = ordersTable = lineitemTable = nullptr;

        Logger::log("NanoDB engine started (pool_size=%d)", bufferPoolSize);
    }

    ~NanoDB() {
        pool->flushAll();
        Logger::log("NanoDB shutdown: all pages flushed");
        delete executor;
        delete catalog;
        delete customerTable;
        delete ordersTable;
        delete lineitemTable;
        delete pool;
        delete disk;
        Logger::close();
    }

    void initialize() {
        setupCustomerSchema();
        setupOrdersSchema();
        setupLineitemSchema();
        Logger::log("Schema initialized: customer, orders, lineitem");
        catalog->listTables();
    }

    void loadData() {
        Logger::log("Loading TPC-H data (LRU eviction logs suppressed during bulk load)...");
        Logger::setSilent(true);
        loadCustomerData("data/customer.tbl");
        loadOrdersData("data/orders.tbl");
        loadLineitemData("data/lineitem.tbl");
        Logger::setSilent(false);
        Logger::log("Data load complete: customer=%d orders=%d lineitem=%d",
                    customerTable->numRows(), ordersTable->numRows(), lineitemTable->numRows());
    }

    void execute(const char* query, int priority = 1) {
        executor->execute(query, priority);
    }

    void enqueue(const char* query, int priority = 1) {
        executor->enqueue(query, priority);
    }

    void processQueue() {
        executor->processQueue();
    }

    void runMemoryStressTest(const char* table, int numRecords) {
        executor->runMemoryStressTest(table, numRecords);
    }

    void flush() { executor->flush(); }

    QueryExecutor* getExecutor() { return executor; }
    Table* getTable(const char* name) { return catalog->getTable(name); }
    SystemCatalog* getCatalog() { return catalog; }
    BufferPool* getPool() { return pool; }
};
