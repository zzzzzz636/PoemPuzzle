

#include "solverworker.h"
#include <QThread>

SolverWorker::SolverWorker(const Grid& start,
                           const Grid& goal,
                           int N,
                           QObject* parent)
    : QObject(parent),
    m_start(start),
    m_goal(goal),
    m_gridSize(N)
{ }

void SolverWorker::solve()
{
    const auto steps = bfsSolve(m_start, m_goal, m_gridSize);
    for (const Step& s : steps) {
        emit stepReady(s);
        QThread::msleep(120);
    }
    emit finished();
}
