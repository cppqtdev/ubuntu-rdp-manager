#pragma once

#include <QStringList>
#include <optional>

namespace urdm {

// Turns raw resource measurements into capacity warnings. "Unlimited sessions" is a
// configuration statement only; this class answers "how many more desktops can this
// machine plausibly host right now".
class ResourceAdvisor
{
public:
    enum class Level { Ok, Warning, Critical };

    struct Thresholds {
        double memAvailableWarnPercent = 15.0;
        double memAvailableCritPercent = 5.0;
        double loadPerCoreWarn = 1.0;
        double loadPerCoreCrit = 2.0;
        double psiWarn = 10.0;  // % of time some task stalled on memory (avg10)
        double psiCrit = 40.0;
        quint64 diskFreeWarnBytes = 2ull * 1024 * 1024 * 1024;
        // Planning figure per additional desktop when no live sessions exist to measure.
        quint64 plannedSessionKiB = 512ull * 1024;
        double memoryReservePercent = 10.0; // kept free for the OS and xrdp itself
    };

    struct Input {
        quint64 memTotalKiB = 0;
        quint64 memAvailableKiB = 0;
        int cpuCount = 1;
        double load1 = 0;
        std::optional<double> memoryPressureAvg10;
        int sessionCount = 0;
        quint64 measuredSessionMemoryKiB = 0; // sum over current session cgroups (0 = unknown)
        std::optional<quint64> diskFreeBytes;
    };

    struct Result {
        Level level = Level::Ok;
        QStringList messages;
        quint64 perSessionEstimateKiB = 0;
        bool perSessionMeasured = false;
        int estimatedAdditionalSessions = 0;
    };

    static Result evaluate(const Input &in);
    static Result evaluate(const Input &in, const Thresholds &t);
};

} // namespace urdm
