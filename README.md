# Vibe OJ

面向小团队（3-20 人）的轻量级 C++ 在线判题系统。

## 功能介绍

- **C++ 判题** — 提交 g++ 代码，通过 stdin/stdout 比对自动判定
- **沙箱执行** — rlimit + seccomp + chroot 三层隔离，保障服务器安全
- **用户系统** — 注册 / 登录 / 登出，bcrypt 密码哈希存储
- **管理后台** — 创建、编辑、删除题目；管理测试用例；查看用户列表
- **服务端渲染** — C++ 读取 HTML 模板，`{{PLACEHOLDER}}` 变量替换，无前端框架依赖
- **判题状态** — AC（通过）/ WA（错误）/ CE（编译错误）/ TLE（超时）/ RE（运行时错误）

## 技术栈

| 组件 | 技术 |
|------|------|
| 后端 | C++17 + [cpp-httplib](https://github.com/yhirose/cpp-httplib) |
| 前端 | 原生 HTML + CSS + JavaScript (fetch API) |
| 数据库 | MySQL |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) |
| 密码 | bcrypt (`<crypt.h>`) |
| 测试 | Google Test（C++）+ Python requests（API） |

## 项目结构

```
vibe-oj/
├── server.cc                  # 主入口，路由注册
├── config.h                   # 数据库配置，判题参数
├── db.h / db.cc               # MySQL 增删改查
├── render.h / render.cc       # 模板加载与 {{KEY}} 替换
├── md.h / md.cc               # Markdown → HTML
├── auth.h / auth.cc           # 会话管理 / bcrypt / 中间件
├── judge.h / judge.cc         # 判题引擎（编译 + 沙箱）
├── log.h / log.cc             # 日志系统
├── static/
│   ├── style.css              # 全局样式
│   └── app.js                 # 前端交互逻辑
├── templates/                 # HTML 模板（{{PLACEHOLDER}} 变量）
├── tests/                     # 单元测试（83 项）+ API 测试（32 项）
├── Makefile
└── SPEC.md                    # 完整需求规格
```

## 快速部署

详见 [DEPLOY.md](DEPLOY.md)

```bash
make       # 编译
./server   # 启动（端口 8080）
```

## API 文档

- [API.md](API.md) — 接口说明
- [curl测试.md](curl测试.md) — 接口测试报告

## 安全设计

- 密码：bcrypt 哈希，无明文存储
- 执行：fork 子进程 → rlimit 资源限制 → seccomp 系统调用白名单 → chroot 到 `/tmp/vibe-oj/`
- 会话：随机 hex token，内存存储，HttpOnly Cookie
- 权限：admin 中间件校验角色，非管理员返回 403

## 许可证

MIT
