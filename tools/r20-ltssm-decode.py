#!/usr/bin/env python3
"""Decode R20 Tegra210 read-only LTSSM register snapshots; no hardware access.

TRACE_STATUS is the L4T trace buffer interface, NOT a guaranteed live state.
When the trace read-data-valid bit is clear, report raw fields only.
"""
import re
import sys

MAJOR = ("detect", "polling", "config", "unknown", "l0", "l1",
         "l2", "recovery", "loopback", "hotreset", "disabled", "txchar")
MINOR = {
  0: ("quiet", "active", "retry", "wait", "entry"),
  1: ("active", "config", "idle", "?", "compliance", "cspeed"),
  2: ("link start", "link accept", "lane accept", "lane wait", "idle", "pwrup", "complete"),
  4: ("normal", "l0s entry", "l0s idle", "l0s wait", "l0s fts", "pwrup"),
  5: ("entry", "waitrx", "idle", "wait", "pwrup", "beacon entry", "beacon exit"),
  6: ("entry", "waitrx", "transmitwake", "idle"),
  7: ("rcvrlock", "rcvrcfg", "speed", "idle", "?", "?", "?", "finish pkt"),
  8: ("entry", "active", "idle", "exit", "speed", "pre speed"),
}
REGEX = re.compile(
  r"x1-r20-ltssm\s+(?P<phase>\w+)\s+port=(?P<port>\d+)\s+"
  r"dbg=(?P<dbg>[0-9a-fA-F]{8})\s+trace_ctl=(?P<ctl>[0-9a-fA-F]{8})\s+"
  r"trace_status=(?P<status>[0-9a-fA-F]{8})")

def decode(line):
    m = REGEX.search(line)
    if not m:
        return None
    d, c, s = [int(m.group(k), 16) for k in ("dbg", "ctl", "status")]
    major = (s >> 12) & 15
    tx, rx = (s >> 16) & 7, (s >> 19) & 7
    valid = bool(s & (1 << 11))
    def minor(v):
        labels = MINOR.get(major, ())
        return labels[v] if v < len(labels) else "unknown"
    state = (f"{MAJOR[major] if major < len(MAJOR) else 'unknown'}/"
             f"tx:{minor(tx)}/rx:{minor(rx)}") if valid else "unavailable(trace not read-valid)"
    return (f"port={m.group('port')} phase={m.group('phase')} "
            f"dbg=0x{d:08x} FSM15-17={(d>>15)&7:03b} "
            f"trace_enabled={bool(c&1)} trace_valid={valid} "
            f"trace_ram_full={bool(s&1)} trace_write_pointer={(s>>1)&31} "
            f"trace_state={state}")

if __name__ == "__main__":
    lines = sys.stdin if len(sys.argv) == 1 else open(sys.argv[1], encoding="utf8", errors="replace")
    matches = [v for x in lines if (v := decode(x)) is not None]
    print("\n".join(matches) if matches else "NO_R20_LTSSM_SNAPSHOTS")
