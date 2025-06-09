#include "solver_util.h"

/* 将网格序列化为形如 "字,字,字,..." 的短字符串 */
QString flatten(const Grid& g, int N)
{
    QStringList list;
    list.reserve(N * N);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            list << g[i][j];
    return list.join(QLatin1Char(','));
}

/* 反序列化 */
Grid unflatten(const QString& key, int N)
{
    auto list = key.split(QLatin1Char(','), Qt::SkipEmptyParts);
    Grid g(N, QVector<QString>(N));
    int k = 0;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            g[i][j] = list[k++];
    return g;
}

/* **简单 BFS**：相邻行 / 列交换 */
QVector<Step> bfsSolve(const Grid& start,
                       const Grid& goal,
                       int N)
{
    QString sKey = flatten(start, N),
        gKey = flatten(goal , N);
    if (sKey == gKey) return {};

    QQueue<QString> q;      q.enqueue(sKey);
    QHash<QString,Step>    preStep;
    QHash<QString,QString> preState; preState[sKey] = QStringLiteral("#");

    while (!q.isEmpty()) {
        QString cur = q.dequeue();
        Grid g = unflatten(cur, N);

        /* 尝试交换相邻行 */
        for (int r = 0; r < N - 1; ++r) {
            Grid nxt = g;  std::swap(nxt[r], nxt[r + 1]);
            QString k = flatten(nxt, N);
            if (!preState.contains(k)) {
                preState[k] = cur;  preStep[k] = { true, r, r + 1 };
                if (k == gKey) goto found;
                q.enqueue(k);
            }
        }

        /* 尝试交换相邻列 */
        for (int c = 0; c < N - 1; ++c) {
            Grid nxt = g;
            for (int i = 0; i < N; ++i)
                std::swap(nxt[i][c], nxt[i][c + 1]);
            QString k = flatten(nxt, N);
            if (!preState.contains(k)) {
                preState[k] = cur;  preStep[k] = { false, c, c + 1 };
                if (k == gKey) goto found;
                q.enqueue(k);
            }
        }
    }

found:
    QVector<Step> path;
    for (QString cur = gKey;
         preState.value(cur) != QStringLiteral("#");
         cur = preState[cur])
        path.prepend(preStep[cur]);

    return path;
}
