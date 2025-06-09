#ifndef SOLVERWORKER_H
#define SOLVERWORKER_H

#include <QObject>
#include "solver_util.h"          // 仅需算法与数据结构

class SolverWorker : public QObject
{
    Q_OBJECT
public:
    SolverWorker(const Grid& start,
                 const Grid& goal,
                 int N,
                 QObject* parent = nullptr);

public slots:
    void solve();

signals:
    void stepReady(const Step& step);
    void finished();

private:
    Grid m_start, m_goal;
    int  m_gridSize;
};

#endif // SOLVERWORKER_H
