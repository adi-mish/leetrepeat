#pragma once
#include "domain/Problem.h"
namespace lr {
struct Schedule { SchedulerState state; QDate due; };
class IScheduler {
public:
    virtual ~IScheduler() = default;
    virtual Schedule initial(QDate today) const = 0;
    virtual Schedule pass(const SchedulerState &before, QDate today) const = 0;
    virtual Schedule fail(const SchedulerState &before, QDate today) const = 0;
    virtual QString name() const = 0;
    virtual int version() const = 0;
};
}
