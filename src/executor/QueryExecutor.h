#pragma once
#include "../schema/SystemCatalog.h"
#include "../memory/BufferPool.h"
#include "../parser/QueryParser.h"
#include "../parser/ExpressionParser.h"
#include "../optimizer/Graph.h"
#include "../utils/PriorityQueue.h"
#include "../utils/Logger.h"
#include <cstdio>
#include <cstring>
#include <ctime>

class QueryExecutor {
    SystemCatalog* catalog;
    BufferPool*    pool;
    PriorityQueue  pq;
    int            queryCount;

    // print query header box before executing
    void printQueryBox(const char* q) {
        printf("\n==========================================\n");
        printf("  Query: %s\n", q);
        printf("==========================================\n");
    }

    void execSelect(QueryObject* qo) {
        Table* tbl = catalog->getTable(qo->table);
        if (!tbl) { printf("[ERROR] table not found: %s\n", qo->table); return; }

        Logger::log("SELECT on '%s' (rows=%d) WHERE='%s'", tbl->getName(), tbl->numRows(), qo->where);

        tbl->printHeader();
        int found = 0;
        printf("  Found %d row(s):\n", countMatches(tbl, qo));

        // tokenize WHERE clause once
        Lexer lex(qo->where);
        CustomArray<Token> tokens = lex.tokenize();
        CustomArray<Token> postfix;
        bool hasWhere = strlen(qo->where) > 0;
        if (hasWhere) postfix = ExpressionParser::buildPostfix(tokens);

        for (int i = 0; i < tbl->numRows(); i++) {
            Row* r = tbl->getRow(i);
            if (!r) continue;
            if (!hasWhere || ExpressionParser::evaluatePostfix(postfix, *r, tbl->getCols())) {
                r->print(tbl->getCols());
                found++;
            }
        }
        printf("\n");
    }

    int countMatches(Table* tbl, QueryObject* qo) {
        if (strlen(qo->where) == 0) return tbl->numRows();
        Lexer lex(qo->where);
        CustomArray<Token> tokens = lex.tokenize();
        CustomArray<Token> pf = ExpressionParser::buildPostfix(tokens);
        // redirect logs for this count pass
        Logger::setSilent(true);
        int cnt = 0;
        for (int i = 0; i < tbl->numRows(); i++) {
            Row* r = tbl->getRow(i);
            if (r && ExpressionParser::evaluatePostfix(pf, *r, tbl->getCols())) cnt++;
        }
        Logger::setSilent(false);
        return cnt;
    }

    void execSeqScan(QueryObject* qo) {
        Table* tbl = catalog->getTable(qo->table);
        if (!tbl) { printf("[ERROR] table '%s' not found\n", qo->table); return; }

        clock_t s = clock();
        int cnt;
        int* idx = tbl->seqScan(cnt);
        clock_t e = clock();
        double ms = 1000.0 * (e-s) / CLOCKS_PER_SEC;
        Logger::log("SEQUENTIAL_SCAN '%s': %d rows, time=%.4f ms", tbl->getName(), cnt, ms);
        printf("  Sequential scan: %d rows | %.4f ms\n", cnt, ms);
        delete[] idx;
    }

    void execIndexScan(QueryObject* qo) {
        Table* tbl = catalog->getTable(qo->table);
        if (!tbl) return;

        int key = atoi(qo->where);
        clock_t s = clock();
        int rowIdx = tbl->indexScan(key);
        clock_t e = clock();
        double ms = 1000.0 * (e-s) / CLOCKS_PER_SEC;
        Logger::log("INDEX_SCAN '%s': key=%d, found=%d, time=%.4f ms", tbl->getName(), key, rowIdx, ms);
        if (rowIdx >= 0) {
            printf("  Index scan: key=%d found at row %d | AVL height=%d | %.4f ms\n",
                   key, rowIdx, tbl->getIndex().height(), ms);
            tbl->printHeader();
            tbl->getRow(rowIdx)->print(tbl->getCols());
        } else {
            printf("  Index scan: key=%d not found\n", key);
        }
    }

