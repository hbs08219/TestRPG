// Main.js — PuerTS 运行时入口(由 C++ UPuertsHost.JsEnv->Start("Main", Args) 加载)
//
// 这是最小可运行 canary：证明 TS 运行时管线打通(Host 传入、OnReady 广播、控制台可见)。
// 真实 gameplay 逻辑请在 TypeScript/Main.ts 编写，经 tsc 编译覆盖本文件。
//
// 注意：本文件为“开箱即用”手写 JS，无需 tsc 即可被 JsEnv 直接加载。

const puerts = require('puerts');
const host = puerts.argv.getByName('Host');

console.log('[GameRuntime] Main.js 已加载; Host =', host ? 'OK' : 'MISSING');

if (host && host.OnReady) {
    host.OnReady.Add(function () {
        console.log('[GameRuntime] OnReady 已触发 → 此处挂载各 gameplay 模块(订阅 C++ 事件 / 注册服务)');
    });
}
