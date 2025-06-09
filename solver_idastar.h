#ifndef SOLVER_IDASTAR_H
#define SOLVER_IDASTAR_H

#include "solver_util.h"

QVector<Step> idaStarSolve(const Grid& start,
                           const Grid& goal,
                           int N,
                           int timeLimitMs = 60000);   // 默认 30 s 超时

#endif
