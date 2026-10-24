# Performance and capacity

## Measured (disposable containers, 2026-10-24)

Host: 14 CPU threads, 15 GiB RAM, Ubuntu 24.04 kernel. Server and all clients ran on the same
physical machine; each client was a separate FreeRDP 3 container rendering into Xvfb at
1280×800×24. Desktops were idle after login. Clients started simultaneously.

| Run | Users | Result | Login time* | Server memory (cgroup) | Per session |
|---|---|---|---|---|---|
| minimal Xfce (`--no-install-recommends`) | 2 | 2/2 | ~1.9 s | 323 MiB | ~150 MiB |
| minimal Xfce | 5 | 5/5 | ~1.8 s | 746 MiB | ~150 MiB |
| minimal Xfce | 10 | 10/10 | 2.6–2.7 s | 1.48 GiB | ~148 MiB |
| full `xfce4` as installed by the wizard, packaged start script, pinned TLS | 10 | 10/10 | 2.6–3.5 s | 2.94 GiB | ~295 MiB |

\* client launch → the user's `xfce4-session` process exists on the server. This is not the time
until a user can type; desktop painting continues for a few seconds.

Per-user RSS sums are higher (430–1000 MiB) because shared libraries are counted in every
process; the cgroup figure is the real marginal cost.

Raw data: `tests/system/results/2026-10-24-{two,five,ten}-users*/`.

## Not measured

Active workloads (browsers, office suites, video), WAN latency, bandwidth, more than 10 users,
long-running stability. A 10-user run on this hardware does **not** establish a maximum; the
observed limit was not reached.

## Planning guidance (rules of thumb, not measured in this project)

* Memory: plan ≥ 300 MiB per idle Xfce desktop plus 0.5–2 GiB per active user depending on
  applications (browsers dominate). The application uses 512 MiB as its default planning figure
  (configurable) and never plans below it, even when measured idle sessions are smaller.
* CPU: rendering is software-only; plan at least one core per 2–4 simultaneously active users for
  office work, more for video.
* Keep ≥ 10 % RAM free (the estimate reserves it).

## In-application monitoring

* 2 s sampling (configurable 2/5/10 s) on a worker thread: CPU, memory, swap, load, memory PSI,
  disk, xrdp process group, per-session cgroup memory/CPU/process count.
* `ResourceAdvisor` warns at configurable thresholds (default: available memory < 15 %, load per
  core > 1.0, PSI some avg10 > 10 %) and estimates additional desktops =
  (available − 10 % reserve) / max(measured per-session, planning size).
* *Session Limits → Use capacity-based limit* converts the estimate into `MaxSessions`.

## Reproducing

```bash
tests/system/run-all.sh build results --with-load
```
