// Main.ts — PuerTS 脚本运行时入口(gameplay 迁移到 TS 的 bootstrap 起点)
//
// C++ UPuertsHost 在引擎初始化后启动本模块并传入 Host(见 host.ts)。
// 等 Host.OnReady(世界+本地玩家就绪)后挂载 gameplay 模块。
// 这是最小 canary：仅证明管线打通。请在此之上扩展你的 gameplay 系统。

import { argv } from 'puerts'
import { host } from './host'

console.log('[TS] Main 入口已加载; host =', host ? 'OK' : 'MISSING')

host.OnReady.Add(() => {
    console.log('[TS] OnReady → gameplay bootstrap 启动点(在此挂载各系统)')
})
