# StackChan FC 掌机模拟器可行性方案

日期：2026-11-12。此文保留最初可行性讨论；用户于 2026-11-13 授权实施，并补充先用原配遥控器、之后接入 CodexPad C10。当前实现、操作与验证结果以 [游戏使用说明](game-setup.zh-CN.md) 为准。

开发分支：`codex/fc-emulator-feasibility`，基于当前 Agent 看板分支创建。

## 1. 结论与已确认需求

**有条件可行。推荐在官方固件的 Launcher 主菜单新增 `GAME` 入口与配套图标，在 Setup 中新增蓝牙手柄配对页，并实现本机 FC/NES 虚拟机从内存卡读取游戏。** 首先验证目标手柄 BLE HID 兼容性、NimBLE 共存、性能、内存卡和固件体积，再投入完整功能开发。

用户已确认：

- 主菜单新增 `Game` 菜单/应用入口，应用图标遵循现有 Launcher 的图标比例、像素资源和配色风格。
- 游戏虚拟机从 StackChan 内存卡加载游戏文件。
- 游戏手柄通过蓝牙连接，用户可在 `Setup` 中扫描和配对。
- 后续确认同时接入 StickC-Plus + Hat Mini JoyC 的 ESP-NOW 游戏模式，以及 CodexPad C10 的专用 BLE 协议。原配遥控器需要升级固件。
- 本轮先给出方案，不开始模拟器实现。

第一版定位为单人、常见 FC 卡带游戏、离线游玩。目标是正常游戏速度、有声音、稳定可玩；显示争取 25–30 帧/秒。这里的帧率是目标，不是已达到的结果。不将全部 FC 游戏兼容性或 60 帧显示作为首版承诺。

## 2. 当前工程提供了什么

| 项目 | 已核查事实 | 对方案的影响 |
| --- | --- | --- |
| 主机 | 仓库 README：CoreS3、ESP32-S3 双核 240MHz、16MB Flash、8MB PSRAM | 有运行轻量 NES 内核的硬件基础；实际空闲内存仍需测量 |
| 固件 | ESP-IDF 6.1、Mooncake、HAL、LVGL 9.5 | 在现有 App 生命周期内集成；旧 ESP32 模拟器不能直接照搬整套工程 |
| 显示 | 320×240，RGB565，SPI 40MHz；LVGL 当前使用 20 行单缓冲 | 图像传输是明确约束，需要限制刷新区域并测量额外拷贝成本 |
| 音频 | 现有 CoreS3 AudioCodec、输出采样率 24kHz | 可作为 PCM 播放适配基础，需要独占与正确释放音频资源 |
| 蓝牙 | ESP32-S3 支持 BLE 而不支持 Bluetooth Classic；sdkconfig 已启用 NimBLE central/observer/GATT client，HID service 选项关闭 | 可以实现 BLE HID 主机；只支持经典蓝牙的手柄不兼容；仍需实现 HID report discovery/解析并实测 |
| BLE 共存 | 固件已有 NimBLE BLE peripheral，SDK 配置同时启用 central 与 peripheral；当前 bleprph 启动流程初始化 NimBLE | 需验证同一 NimBLE Host 多角色/连接、Setup 扫描和游戏连接与既有手机 App 服务的共存及生命周期；现有 peripheral 不是手柄 Host |
| 存储 | README 标注 microSD；当前主机 HAL 未提供已集成的游戏 SD 挂载路径 | 必须核对实际卡槽、引脚和总线，再完成 ROM 文件读取 |
| 应用分区 | 单个 OTA 槽 `0x4f0000`；最近验证镜像 4,293,696 字节 | 按文档记录尚余 883,648 字节，约 863KiB；新内核能否装下必须构建确认 |

证据位置：`firmware/main/hal/board/stackchan.cc`、`config.h`、`stackchan_display.cc`、`firmware/partitions.csv`、`firmware/sdkconfig`、`firmware/main/hal/hal_ble.cpp`、`firmware/main/hal/utils/bleprph/bleprph.c`、`firmware/main/apps/app_launcher/app_launcher.cpp` 和 `docs/agent-tasks-validation.zh-CN.md`。镜像体积来自已有验证记录，本轮未重新构建。

## 3. 路线比较