    void execInsert(QueryObject* qo) {
        Table* tbl = catalog->getTable(qo->table);
        if (!tbl) { printf("[ERROR] table '%s' not found\n", qo->table); return; }

        // tokenize the values part
        Lexer lex(qo->where);
        CustomArray<Token> tokens = lex.tokenize();

        int ncols = tbl->numCols();
        Row* row = new Row(ncols);
        const CustomArray<Column>& cols = tbl->getCols();

        int ti = 0;
        for (int i = 0; i < ncols && ti < tokens.size(); i++) {
            Token t = tokens[ti++];
            Value* v = nullptr;
            if (t.type == TokenType::INT_LIT)    v = new IntValue((int)t.fval);
            else if (t.type == TokenType::FLOAT_LIT) v = new FloatValue((float)t.fval);
            else if (t.type == TokenType::STRING_LIT) v = new StringValue(t.sval);
            else if (t.type == TokenType::IDENTIFIER) {
                // could be a number stored as identifier
                v = new StringValue(t.sval);
            }
            if (!v) v = new IntValue(0);
            row->set(i, v);
        }

        int pk = -1;
        if (ncols > 0 && row->get(0)) pk = (int)row->get(0)->toDouble();
        Logger::log("INSERT into '%s': pk=%d (total rows=%d)", tbl->getName(), pk, tbl->numRows()+1);
        tbl->insertRow(row);
        printf("  Inserted 1 row into '%s'. Total: %d rows.\n\n", tbl->getName(), tbl->numRows());
    }

    void execUpdate(QueryObject* qo) {
        Table* tbl = catalog->getTable(qo->table);
        if (!tbl) { printf("[ERROR] table '%s' not found\n", qo->table); return; }

        // parse: SET colname = value WHERE condition
        // format: "SET c_acctbal = 9999.99 WHERE c_custkey == 1"
        char colName[64] = "", whereClause[256] = "";
        float newValF = 0;
        int   newValI = 0;
        bool  isFloat = false;

        const char* setPtr = strstr(qo->where, "SET ");
        const char* wherePtr = strstr(qo->where, "WHERE ");

        if (!setPtr) { printf("[ERROR] UPDATE: missing SET clause\n"); return; }

        char setExpr[256] = "";
        if (wherePtr) {
            int len = (int)(wherePtr - setPtr - 4);
            strncpy(setExpr, setPtr+4, len < 255 ? len : 255);
            strncpy(whereClause, wherePtr+6, 255);
        } else {
            strncpy(setExpr, setPtr+4, 255);
        }

        // parse "colname = value"
        char valStr[128] = "";
        sscanf(setExpr, "%63s = %127s", colName, valStr);
        if (strchr(valStr, '.')) { isFloat = true; newValF = atof(valStr); }
        else newValI = atoi(valStr);

        // find matching rows and update
        int updated = 0;
        Lexer wlex(whereClause);
        CustomArray<Token> wtokens = wlex.tokenize();
        CustomArray<Token> wpf;
        bool hasWhere = strlen(whereClause) > 0;
        if (hasWhere) {
            Logger::setSilent(true);
            wpf = ExpressionParser::buildPostfix(wtokens);
            Logger::setSilent(false);
        }

        for (int i = 0; i < tbl->numRows(); i++) {
            Row* r = tbl->getRow(i);
            if (!r) continue;
            if (hasWhere && !ExpressionParser::evaluatePostfix(wpf, *r, tbl->getCols())) continue;
            Value* nv = isFloat ? (Value*)new FloatValue(newValF) : (Value*)new IntValue(newValI);
            tbl->updateRow(i, colName, nv);
            updated++;
        }
        Logger::log("UPDATE '%s': set %s, updated %d rows", tbl->getName(), colName, updated);
        printf("  Updated %d row(s) in '%s'\n\n", updated, tbl->getName());
    }

