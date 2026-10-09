# 无头资产同步（Headless Asset Sync）— C0 侦察结论与 C1 实现依据

> 状态：C0 侦察完成（2026-08-03），结论 = **可行**。
> 本文档是 C1 commandlet 实现的技术依据，落在 AiGameBridge 插件内随模板分发。

## 背景

资产同步系统设计目标：AiGame 工作台里"被编辑器引用的 approved 资产版本"自动流入 UE 工程 `Content/`，引用消失则转入缓存态。同步通道复用 UE MCP（ModelContextProtocol），但要求**不开编辑器 UI 也能导入**——否则用户必须常开编辑器，体验不可接受。

调研结论（见对话 2026-08-03）：选**路径二 = 无头常驻 MCP 服务**，放弃路径一（`-run=ImportAssets` 短进程，每次 5–20s 冷启动、并发写 Content 不安全、失败观测靠 stdout）。

## C0 侦察结论（已读 UE 5.8 引擎源码坐实）

引擎安装：`C:/Program Files/Epic Games/UE_5.8`
MCP 插件：`Engine/Plugins/Experimental/ModelContextProtocol`

### 1. MCP 主模块是 Runtime，无头可加载

`ModelContextProtocol.uplugin` 模块类型：

| 模块 | Type | 我们依赖？ |
|---|---|---|
| `ModelContextProtocol` | **Runtime** | ✅ 核心：`IModelContextProtocolModule`/`AddTool`/`StartServer` |
| `ModelContextProtocolEngine` | Runtime | 可选 |
| `ModelContextProtocolEditor` | Editor | ❌ 编辑器 Toolset 自动发现，绕过 |

→ `UnrealEditor-Cmd.exe`（Editor target 无 UI）能加载 Runtime 模块，`AddTool`/`StartServer` 在无头下可用。

### 2. StartServer / AddTool 不碰 UI（已读 cpp 验证）

`ModelContextProtocolModule.cpp`：

- `IMPLEMENT_MODULE(FModelContextProtocolModule, ModelContextProtocol)` 在 Runtime 模块；
- `StartServer(Port, UrlPath)`（行 216）只 `MakeUnique<FModelContextProtocolServer>()` + `Server->StartServer`，**无 GEditor/UI 依赖**；
- `AddTool`（行 109）只做名称查重 + `Tools.Add(Tool)`，纯数据结构操作；
- `StartupModule`（行 237）仅 `check(Tools.IsEmpty())`，无副作用。

→ 官方"Toolset 自动发现是编辑器专属，无头需显式 AddTool"的限制确认，但**显式注册路径完全通畅**。

### 3. 工具接口极简

`IModelContextProtocolTool.h`：实现 4 个虚函数即可——

- `GetName()` → 工具名（如 `import_asset`）
- `GetDescription()` → 描述
- `GetInputJsonSchema()` → JSON Schema（可选，默认接受任意 object）
- `Run(Params)` → 同步返回 `FModelContextProtocolToolResult`（或 `RunAsync` 异步）

结果用 `UE::ModelContextProtocol::MakeErrorResult` / 构造 `structuredContent`。

### 4. 资产导入底层 API 无头可用

AiGameBridge 已依赖 `AssetTools` + `UnrealEd`（见 `AiGameBridge.Build.cs`），模块 `Type=Editor`。无头 commandlet 跑在 Editor target（只是无 UI），`FAssetToolsModule::Get().ImportAssetTasks()` + `UAssetImportTask` 在无头下可用——导入是纯数据操作，不弹进度/选项对话框（`bAutomated` 语义）。

## C1 实现方案（AiGameAssetSync commandlet）

### 进程形态

```
UnrealEditor-Cmd.exe <proj.uproject>
  -run=AiGameAssetSync
  -MCPport=<N>                # 独立端口，避免与编辑器 8000 冲突
  -unattended -nopause -nullrhi -nosourcecontrol
```

commandlet 启动后：加载 Runtime MCP 模块 → `StartServer(<N>)` → `AddTool(import_asset)` → 挂起等待（不退出，常驻服务）。

### 代码落点

在 AiGameBridge 插件内新增（复用其 Editor 模块 + AssetTools/UnrealEd 依赖，无需新建插件）：

```
Source/AiGameBridge/Private/HeadlessSync/
  AiGameAssetSyncCommandlet.h / .cpp   # UCommandlet 子类，注册工具 + StartServer
  Tools/
    ImportAssetTool.h / .cpp           # IModelContextProtocolTool 实现：import_asset
    MoveAssetTool.h / .cpp             # （C4）move_asset：Published <-> Cache
    DeleteAssetTool.h / .cpp           # （C4）delete_asset：缓存清理
    SyncStatusTool.h / .cpp            # ue_status：健康检查
```

### import_asset 工具契约（与现有 MCP 通道对齐）

输入（JSON）：
```json
{
  "sourcePath": "<磁盘绝对路径，沙盒临时文件>",
  "destinationRoot": "/Game/AiGame/Published",
  "assetName": "KnightIcon",
  "overwrite": false
}
```

内部：`UAssetImportTask`（Filename=sourcePath, DestinationPath=destinationRoot, bAutomated=true, bReplaceExisting=overwrite, bSave=true）→ `ImportAssetTasks()` → 取 `ImportedObjectPaths`。

输出（structuredContent，与现有 publisher 校验逻辑兼容）：
```json
{
  "objectPath": "/Game/AiGame/Published/KnightIcon.KnightIcon",
  "packagePath": "/Game/AiGame/Published/KnightIcon",
  "checksum": "<源文件 sha256，由服务端核对>",
  "message": "import completed"
}
```

## 进程互斥与优雅让位（C1.5，服务端 dev-control-server）

| 机制 | 行为 |
|---|---|
| 端点解析 | 编辑器在（扫描 UnrealEditor.exe + uproject）→ 用编辑器 MCP:8000；不在 → 拉起/连无头 MCP:<N> |
| 单实例锁 | 无头进程启动创建 `<proj>/Saved/AiGameSync.lock`，已存在则复用不重复拉起 |
| 优雅让位（B1） | 检测到编辑器启动 → 无头完成当前批 → 拒新任务 → 退出，让位给编辑器 |
| DDC 隔离 | 无头进程建议 `-noddc` 或本地 DDC，避免与用户编辑器 DDC 竞争 |

## 后续工作包

- C1：commandlet + import_asset 工具（本文档范围）
- C2：reconcile 核心（desired/actual diff + 串行队列，服务端 JS）
- C3：触发器接线 + 无头进程生命周期管理
- C4：缓存态 move + 手动清理（move_asset/delete_asset 工具）
- C5：UI 只读化 + 同步状态区

打包只收 `/Game/AiGame/Published`（`DefaultGame.ini` `DirectoriesToAlwaysCook` 白名单），缓存在 `/Game/AiGame/Cache/` 不进发布包——该 UE 工程配置后续由 AGMaker-UE Task 模板处理。
