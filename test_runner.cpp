#include "src/NanoDB.h"
#include "src/utils/Logger.h"
#include <cstdio>
#include <cstring>


// runs all queries from queries.txt

int main() {
    printf("NanoDB Test Runner - executing queries from queries.txt\n\n");

    Logger::log("=== Automated Test Runner started ===");

    NanoDB db(1000);
    db.initialize();
    db.loadData();

    FILE* f = fopen("queries.txt", "r");
    if (!f) {
        printf("[ERROR] queries.txt not found!\n");
        return 1;
    }

    char line[2048];
    int  qnum = 0;
    int  adminCount = 0;

    while (fgets(line, sizeof(line), f)) {
        // Strip newline
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

        // Skip comments and empty lines
        if (len == 0 || line[0] == '#') continue;

        qnum++;
        printf("\n[Query %2d] %s\n", qnum, line);

        // Determine priority: lines starting with ADMIN: are priority 0
        if (strncmp(line, "ADMIN:", 6) == 0) {
            db.execute(line + 6, 0);
            adminCount++;
        } else {
            db.execute(line, 1);
        }
    }
    fclose(f);

    Logger::log("Test runner complete: executed %d queries (%d admin)", qnum, adminCount);
    printf("\n\n✓ Test runner complete: %d queries executed (%d admin)\n", qnum, adminCount);
    printf("  See nanodb_execution.log for full execution trace.\n\n");

    return 0;
}