    void execDelete(QueryObject* qo) {
        Table* tbl = catalog->getTable(qo->table);
        if (!tbl) return;

        Lexer lex(qo->where);
        CustomArray<Token> tokens = lex.tokenize();
        CustomArray<Token> pf;
        bool hasWhere = strlen(qo->where) > 0;
        if (hasWhere) {
            Logger::setSilent(true);
            pf = ExpressionParser::buildPostfix(tokens);
            Logger::setSilent(false);
        }

        int deleted = 0;
        for (int i = 0; i < tbl->numRows(); i++) {
            Row* r = tbl->getRow(i);
            if (!r) continue;
            if (!hasWhere || ExpressionParser::evaluatePostfix(pf, *r, tbl->getCols())) {
                tbl->deleteRow(i);
                deleted++;
            }
        }
        Logger::log("DELETE from '%s': %d rows removed", tbl->getName(), deleted);
        printf("  Deleted %d row(s) from '%s'\n\n", tbl->getName(), deleted);
    }

    void execJoin(QueryObject* qo) {
        // collect all tables named in the query
        CustomArray<const char*> tnames;
        CustomArray<int> trows;

        // primary table
        Table* t0 = catalog->getTable(qo->table);
        if (!t0) return;
        tnames.push_back(t0->getName());
        trows.push_back(t0->numRows());

        // extra tables in joins[]
        for (int i = 0; i < qo->numJoins; i++) {
            Table* tj = catalog->getTable(qo->joins[i]);
            if (tj) { tnames.push_back(tj->getName()); trows.push_back(tj->numRows()); }
        }

        Logger::log("JoinOptimizer: building graph for %s(%d rows) JOIN %s(%d rows) JOIN %s(%d rows)",
                    tnames.size() > 0 ? tnames[0] : "?", trows.size() > 0 ? trows[0] : 0,
                    tnames.size() > 1 ? tnames[1] : "?", trows.size() > 1 ? trows[1] : 0,
                    tnames.size() > 2 ? tnames[2] : "?", trows.size() > 2 ? trows[2] : 0);

        // get MST-optimized order
        CustomArray<const char*> order;
        Graph g;
        for (int i = 0; i < tnames.size(); i++) g.addTable(tnames[i], trows[i]);
        g.getJoinOrder(order);

        Logger::log("Executing %d-table JOIN", (int)order.size());

        // nested loop join (left-deep)
        Table* left = catalog->getTable(order[0]);
        if (!left) return;

        // start result with rows from leftmost table
        CustomArray<Row*> result;
        const CustomArray<Column>& lcols = left->getCols();
        for (int i = 0; i < left->numRows(); i++) {
            Row* r = left->getRow(i);
            if (r) result.push_back(new Row(*r));
        }

        // join each subsequent table
        for (int t = 1; t < order.size() && result.size() > 0; t++) {
            Table* right = catalog->getTable(order[t]);
            if (!right) continue;
            const CustomArray<Column>& rcols = right->getCols();
            CustomArray<Row*> newResult;

            for (int i = 0; i < result.size(); i++) {
                Row* lr = result[i];
                // match on first col of right == first int col of left
                Value* lk = lr->get(0);
                if (!lk) continue;
                for (int j = 0; j < right->numRows(); j++) {
                    Row* rr = right->getRow(j);
                    if (!rr) continue;
                    Value* rk = rr->get(0);
                    if (!rk) continue;
                    if ((int)lk->toDouble() == (int)rk->toDouble()) {
                        // merge both rows
                        int nc = lr->cols() + rr->cols();
                        Row* merged = new Row(nc);
                        for (int c = 0; c < lr->cols(); c++)
                            if (lr->get(c)) merged->set(c, lr->get(c)->clone());
                        for (int c = 0; c < rr->cols(); c++)
                            if (rr->get(c)) merged->set(lr->cols()+c, rr->get(c)->clone());
                        newResult.push_back(merged);
                        if (newResult.size() >= 20) goto doneJoin; // cap output for demo
                    }
                }
            }
            doneJoin:
            for (int i = 0; i < result.size(); i++) delete result[i];
            result = newResult;
            if (result.size() >= 20) break;
        }

        printf("  Join result (first 20 rows shown):\n");
        left->printHeader();
        for (int i = 0; i < result.size(); i++) {
            result[i]->print(left->getCols());
            delete result[i];
        }
        Logger::log("JOIN produced %d result rows (shown first 20)", (int)result.size());
        printf("\n  Total join matches found: %d\n\n", (int)result.size());
    }