| 路线 | 优点 | 主要代价 | 建议 |
| --- | --- | --- | --- |
| 官方固件新增 Game App、Launcher 图标和 BLE Setup | 与现有 Launcher、Setup、看板共存；适配标准 BLE HID 手柄；本机离线运行 | 需实现 BLE GATT HID Host、绑定及重连；OTA 槽余量有限 | **优先选择** |
| 独立游戏固件 | 减少与机器人功能竞争资源，方便集中优化 | 切换回日常 StackChan 需要重新启动/选择固件；分区和升级维护更复杂 | 集成路线确实受资源限制时的备选 |
| 电脑运行模拟器、主机串流显示 | 主机模拟计算负担低 | 依赖电脑和网络，增加编码与输入延迟，也没有消除 LCD 带宽限制 | 与离线掌机目标不符，优先级低 |

不将现有 OTA 备用槽直接当游戏固件区；这会改变原有升级和回滚语义，需要单独设计。

## 4. 推荐架构

```text
标准 BLE HID 手柄
  十字键/摇杆、A/B、Start/Select
             │ NimBLE central / HID over GATT
             ▼
Launcher: GAME 图标 → StackChan Game App
  输入适配 → NES 内核 → 帧缓冲 → 显示适配 → LCD
                 └──→ PCM 缓冲 → 音频适配 → 扬声器
  ROM / SRAM 文件 ← SD 存储适配

Setup: 手柄扫描 → 选择 BLE HID → 配对/绑定 → 记住设备
```

模块边界：

- **Game App / FC 虚拟机**：在现有 Launcher 展示 `GAME` 项，使用现有 LVGL 像素图资产与主题色方式创建图标并并入 `assets_bin`；App 提供游戏列表、虚拟机和暂停菜单，遵循 Mooncake 生命周期。
- **NES 内核组件**：CPU、PPU、APU、Mapper；不直接访问 LVGL、网络或舵机。
- **BLE HID Host 适配**：通过 NimBLE central 发现带 Human Interface Device 服务的 BLE 手柄，读取 report map、订阅输入报告并解析方向和按钮；输入回调只发布线程安全的最新按键快照，不运行模拟器、不操作 UI、不访问 SD。
- **显示适配**：帧所有权和缓冲交接；统一屏幕写入路径，避免 LVGL 与直接 LCD 写入互相覆盖。
- **音频适配**：将内核 PCM 转换为硬件输出格式，维护有限队列，退出时停止播放。
- **存储适配**：识别卡带格式、加载 ROM、保存 SRAM，返回明确的无卡/损坏/不支持提示。

Game 图标建议采用简化的 FC 矩形手柄轮廓：左侧十字键、右侧两枚红色动作键，以便与现有 ESPNOW 遥控入口区分；图标尺寸、透明背景、主体占比、边缘处理和主题背景过渡需对照现有 `icon_controller.bin`、`icon_setup.bin` 等实图统一。本轮确定图标主题和集成方式，尚未生成最终图像资产。Game 首屏显示游戏文件列表、SD 卡状态与手柄连接状态；无卡、空目录或不支持文件时保留明确的返回与刷新操作。

进入游戏时暂停头像动画、自动动作及竞争的音频功能。首版不同时运行 AI 语音会话和游戏。既有看板配置与官方功能继续保留，退出游戏后恢复正常使用。

当前 NimBLE 初始化主要启动 peripheral。需为 BLE HID central 增加明确的扫描、连接、服务发现、通知订阅和释放生命周期；Setup 保存绑定密钥，提供忘记、重配和自动重连。先验证 NimBLE 单 host 多角色以及与手机端 BLE 服务共存；若受资源或实现限制，明确规定手柄游戏期间手机 BLE 控制服务暂停，并在退出后恢复，不应假设两种角色自动兼容。

## 5. 手柄配对与游戏操作

Setup 增加 `Gamepad` 配置项，展示扫描状态、发现的 BLE HID 手柄名称与地址摘要、连接和配对结果、已绑定设备、忘记设备与重新扫描操作。只将符合 HID 服务要求的 BLE 设备作为手柄候选。蓝牙经典设备、非 HID BLE 手柄和厂商私有 BLE 协议不属于首版兼容范围；Setup 应清楚反馈不支持。

