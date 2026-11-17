# Robot Bridge 固件差异移植

本分支移植了 `waynecc-at/robot-bridge` 的 `ARCHITECTURE.md` 所记录的 StackChan 固件行为，同时保留本项目官方 ESP-IDF 固件、手机 App 和 Agent 看板架构。

上游仓库的许可证为 MIT。其公开仓库没有包含文档引用的 `/tmp/StackChan/firmware` ESP-IDF 源码或构建镜像，因此这里按文档说明移植行为，不是对作者固件源码的逐行合并。上游文档：[ARCHITECTURE.md](https://github.com/waynecc-at/robot-bridge/blob/main/ARCHITECTURE.md)。

## 移植内容

- 待机随机动作的默认间隔由 4–8 秒调整为 55–65 秒。Setup 中显式设置的 Low（8–12 秒）和 High（2–4 秒）档保持原值。
- 播报状态启用 `SpeakingModifier` 的轻微头部动作。设备正在移动时该动作会跳过。
- Assets 分区扩至 6 MiB。为保持 16 MiB flash 范围内的 core-dump 支持，将 64 KiB `coredump` 放入原本位于 `0x10000` 的空隙。
- `self.robot.get_head_angles`、`self.robot.set_head_angles` 和 `self.robot.set_led_color` 已存在于本项目固件 MCP 服务，继续沿用现有名称、参数及执行实现。

手机 App 的 BLE GATT UUID、配网与动作 JSON/分片协议、局域网消息协议均未修改。说话和待机动作频率改变会影响用户看到的舵机表现，但不改变 App 的连接和命令格式。

## 分区布局

| 分区 | 起始地址 | 大小 |
|---|---:|---:|
| NVS | `0x9000` | `0x4000` |
| OTA 元数据 | `0xD000` | `0x2000` |
| PHY 初始化 | `0xF000` | `0x1000` |
| Core dump | `0x10000` | `0x10000` |
| OTA 0 | `0x20000` | `0x4F0000` |
| OTA 1 | `0x510000` | `0x4F0000` |
| Assets | `0xA00000` | `0x600000` |

分区回归测试验证 4 KiB 对齐、无重叠、flash 地址上限、两个 OTA 槽和 Assets/core-dump 的预期位置。

## 构建与验证

安装并启用项目指定版本 ESP-IDF 后，在 `firmware/` 下执行：

```sh
idf.py build
```

用官方构建产物更新分区表、固件与 Assets：

```sh
idf.py -p PORT flash
```

不要照抄上游文档中引用 `/tmp/nvs_patched_v3.bin` 的全量 `esptool write-flash` 命令。该文件不在仓库中，且其 NVS 内容针对作者设备。分区表变化需要将新分区表和相匹配的固件/Assets 一并刷入；仅通过 OTA 写入应用镜像不会更新分区表。

宿主可运行的固件测试：

```sh
cmake -S firmware/tests -B firmware/build-host-tests
cmake --build firmware/build-host-tests
ctest --test-dir firmware/build-host-tests --output-on-failure
```

网关回归测试：

```sh
cd gateway
uv run --locked pytest
uv run --locked ruff check .
```

宿主测试不验证 ESP32 实机运动、Assets 读取或手机 BLE 共存；这些需要设备验收。`idf.py` 不可用时也无法确认 ESP-IDF 目标构建结果。
