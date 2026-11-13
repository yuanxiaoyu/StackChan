# StackChan Game Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development for isolated components and integration review. Track completed checks below.

**Goal:** 在官方固件加入 GAME、内存卡 FC 游戏播放、原配 ESP-NOW 遥控器游戏模式与 Setup 蓝牙手柄连接。

**Architecture:** 将上游 Nofrendo 内核作为独立 ESP-IDF component；Game App 管理 ROM 浏览、音视频和暂停生命周期。BLE Gamepad 服务复用单一 NimBLE host，通过纯 C++ HID parser 输出 NES 按键快照，Setup 提供扫描/绑定/忘记；SD 使用 CoreS3 SPI3 共享总线并正确切换 GPIO35 的 MISO/LCD DC 功能。

**Tech Stack:** ESP-IDF 6.1 / Mooncake / LVGL 9.5 / NimBLE / Nofrendo / FATFS SDSPI。

**Spec:** `docs/fc-emulator-feasibility.zh-CN.md`，输入目录优先兼容用户现有 `/room`。

## Global Constraints

- 保留官方固件、Setup、看板和 OTA 架构，不改网关。
- BLE HID 与 CodexPad C10 FFA0/FFA1 专用 BLE 协议；另支持原配遥控器的 ESP-NOW 游戏模式。ESP32-S3 不支持 Bluetooth Classic。
- 不格式化内存卡、不修改原 ROM、不提交 ROM、密钥、会话数据或构建产物。
- 分区槽限制 `0x4f0000`；先验证体积，游戏逻辑约 60Hz、显示目标 25–30 FPS。
- 使用当前已隔离的 `codex/fc-emulator-feasibility` 分支；无需另建工作树。

## Task 1: BLE gamepad service and HID parser

Files: `firmware/main/hal/gamepad/*`, `firmware/main/hal/utils/bleprph/bleprph.*`, `firmware/tests/gamepad_hid_test.cpp`, `firmware/sdkconfig.defaults`, `firmware/sdkconfig`。

- [ ] 编写 HID report map/输入解析测试，覆盖 hat、buttons、axes、report IDs、短报告拒绝和 disconnect 清键；运行见失败。
- [ ] 实现 `gamepad::initialize()`, `scan()`, `devices()`, `connect(id)`, `forget()`, `snapshot()`；`Snapshot` 含连接状态、状态文本、NES 按键位图（A=1 B=2 Select=4 Start=8 Up=16 Down=32 Left=64 Right=128）。
- [ ] 使用同一 NimBLE host 初始化和 sync 回调，发现 HOGP 0x1812、读取 map、报告引用、订阅 input CCCD；保存成功连接设备身份及 bond，处理重连/失败/取消。
- [ ] 运行宿主解析测试与 IDF 编译；审查线程安全和现有 BLE 广播共存。

## Task 2: Nofrendo runtime, ROM storage and display/audio

Files: `firmware/components/nofrendo/*`, `firmware/main/apps/app_game/game_runtime.*`, `game_storage.*`, `rom_info.*`, board shared SPI/display hooks, host ROM tests。

- [ ] 下载固定上游提交、保留许可和来源，不携带 ROM。
- [ ] 先测试 iNES header、trainer/PRG/CHR 长度、不支持 NES2/mapper、路径扩展名与容量边界；实现 ROM 目录扫描 `/room`, `/rom`, `/roms/nes`，无卡/坏卡不格式化。
- [ ] 核对 GPIO35 复用和 SPI3 SD/LCD 串行访问，SD 只在暂停/浏览时访问并在恢复显示前恢复 DC 输出路由。
- [ ] 实现非阻塞 runtime API：start(path), stop(), pause(bool), setButtons(mask), copyFrame(buffer), status()。使用 PSRAM 图像双缓冲、24kHz PCM 输出和帧节拍。
- [ ] 验证线程退出、分配失败、ROM load failure 资源归还；构建计算镜像体积。

## Task 3: Game UI, icon and Setup integration

Files: `firmware/main/apps/app_game/app_game.*`, `gamepad_setup_worker.*`, `firmware/main/apps/apps.h`, `firmware/main/main.cpp`, `firmware/main/apps/app_setup/app_setup.cpp`, assets icon source/build script。

- [ ] 解码现有图标确认造型，制作同尺寸 188×150、RGB565A8 Game 图标并保留可重建源文件。
- [ ] Launcher 注册 GAME（追加安装以保留现有 app indices），添加浏览、错误、启动、运行、暂停/退出 UI；主 UI 不阻塞在 ROM/模拟任务中。
- [ ] Setup 增加 Gamepad 配对页：扫描、设备列表、连接、忘记、返回；所有异步更新回到 LVGL 线程。
- [ ] 无卡/空目录/坏 ROM 可返回；掉线暂停并清键；菜单进入退出和音频独占恢复。

## Task 4: Full verification and device

- [ ] 宿主：`cmake -S firmware/tests -B firmware/build-host-tests && cmake --build firmware/build-host-tests && ctest --test-dir firmware/build-host-tests --output-on-failure`。
- [ ] IDF 6.1：构建 app 和 assets，确认槽大小；独立代码审查并修复阻断问题。
- [ ] 设备验证前备份运行 app、NVS/OTA、assets 到仓库外私有目录，仅写入必要固件和更新图标资产，不修改分区表/NVS/OTA。
- [ ] 串口记录卡识别、`room` ROM 头/mapper、模拟帧率与稳定性；日志不输出令牌或聊天正文。
- [ ] 更新使用、验证、限制与待用户验证的蓝牙手柄型号；如没有手柄连接，不宣称蓝牙实机验收通过。

## Task 5: Original controller and CodexPad C10 follow-up

- [x] 对照 CodexPad 官方库加入 FFA0/FFA1 和八字节状态解码，不把 C10 假定为标准 HID。
- [x] 原配遥控器新增 GAME 模式和 20 ms 全状态报文，保留原有遥控模式，短按中心 Start、长按 Menu。
- [x] 主机解码七字节 GP 报文，序号、掉线和松键验证；旧报文只提供有限兼容。
- [x] 暂停菜单可用摇杆选择 Resume / Games / Start / Select / Exit，A 确认。
- [ ] 遥控器固件实机升级与按键游玩（等待 USB 连接）；C10 实测等待到货。