Game App 首次运行时，如果没有已绑定手柄，提示在 Setup 中进行配对。常规游戏使用十字键/摇杆、A、B、Start 和 Select。手柄布局通过映射设置适配不同厂商的标准 HID report。默认以 `Start + Select` 长按 0.5 秒打开暂停菜单，避免误占游戏内的单键功能；游戏菜单支持继续、重置、调出 ROM 列表、调整输入映射和退出。连接中断时立即释放所有按键并暂停，显示断连提示；重连后要求用户确认继续。

游戏菜单可由 StackChan 触摸屏和手柄操作，模拟器游戏控制由蓝牙手柄负责。目标兼容型号需要逐台实测并维护清单，不能笼统承诺所有蓝牙手柄通用。

## 6. 蓝牙手柄兼容与生命周期

BLE 配对和手柄输入使用 NimBLE/HOGP，不复用 ESP-NOW 的动作报文。需要处理 CCCD/报告订阅、report map 解析、报告 ID、按键按下/释放、断连及重连；输入状态及时交给模拟器并避免逐报告输出日志。首版不依赖 MCP 网关转发输入、视频或音频。Setup 扫描、手柄连接与现有手机 App 的 BLE 外设服务需要回归测试。

## 7. 性能和资源预算

### 显示

NES 常见输出为 256×240。首版建议原像素居中显示，两侧各留 32 像素，不做全屏缩放。此模式保留全部像素，但不校正 FC 在电视上的像素宽高比；4:3 缩放模式后续再评估。

- 每幅 RGB565 图像：`256 × 240 × 2 = 122,880` 字节。
- 每秒显示 30 幅：约 3.69MB/s；60 幅约 7.37MB/s。
- 40MHz 单线 SPI 理论吞吐只有 5MB/s，原画幅全量传输上限约 40.7 帧/秒，实际还要扣除命令、拷贝和调度。
- 全屏 320×240 在 30 帧时已需约 4.61MB/s，余量很小，因此不作为首选。

**以约 60Hz 推进 NTSC 游戏逻辑和音频，画面争取 25–30 帧/秒。** 跳过部分画面的显示不能让游戏整体慢放，也不能跳过维持游戏正确性所需的 PPU 行为。候选内核是否支持高效且正确的跳帧，需要验证。

先测现有 LVGL 图像路径，仅刷新游戏区域，菜单暂停模拟；若无法达到目标，再评估由显示适配统一管理的 DMA 直写。不能一边让 LVGL 刷屏一边独立直写同一 LCD。

### 内存、音频、体积

- 两个原画幅 RGB565 缓冲合计约 240KiB；另需索引图、模拟器工作内存、ROM 和音频缓冲。
- ROM 与大帧缓冲优先放 PSRAM；热路径和 DMA 缓冲根据实际能力分配内部内存。8MB 是物理容量，不能当作 App 可自由分配容量。
- 首版建议 ROM 文件上限先限制为 1MiB；准确值由启动基线、最大连续可分配块和实际内核测量决定，加载前做边界检查。
- 使用现有 24kHz 输出或明确的重采样方案，音频以时间基准连续推进；监测欠载，不能靠无限堆积音频掩盖变慢。
- 当前记录的 OTA 槽余量不足 1MiB，内核和适配加入后必须测量镜像。若超限，先精简首版内核功能和不必要依赖，再讨论分区，避免直接挤占现有 assets 或升级槽。
- 不预先绑死双核任务分工；先测 Wi-Fi、LVGL、模拟与音频负载，再设置任务优先级和亲和性。

## 8. 内核、存储与首版范围

内核优先评估 **Retro-Go 使用的 Nofrendo 分支**，理由是已有 ESP32 设备应用经验；仅提取所需内核并编写本项目适配，不移植其整个多模拟器系统。历史 `espressif/esp32-nesemu` 可作为架构参考。它们都不构成当前 ESP-IDF 6.1、StackChan 硬件的现成兼容保证。

备选为 MIT 许可的 **binjnes**；其周期精确实现有利于兼容性，但本项目没有 ESP32-S3 性能数据，不能仅凭许可或功能表就选定。

Retro-Go Nofrendo 目录附带 GPLv2 COPYING，历史 Nofrendo 文件也存在 GNU Library GPL v2 声明。最终固定提交前应按具体文件和依赖核对许可，并按所选代码的要求分发对应源码和许可；不能仅保留本项目 MIT 标识。用户自行提供 ROM，仓库不内置商业游戏。

