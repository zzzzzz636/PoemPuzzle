#ifndef SOLVER_UTIL_H
#define SOLVER_UTIL_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QQueue>
#include <QHash>

/* -------- 公共类型 -------- */
using Grid = QVector<QVector<QString>>;

struct Step {            // 行 / 列交换的一步
    bool isRow;          // true: 行, false: 列
    int  a, b;        // 被交换的两行(列)下标
    Step reversed() const{return *this;}
};

/* -------- 声明 -------- */
QString       flatten  (const Grid& g, int N);
Grid          unflatten(const QString& key, int N);
QVector<Step> bfsSolve (const Grid& start,
                       const Grid& goal,
                       int N);

#endif // SOLVER_UTIL_H
