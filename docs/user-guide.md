# User guide

Start **Ubuntu RDP Manager** from the application menu (its window is titled *Remote Desktop Manager*). Changes ask for an administrator password
(polkit); viewing never does. Navigate with the sidebar, `Ctrl+1 … Ctrl+0`, or
`Ctrl+PgUp/PgDown`. The sidebar collapses with the ☰ button and automatically on narrow windows.

## Overview
A large **Remote Desktop is ON / OFF** panel with one switch (turning it on also makes it start
with the computer; turning it off does the opposite). **How to connect** shows the address to type
and short instructions for Windows, Mac/phones and Linux. Below: people connected, the connection
limit and room for more, how busy the computer is, who is connected, and problems from the last
24 hours as plain sentences. *Show technical details* reveals versions, ports and addresses.

## Setup
The guided setup described in `installation.md` (steps 1–4). *Check again* re-detects everything.

## Turn On / Off
Turn on, Turn off, Restart and *Start automatically when this computer starts*. Stop and Restart ask for confirmation and
state how many desktops are running: on xrdp 0.9 those desktops survive but become **orphaned**
(not reconnectable). Details show systemd state, PID, uptime, versions and listening addresses.

## Disconnecting from the remote side
Inside a remote desktop, the Xfce **Applications** menu has **Disconnect** (System), and a
*Disconnect* icon is put on the remote desktop once. It closes the connection but keeps the desktop
open; connecting again continues where you left off. To end the desktop, use **Log Out** instead.
In a FreeRDP full-screen window, `Ctrl+Alt+Enter` leaves full screen so the window can be closed.

## Who's Connected
One row per RDP session: id, user, state, display, start time, client address, memory, CPU.

* **Connected**, **Away – still open** (window closed, programs still running) and **Stuck – can't reconnect** (open before Remote Desktop was restarted). Hover a status for an explanation.
* **Show where they connect from** looks up each person's computer address (asks for your password).
* Row buttons: more information, **Disconnect** (programs stay open; they can reconnect),
  **Close desktop** (closes all their programs; asks first).
* **Close left-open…** closes stuck desktops and desktops unused for longer than the chosen hours.

## Who Can Connect
Choose *Everyone who has an account* (Ubuntu default) or *Only the people I choose*. When restricted, toggle each
account. The last member cannot be removed and an empty group cannot be enabled. The *Last denied
login* column shows recent refusals from the logs.

## Connection Limits
*No limit* or a maximum number of people; what happens when someone reconnects; closing
desktops that were left open; disconnecting inactive people. The capacity card estimates how many more desktops fit in memory
and can turn that into a limit. Warning thresholds are adjustable. Apply, then restart the server
when nobody is logged in.

## Settings
**What the connecting computer can use** lists every option a remote desktop program can switch on
(sound, microphone, disks and USB sticks, copy and paste, smart cards, several monitors, resizing,
RemoteApp, printers, USB devices, camera, faster graphics, NLA, gateway) with *Allowed*, *Off*,
*Part missing* or *Not supported by this server*. *Turn on* marks the needed settings; press
**Save**. *Sound tips* explains choppy or delayed sound.

Searchable settings grouped into Network, Security, Sharing between computers, Picture quality
and Activity log detail, each with a plain explanation (technical name in small print); edited
values are highlighted and validated before **Save**. Also: security certificate (view, create, use your own), what people see when they connect, and
**Undo history** with one-click restore.

## Performance
Live charts for CPU, memory and session count; xrdp resource use; session start/failure counts
from logs; last connection-test timings; per-session memory and CPU. Metrics that the system
cannot provide (GPU, per-session bandwidth) are labelled as unavailable.

## Activity Log
Opens on **Important events only** — one plain sentence per event (signed in, wrong password,
turned away, desktop closed, turned on/off, real problems). Switch to *Problems only* or
*Everything (technical)*. Search, filter by kind, update live, save a copy (passwords never
included). If your account cannot read the log, press *Show log (asks for password)*.

## Health Check
Checks grouped by Software, Remote desktop server, Security, Network, Users and capacity, and
problems in the last 24 hours. Each says what is wrong in plain words, *What to do*, and has a
*Fix it* button where possible. It also warns if GNOME's own Remote Desktop sharing is switched on
(it would compete for the same port).

**Test the connection:** enter the computer and port and *Start test*. Stages: TCP connection → RDP security
negotiation (detects servers that fall back to non-TLS) → TLS handshake → certificate
validation. Authentication, session creation and desktop rendering need a real sign-in: use
*Try it*, sign in, then tick what worked and *Save result*. Results are kept in the history.

**Save report for support…** writes a `.tar.gz` with a JSON report, sanitized configuration and journal.

## About & Help
Common questions answered in plain language, keyboard shortcuts, versions, licences.

## Theme
The sidebar's *Appearance* item cycles Automatic → Light → Dark. Because Qt 6.8 cannot switch colour
scheme at runtime on Linux, the change takes effect after *Restart app to change appearance*.
