#pragma once
#include "../utils/CustomArray.h"
#include "../utils/Logger.h"
#include <cstring>
#include <cstdio>

// edge in the join graph, weight = smaller table cardinality
struct Edge {
    int from, to, weight;
};

// Union-Find for Kruskal's MST
struct UnionFind {
    int parent[16];
    int rank[16];
    int n;

    UnionFind(int n) : n(n) {
        for (int i = 0; i < n; i++) { parent[i] = i; rank[i] = 0; }
    }

    int find(int x) {
        if (parent[x] != x) parent[x] = find(parent[x]); // path compression
        return parent[x];
    }

    bool unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;
        if (rank[ra] < rank[rb]) { int t = ra; ra = rb; rb = t; }
        parent[rb] = ra;
        if (rank[ra] == rank[rb]) rank[ra]++;
        return true;
    }
};

// join graph for MST optimizer
class Graph {
    int  numNodes;
    char tableNames[16][64];
    int  tableRows[16];
    CustomArray<Edge> edges;

    // simple insertion sort on edges by weight
    void sortEdges() {
        for (int i = 1; i < edges.size(); i++) {
            Edge key = edges[i];
            int j = i - 1;
            while (j >= 0 && edges[j].weight > key.weight) {
                edges[j+1] = edges[j];
                j--;
            }
            edges[j+1] = key;
        }
    }

    // Kruskal's MST - returns edges in MST
    CustomArray<Edge> kruskal() {
        sortEdges();
        UnionFind uf(numNodes);
        CustomArray<Edge> mst;
        for (int i = 0; i < edges.size() && mst.size() < numNodes-1; i++) {
            if (uf.unite(edges[i].from, edges[i].to))
                mst.push_back(edges[i]);
        }
        return mst;
    }

public:
    Graph() : numNodes(0) {}

    void addTable(const char* name, int rows) {
        strncpy(tableNames[numNodes], name, 63);
        tableNames[numNodes][63] = '\0';
        tableRows[numNodes] = rows;
        numNodes++;
    }

    void addEdge(int a, int b) {
        Edge e;
        e.from   = a;
        e.to     = b;
        e.weight = tableRows[a] < tableRows[b] ? tableRows[a] : tableRows[b];
        edges.push_back(e);
    }

    // figure out join order using MST and return ordered table names
    void getJoinOrder(CustomArray<const char*>& order) {
        if (numNodes == 0) return;
        if (numNodes == 1) { order.push_back(tableNames[0]); return; }

        // connect all tables into fully connected graph
        for (int i = 0; i < numNodes; i++)
            for (int j = i+1; j < numNodes; j++)
                addEdge(i, j);

        CustomArray<Edge> mst = kruskal();

        // build traversal order from MST (start from smallest table)
        int start = 0;
        for (int i = 1; i < numNodes; i++)
            if (tableRows[i] < tableRows[start]) start = i;

        bool visited[16] = {};
        visited[start] = true;
        order.push_back(tableNames[start]);

        // BFS over MST edges
        for (int pass = 0; pass < numNodes-1; pass++) {
            for (int i = 0; i < mst.size(); i++) {
                int a = mst[i].from, b = mst[i].to;
                if (visited[a] && !visited[b]) {
                    visited[b] = true;
                    order.push_back(tableNames[b]);
                } else if (visited[b] && !visited[a]) {
                    visited[a] = true;
                    order.push_back(tableNames[a]);
                }
            }
        }

        // log the MST path
        char path[256] = "";
        for (int i = 0; i < order.size(); i++) {
            strncat(path, order[i], sizeof(path)-strlen(path)-1);
            if (i < order.size()-1) strncat(path, " -> ", sizeof(path)-strlen(path)-1);
        }
        Logger::log("Multi-table join routed via MST: %s", path);
    }
};

// JoinOptimizer is just a wrapper that builds the graph and calls getJoinOrder
class JoinOptimizer {
public:
    static void optimize(CustomArray<const char*>& tableNames,
                         CustomArray<int>& tableRowCounts,
                         CustomArray<const char*>& order) {
        Graph g;
        for (int i = 0; i < tableNames.size(); i++)
            g.addTable(tableNames[i], tableRowCounts[i]);
        g.getJoinOrder(order);
    }
};
