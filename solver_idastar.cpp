#include "solver_idastar.h"
#include"solver_util.h"
#include <QElapsedTimer>
#include <climits>
#include<QSet>
static int tileMismatch(const Grid& cur, const Grid& goal, int N)
{
    int h = 0;
    for (int r=0; r<N; ++r)
        for (int c=0; c<N; ++c)
            if (cur[r][c] != goal[r][c]) ++h;
    return h / 3;      // 粗调：对 9×9 来说这样比原来快 3~5 倍
}

struct Node {
    Grid   g;
    int    gCost;
    Step   prevAction;
};

static bool dfs(Node& node,
                int threshold,
                const Grid& goal,
                int N,
                QSet<QString>& visited,
                QVector<Step>& path,
                QElapsedTimer& clock,
                int timeLimitMs)
{
    int f = node.gCost + tileMismatch(node.g, goal, N);
    if (f > threshold) return false;
    if (clock.elapsed() > timeLimitMs) throw std::runtime_error("timeout");

    if (node.g == goal) return true;
    QString key = flatten(node.g, N);
    visited.insert(key);

    /* 生成后继 —— 只交换相邻行/列 */
    /* —— 1. 行 —— */
    for (int r = 0; r < N - 1; ++r) {
        Grid nxt = node.g;      std::swap(nxt[r], nxt[r + 1]);
        QString k = flatten(nxt, N);
        if (visited.contains(k)) continue;

        Node child{nxt, node.gCost + 1, {true, r, r + 1}};
        if (dfs(child, threshold, goal, N, visited, path, clock, timeLimitMs)) {
            path.prepend(child.prevAction);   return true;
        }
        visited.remove(k);
    }
    /* —— 2. 列 —— */
    for (int c = 0; c < N - 1; ++c) {
        Grid nxt = node.g;
        for (int i = 0; i < N; ++i) std::swap(nxt[i][c], nxt[i][c + 1]);
        QString k = flatten(nxt, N);
        if (visited.contains(k)) continue;

        Node child{nxt, node.gCost + 1, {false, c, c + 1}};
        if (dfs(child, threshold, goal, N, visited, path, clock, timeLimitMs)) {
            path.prepend(child.prevAction);   return true;
        }
        visited.remove(k);
    }
    return false;
}

QVector<Step> idaStarSolve(const Grid& start,
                           const Grid& goal,
                           int N,
                           int timeLimitMs)
{
    if (start == goal) return {};
    QElapsedTimer clk; clk.start();

    int bound = tileMismatch(start, goal, N);
    while (true) {
        QSet<QString> visited;
        QVector<Step> path;
        Node root{start, 0, {}};

        try {
            if (dfs(root, bound, goal, N, visited, path, clk, timeLimitMs))
                return path;
        } catch (...) {   // timeout
            return {};   // 空结果 → UI 提示“超时”
        }
        ++bound;                // 经典 IDA*：逐步加深
    }
}