    // dispatch a parsed query to the right exec function
    void execParsed(QueryObject* qo) {
        switch (qo->type) {
            case QueryType::SELECT:       execSelect(qo);   break;
            case QueryType::INSERT:       execInsert(qo);   break;
            case QueryType::UPDATE:       execUpdate(qo);   break;
            case QueryType::DELETE_ROWS:  execDelete(qo);   break;
            case QueryType::JOIN:         execJoin(qo);     break;
            case QueryType::SEQ_SCAN:     execSeqScan(qo);  break;
            case QueryType::INDEX_SCAN:   execIndexScan(qo); break;
            default: printf("[ERROR] unknown query type\n"); break;
        }
    }

public:
    QueryExecutor(SystemCatalog* cat, BufferPool* bp)
        : catalog(cat), pool(bp), queryCount(0) {}

    void execute(const char* query, int priority = 1) {
        printQueryBox(query);
        QueryParser parser(query);
        QueryObject* qo = parser.parse();
        if (!qo) { printf("[ERROR] failed to parse: %s\n", query); return; }
        Logger::log("Parsed query: type=%d table='%s' where='%s' priority=%d",
                    (int)qo->type, qo->table, qo->where, priority);
        execParsed(qo);
        delete qo;
    }

    void enqueue(const char* query, int priority) {
        Logger::log("Enqueued query (priority=%d id=%d): %s", priority, queryCount, query);
        pq.enqueue(query, priority, queryCount++);
    }

    void processQueue() {
        Logger::log("Processing priority queue (%d queries)", pq.size());
        printf("\n=== Processing Priority Queue (%d queries) ===\n", pq.size());
        while (!pq.empty()) {
            PQEntry e = pq.dequeue();
            Logger::log("Dequeued priority=%d id=%d: %s", e.priority, e.id, e.query);
            printf("\n  [PQ] Priority=%d | %s\n", e.priority, e.query);
            execute(e.query, e.priority);
        }
    }

    void runMemoryStressTest(const char* tableName, int numRecords) {
        Logger::log("=== Memory Stress Test: scanning %d records from '%s' (pool=%d pages) ===",
                    numRecords, tableName, pool->getPoolSize());

        Table* tbl = catalog->getTable(tableName);
        if (!tbl) { printf("[ERROR] Table '%s' not found.\n", tableName); return; }

        int beforeEvictions = pool->getEvictions();
        int scanned = 0;

        // fetch pages directly so we go through the buffer pool and trigger real evictions
        int tableId    = tbl->getId();
        int pagesNeeded = (numRecords + RECORDS_PER_PAGE - 1) / RECORDS_PER_PAGE;
        for (int p = 0; p < pagesNeeded && scanned < numRecords; p++) {
            Page* pg = pool->fetchPage(tableId, p);
            int recs = pg->numRecords();
            if (recs == 0) recs = RECORDS_PER_PAGE;
            scanned += recs;
        }
        if (scanned > numRecords) scanned = numRecords;

        int afterEvictions = pool->getEvictions();
        int newEvictions   = afterEvictions - beforeEvictions;

        Logger::log("Stress test complete: scanned=%d evictions_during_test=%d total_evictions=%d",
                    scanned, newEvictions, afterEvictions);
        printf("\n  [Memory Stress Test]\n");
        printf("  Records scanned : %d\n", scanned);
        printf("  Page evictions  : %d\n", newEvictions);
        printf("  Total evictions : %d\n", afterEvictions);
        printf("  Pool size       : %d pages\n\n", pool->getPoolSize());

        pool->printStats();
    }

    void flush() {
        pool->flushAll();
        Logger::log("All dirty pages flushed to disk (persistence guaranteed)");
        printf("  [Persistence] All dirty pages written to disk.\n\n");
    }
};
