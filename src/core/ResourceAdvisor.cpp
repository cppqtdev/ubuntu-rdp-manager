#include "ResourceAdvisor.h"

#include <algorithm>
#include <cmath>

namespace urdm {

namespace {
void raise(ResourceAdvisor::Result &r, ResourceAdvisor::Level l)
{
    if (int(l) > int(r.level))
        r.level = l;
}
} // namespace

ResourceAdvisor::Result ResourceAdvisor::evaluate(const Input &in)
{
    return evaluate(in, Thresholds{});
}

ResourceAdvisor::Result ResourceAdvisor::evaluate(const Input &in, const Thresholds &t)
{
    Result r;
    if (in.memTotalKiB == 0) {
        r.level = Level::Warning;
        r.messages << QStringLiteral("Memory information unavailable");
        return r;
    }

    const double availPct = 100.0 * double(in.memAvailableKiB) / double(in.memTotalKiB);
    if (availPct < t.memAvailableCritPercent) {
        raise(r, Level::Critical);
        r.messages << QStringLiteral("Only %1% of memory is available – new sessions may fail or be killed").arg(availPct, 0, 'f', 1);
    } else if (availPct < t.memAvailableWarnPercent) {
        raise(r, Level::Warning);
        r.messages << QStringLiteral("Available memory is low (%1%)").arg(availPct, 0, 'f', 1);
    }

    const int cores = std::max(1, in.cpuCount);
    const double loadPerCore = in.load1 / cores;
    if (loadPerCore >= t.loadPerCoreCrit) {
        raise(r, Level::Critical);
        r.messages << QStringLiteral("CPU is heavily overloaded (load %1 on %2 cores)").arg(in.load1, 0, 'f', 2).arg(cores);
    } else if (loadPerCore >= t.loadPerCoreWarn) {
        raise(r, Level::Warning);
        r.messages << QStringLiteral("CPU load is high (load %1 on %2 cores)").arg(in.load1, 0, 'f', 2).arg(cores);
    }

    if (in.memoryPressureAvg10) {
        if (*in.memoryPressureAvg10 >= t.psiCrit) {
            raise(r, Level::Critical);
            r.messages << QStringLiteral("Severe memory pressure (PSI some avg10 = %1%)").arg(*in.memoryPressureAvg10, 0, 'f', 1);
        } else if (*in.memoryPressureAvg10 >= t.psiWarn) {
            raise(r, Level::Warning);
            r.messages << QStringLiteral("Memory pressure detected (PSI some avg10 = %1%)").arg(*in.memoryPressureAvg10, 0, 'f', 1);
        }
    }

    if (in.diskFreeBytes && *in.diskFreeBytes < t.diskFreeWarnBytes) {
        raise(r, Level::Warning);
        r.messages << QStringLiteral("Low free disk space (%1 MiB)").arg(*in.diskFreeBytes / (1024 * 1024));
    }

    if (in.sessionCount > 0 && in.measuredSessionMemoryKiB > 0) {
        r.perSessionEstimateKiB = in.measuredSessionMemoryKiB / quint64(in.sessionCount);
        r.perSessionMeasured = true;
    } else {
        r.perSessionEstimateKiB = t.plannedSessionKiB;
    }
    // Never plan below the planning figure: idle desktops grow once users open applications.
    const quint64 perSession = std::max(r.perSessionEstimateKiB, t.plannedSessionKiB);
    const double reserve = double(in.memTotalKiB) * t.memoryReservePercent / 100.0;
    const double usable = double(in.memAvailableKiB) - reserve;
    r.estimatedAdditionalSessions = usable > 0 ? int(std::floor(usable / double(perSession))) : 0;

    if (r.estimatedAdditionalSessions == 0) {
        raise(r, Level::Warning);
        r.messages << QStringLiteral("Memory headroom for another desktop session is exhausted");
    }
    return r;
}

} // namespace urdm