首版文件与兼容范围建议：

- SD 卡 `/roms/nes/` 下未压缩的 `.nes` 文件，先支持经典 iNES；校验头、长度、Mapper 和内存需求。
- 先验收 Mapper 0，再加入 2、3；Mapper 1、4 在内核能力和实机结果允许时逐项纳入。是否支持某个游戏，以具体 ROM 的 Mapper 和测试为准。
- 提供游戏列表、声音/静音、暂停、重置、换游戏和退出；错误也可用手柄关闭或返回。
- 实际 SD 卡硬件与总线先验证；如与 LCD 共享总线，必须协调访问，ROM 加载完成后尽量从内存运行。
- SRAM 电池存档可在基础版本稳定后加入，按 ROM 标识保存并采用临时文件替换，避免相同名称串档；即时存档、双人、FDS、金手指和联网功能后续再评估。

## 9. 建议实施顺序与验收门槛

以下是后续工作建议，本轮没有执行实现或刷机。

| 阶段 | 核心任务 | 继续开发的依据 |
| --- | --- | --- |
| A：验证瓶颈 | 用一款目标 BLE HID 手柄验证扫描、绑定、HID 报告和按键；实测现有 peripheral 共存、主机空闲内存、SD 读卡和 LCD；最小内核在 IDF 6.1 编译并核对镜像体积和许可 | 标准 BLE HID 输入和 BLE 共存可用、ROM 来源可用、资源有余量；性能瓶颈有明确处理路线 |
| B：打通最小游戏 | 单个测试/自制 ROM、Mapper 0、完整方向/A/B/Start/Select、触屏与手柄菜单、暂停退出 | Setup 能配对，手柄可游玩及进出游戏；断连能松键并暂停 |
| C：可玩的首版 | 连续声音、SD 游戏列表、目标 Mapper、换游戏、模式恢复 | 达到正常游戏速度，显示争取 25–30 帧，连续玩 30 分钟无崩溃或持续音频欠载 |
| D：兼容与回归 | 按目标 ROM 建立清单，验证手柄重连、坏文件、无卡、反复进入退出及原功能 | 主机恢复 Setup、手机 BLE 服务和正常 Launcher；已支持与不支持范围可追溯 |

输入到画面变化以低于约 80ms 为首版体验目标，可用高帧率视频测量；报告实测分布，不能只用 BLE 通知周期推断延迟。

后续实现时运行仓库规定的宿主 CTest 和主机 ESP-IDF 6.1 构建；网关如无修改，不为模拟器任务引入网关改造。实际游玩验收优先于大量脱离硬件的模拟测试。必要的宿主测试集中于 HID 报告解析、松键/掉线状态、手柄键位映射和 ROM 边界。

## 10. 决策建议

采用“官方固件内 Game App + Launcher 同风格图标 + Setup BLE HID 配对 + Nofrendo 候选内核 + SD ROM”的路线。下一步先做阶段 A，重点确认标准 BLE HID 手柄与现有 NimBLE 服务能够共存。

当前最关键的未知项是 **目标手柄 HID 报告格式、NimBLE central 与既有 BLE peripheral 共存、游戏时手机 BLE 服务的状态、本机空闲内存、SD 接入、加入内核后的 OTA 镜像体积，以及有声音时的游戏速度和显示帧率**。ESP32-S3 没有 Bluetooth Classic，因此应先选一款明确支持标准 BLE HID 的目标手柄实测，不能假定经典蓝牙手柄可用。

## 参考资料

- [CoreS3 官方规格：ESP32-S3、Wi-Fi 与 BLE](https://docs.m5stack.com/en/core/CoreS3)
- [ESP-IDF NimBLE Host central API](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/bluetooth/nimble/index.html)
- [Retro-Go 项目与支持设备](https://github.com/ducalex/retro-go)
- [Retro-Go Nofrendo 目录许可](https://github.com/ducalex/retro-go/blob/master/retro-core/components/nofrendo/COPYING)
- [历史 ESP32 NES 移植](https://github.com/espressif/esp32-nesemu)
- [binjnes：功能、构建与许可](https://github.com/binji/binjnes)
