// host.ts — TS 侧对 C++ 宿主 / 子系统的统一访问层
//
// 各 gameplay 模块共享:经 UPuertsHost 桥拿到 C++ 子系统(并订阅其 delegate)。
// 抽成独立模块,避免入口 Main 与各业务模块循环依赖。

import * as UE from 'ue'
import { argv } from 'puerts'

/** C++ UPuertsHost 实例(JsEnv 启动时作为 "Host" 参数传入) */
export const host = argv.getByName('Host') as UE.PuertsHost

/** 取 GameInstanceSubsystem。用法:getGameSub<UE.QuestSubsystem>(UE.QuestSubsystem.StaticClass()) */
export function getGameSub<T extends UE.GameInstanceSubsystem>(cls: UE.Class): T | null {
    return host.GetGameSubsystem(cls) as unknown as T
}
