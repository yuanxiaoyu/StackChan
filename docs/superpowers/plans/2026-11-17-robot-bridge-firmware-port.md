# Robot Bridge Firmware Port Plan

> **For agentic workers:** Execute inline with the repository's TDD and verification instructions. Each task has an independent check.

**Goal:** Port the firmware changes documented by `waynecc-at/robot-bridge` while retaining this project's official StackChan BLE, LAN, and agent-dashboard behavior.

**Architecture:** Keep the existing ESP-IDF firmware and gateway. Preserve the existing MCP servo/LED tools after auditing them, change idle and speaking motion defaults, and enlarge the Assets partition while relocating the 64 KiB core-dump partition into the existing aligned gap before `ota_0`.

**Tech Stack:** ESP-IDF 6.1, C++ firmware, CMake host tests, Python standard library for partition-layout validation.

**Spec:** `https://github.com/waynecc-at/robot-bridge/blob/main/ARCHITECTURE.md` (documented firmware deltas and 16 MB flash command; the upstream repository does not track the referenced ESP-IDF firmware source).

## Global Constraints

- Retain official firmware BLE GATT and local-network App message protocols.
- Retain both OTA app slots and their existing offsets and sizes.
- Use a 16 MiB flash layout without overlapping partitions or dropping core-dump support.
- Do not write local NVS backups, credentials, or build artifacts into the repository.
- Do not flash a physical device as part of source validation.

---

### Task 1: Partition geometry regression test

**Files:**
- Create: `firmware/tests/check_partition_layout.py`
- Modify: `firmware/tests/CMakeLists.txt`
- Test: CTest target `partition_layout_test`

- [x] Add a standard-library checker that parses `firmware/partitions.csv`, resolves implicit offsets in order, verifies 4 KiB alignment and non-overlap, checks every end offset is at most `0x1000000`, and asserts `assets=(0xA00000,0x600000)` and `coredump=(0x10000,0x10000)`.
- [x] Register the checker in CTest with the discovered Python 3 interpreter.
- [x] Run CTest and confirm it fails against the current 4.5 MiB Assets size and trailing core-dump location.

### Task 2: Port motion behavior and partition table

**Files:**
- Modify: `firmware/main/stackchan/modifiers/idle_motion.h`
- Modify: `firmware/main/hal/board/stackchan_display.cc`
- Modify: `firmware/partitions.csv`
- Audit: `firmware/main/hal/hal_mcp.cpp`

- [x] Change the default idle interval to 55,000–65,000 ms; keep explicit Low/High intervals unchanged.
- [x] Enable the existing speaking head-motion behavior for the speaking modifier.
- [x] Set Assets to 6 MiB at `0xA00000`; place the 64 KiB core-dump partition at `0x10000`, preserving both OTA partitions.
- [x] Confirm MCP head-angle and onboard LED tools remain registered and their tool names and execution paths remain unchanged.
- [x] Run the partition CTest and the complete firmware host-test suite.

### Task 3: Migration and validation notes

**Files:**
- Create: `docs/robot-bridge-port.zh-CN.md`

- [x] Record the upstream source and license, ported behavior, unchanged App interfaces, partition map, build command, and full-flash considerations.
- [x] State that the upstream ESP-IDF source and its binary are not present in `robot-bridge`; the port follows `ARCHITECTURE.md` claims and is not a source diff.
- [x] Run the documented gateway pytest/ruff checks and firmware CMake/CTest checks.
- [x] Review the final diff for credentials, local NVS data, and generated build files before committing.
