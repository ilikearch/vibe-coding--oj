# SPEC: Vibe OJ — 仿 LeetCode 在线判题系统

---

## 1. 项目概述

一个面向 3 -20人小团队的 C++ 在线判题系统（Online Judge），支持题目管理、代码提交、自动判题。

| 维度 | 决策 |
|------|------|
| 使用场景 | 个人/小团队自用刷题 |
| 判题模式 | stdin/stdout 比对 |
| 编程语言 | C++ (g++), MVP 仅此一种 |
| 用户系统 | 注册/登录, admin + user 双角色 |
| 数据库 | MySQL (仅持久化题目 + 用户, 提交记录不持久化) |
| 部署 | 本地 Linux, `./server` 直接运行 |

---

## 2. 架构总览

```
┌────────────────────────────────────────────────┐
│                   Browser                      │
│  (原生 HTML + CSS, 传统多页面, <form> 提交)      │
└────────────┬───────────────────────────────────┘
             │ HTTP (完整 HTML 页面)
             │ GET /problem/1  → 服务端返回完整 HTML
             │ POST /problem/1/submit → 服务端返回结果页 HTML
             ▼
┌────────────────────────────────────────────────┐
│              cpp-httplib Server                 │
│                                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
│  │  Router  │  │  Auth    │  │  Judge       │ │
│  │  路由 +   │  │ (Session │  │  Engine      │ │
│  │  HTML 渲 │  │  Token)  │  │  (判题引擎)    │ │
│  │  染      │  │          │  │              │ │
│  └────┬─────┘  └────┬─────┘  └──────┬───────┘ │
│       │             │               │          │
│       └─────────────┼───────────────┘          │
│                     │ fork()                    │
│              ┌──────▼───────┐                  │
│              │   Sandbox    │                  │
│              │  (子进程隔离)  │                  │
│              │  rlimit +    │                  │
│              │  seccomp     │                  │
│              └──────────────┘                  │
│                     │                          │
│              ┌──────▼───────┐                  │
│              │    MySQL     │                  │
│              │  (持久化)     │                  │
│              └──────────────┘                  │
└────────────────────────────────────────────────┘
```

**交互模式**: 传统多页面 (MPA)
- 浏览器发送 GET/POST 请求，服务端返回完整 HTML 页面
- 无 AJAX/fetch，无客户端路由
- 数据随 HTML 页面由服务端 C++ 代码拼装 (字符串模板替换 `{{var}}`)
- 用户操作通过 `<form>` 提交，服务端处理后重定向或返回新页面

**数据流 (判题路径)**:
1. 用户在 `/problem/:id` 页面的 `<form>` 中填写代码，`POST /problem/:id/submit`
2. 后端在沙箱外调用 `g++` 编译
3. 编译成功 → fork 子进程 → 限制 CPU/内存/IO/网络
4. 子进程读取 stdin (测试用例 input), 写入 stdout
5. 父进程比对 stdout 与 expected, 判定 AC/WA/TLE/MLE/RE
6. 服务端将判题结果拼入 HTML，返回完整结果页

---

## 3. 数据库 Schema (MySQL)

```sql
CREATE DATABASE vibe_oj CHARACTER SET utf8mb4;

-- 题目表
CREATE TABLE problems (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  title      VARCHAR(255) NOT NULL,
  difficulty ENUM('Easy','Medium','Hard') NOT NULL,
  content    TEXT NOT NULL,          -- 题目描述 (Markdown)
  template   TEXT,                   -- 代码模板 (可选)
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 测试用例表 (1:N 关联题目)
CREATE TABLE test_cases (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  problem_id INT NOT NULL,
  input      TEXT NOT NULL,          -- stdin 输入数据
  expected   TEXT NOT NULL,          -- 期望 stdout
  position   INT NOT NULL DEFAULT 0, -- 用例执行序号
  FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);

-- 用户表
CREATE TABLE users (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  username   VARCHAR(64) UNIQUE NOT NULL,
  password   VARCHAR(128) NOT NULL,  -- bcrypt 哈希
  role       ENUM('user','admin') DEFAULT 'user',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

---

## 4. 路由设计 (服务端渲染 HTML)

**Base URL**: `http://localhost:8080`
**响应格式**: `text/html` (服务端拼装完整 HTML 页面)
**认证**: Session Cookie (`session_id=xxx`)，由服务端中间件校验
**admin 路由**: 中间件校验 `role=admin`，非 admin 返回 403 页面

### 4.1 公开页面 (无需登录)

| Method | Path | 说明 |
|--------|------|------|
| GET | `/` | **首页落地页**。展示 OJ 简介、功能特性、统计数据，提供 登录/注册/浏览题目 入口 |
| GET | `/login` | 返回登录页 HTML (含 `<form method="POST" action="/login">`) |
| POST | `/login` | 处理登录表单 (`username`, `password`), 成功→重定向 `/problems`, 失败→返回 login 页+错误提示 |
| GET | `/register` | 返回注册页 HTML |
| POST | `/register` | 处理注册, 成功后重定向 `/login` |
| GET | `/logout` | 清除 session, 重定向 `/` |

### 4.2 用户页面 (需登录)

| Method | Path | 说明 |
|--------|------|------|
| GET | `/problems` | 题目列表页。服务端查 DB → 拼装 HTML (含题号/标题/难度表格) |
| GET | `/problem/:id` | 题目详情页。左侧题目描述(Markdown→HTML) + 示例用例, 右侧 `<textarea>` 编辑区 + `<form method="POST" action="/problem/:id/submit">` |
| POST | `/problem/:id/submit` | 处理代码提交 (`code` 字段)。编译+判题, 将结果 (AC/WA/TLE/...) 拼入结果页 HTML 返回 (同一页面, 下方显示结果) |

### 4.3 管理后台 (需 role=admin)

| Method | Path | 说明 |
|--------|------|------|
| GET | `/admin` | 管理面板主页 (题目列表 + 操作入口) |
| GET | `/admin/problems/new` | 新建题目表单页 |
| POST | `/admin/problems` | 创建题目 (`title`, `difficulty`, `content`, `template`), 重定向回 `/admin` |
| GET | `/admin/problems/:id/edit` | 编辑题目表单页 (预填现有数据) |
| POST | `/admin/problems/:id/edit` | 更新题目, 重定向回 `/admin` |
| POST | `/admin/problems/:id/delete` | 删除题目 (级联删用例), 重定向 `/admin` |
| GET | `/admin/problems/:id/testcases` | 该题目的测试用例管理页 (列表+添加表单) |
| POST | `/admin/problems/:id/testcases` | 添加测试用例 (`input`, `expected`, `position`) |
| POST | `/admin/testcases/:id/delete` | 删除单个用例, 重定向回原题目用例页 |
| GET | `/admin/users` | 用户列表 (只读) |

### 4.4 服务端 HTML 拼装约定

```
服务端 C++ 代码中, 用函数拼装 HTML:

string render_page(const string& title, const string& body) {
    return R"(<!DOCTYPE html>
<html><head><title>)" + title + R"(</title>
<link rel="stylesheet" href="/style.css"></head>
<body>
<nav>...导航栏...</nav>
)" + body + R"(
</body></html>)";
}
```

- 判题结果在 `problem.html` 模板中通过 `{{result}}` 占位符替换为 HTML 片段
- 题目列表通过循环拼接 `<tr>` 行
- 导航栏区分登录/未登录/管理员状态
- 所有样式集中在 `static/style.css`

---

## 5. 判题引擎 (Judge Engine)

### 5.1 资源限制 (全局统一)

| 资源 | 限制 | 实现方式 |
|------|------|----------|
| CPU 时间 | 500ms | `RLIMIT_CPU` |
| 真实时间 | 2000ms | 父进程 `alarm()` / `waitpid` 超时 |
| 内存 | 128MB | `RLIMIT_AS` |
| 子进程数 | 1 | `RLIMIT_NPROC` |
| 输出大小 | 1MB | `RLIMIT_FSIZE` |
| 文件系统 | 无权限 | `chroot` 到空目录 |
| 网络 | 禁止 | `seccomp` 过滤 socket/connect/bind syscall |

### 5.2 判题流程

```
                  ┌─ compile error ──→ CE
                  │
submit(code) ──→ g++ code.cpp -o prog
                  │
                  ├─ compile ok
                  │
         ┌────────▼────────┐
         │  for each test  │
         │  case in order │
         └────────┬────────┘
                  │
         ┌────────▼────────┐
         │ fork()          │
         │ setrlimit()     │
         │ seccomp()       │
         │ chroot()        │
         │ execve("./prog")│
         └────────┬────────┘
                  │
         ┌────────▼────────┐
         │ waitpid(timeout)│
         └────────┬────────┘
                  │
     ┌────────────┼────────────┐
     │            │            │
  timeout      killed      exited
     │         (WIFSIGNALED)   │
     │            │            │
    TLE       SIGXCPU→TLE   compare stdout
                  │          vs expected
               SIGSEGV→RE       │
                  │         ┌───┴───┐
               SIGKILL→MLE  match  diff
                              │      │
                             next   WA
                              │
                           all passed
                              │
                             AC
```

### 5.3 安全设计

- **编译**: 沙箱外进行（仅限制编译超时 30s）
- **执行**: fork 子进程 → 设置 rlimit → seccomp 过滤 → chroot → execve
- **chroot**: 切换到 `/tmp/vibe-oj/judge-XXXXXX/` (空目录, 只有 prog 二进制)
- **seccomp**: 白名单模式, 仅允许 `read/write/exit/exit_group/brk` 等必要 syscall
- **stdin/stdout**: 父进程通过 pipe 写入 stdin、读取 stdout/stderr
- **清理**: 父进程 always 回收子进程 + rm -rf 临时目录

---

## 6. 前端页面

### 6.1 页面路由

| 路径 | 页面 | 认证 | 说明 |
|------|------|------|------|
| `/` | 首页落地页 | 无 | OJ 简介 + 功能亮点 + 统计数据 + 登录/注册/浏览题目入口 |
| `/login` | 登录 | 无 | `<form>` 提交 username+password |
| `/register` | 注册 | 无 | `<form>` 提交, 默认 role=user |
| `/problems` | 题目列表 | 需登录 | 表格展示 id/标题/难度, 点击进入详情 |
| `/problem/:id` | 题目详情 + 提交 | 需登录 | 左侧题目描述, 右侧代码 `<textarea>` + 提交按钮, 下方判题结果 |
| `/admin` | 管理面板 | admin | 题目列表 + 新建题目入口 + 管理用户入口 |
| `/admin/problems/new` | 新建题目 | admin | 表单: title/difficulty/content/template |
| `/admin/problems/:id/edit` | 编辑题目 | admin | 表单预填 |
| `/admin/problems/:id/testcases` | 管理用例 | admin | 用例列表 + 添加用例表单 |
| `/admin/users` | 用户列表 | admin | 表格只读展示 |

### 6.2 技术约束

- **纯原生 HTML + CSS**：不引入任何 JS/CSS 框架或 CDN
- **交互模式**：传统 `<form>` 提交 + 服务端返回新页面 / 重定向
- **无 AJAX**：不使用 `fetch()` / `XMLHttpRequest`
- **代码编辑器**：`<textarea>` (无语法高亮)
- **Markdown 渲染**：服务端 C++ 侧做简易 Markdown→HTML 转换（支持标题/代码块/段落/加粗/列表即可）
- **导航栏**：服务端根据登录状态和 role 动态渲染（登录前显示 首页/登录/注册，登录后显示 题目列表/用户名/登出，admin 额外显示 管理后台）

### 6.3 UI 布局 (题目详情页 — 服务端渲染单页)

```
┌──────────────────────────────────────────────┐
│  [OJ Logo] 首页 题目列表  |  admin (登出)          │  ← 导航栏 (服务端拼)
├──────────────────────┬───────────────────────┤
│  题目描述 (HTML)      │  <form method="POST"  │
│                      │   action="/problem/   │
│  # 两数之和           │   :id/submit">        │
│                      │   <textarea           │
│  给定一个整数数组...   │    name="code">       │
│                      │   </textarea>         │
│  **示例:**            │   <button>提交</button>│
│  输入: 1 2           │  </form>              │
│  输出: 3             │                       │
│                      │  ── 判题结果 ──        │  ← 服务端注入
│                      │  状态: WA             │
│                      │  用时: 42ms           │
│                      │  用例 #3 失败:        │
│                      │   期望: 3             │
│                      │   实际: 4             │
└──────────────────────┴───────────────────────┘
```

### 6.4 服务端 HTML 渲染方式

- 静态部分：直接写在 `.html` 模板文件中，C++ 读取后替换 `{{KEY}}` 占位符
- 动态数据（题目列表/用户列表/判题结果）：C++ 循环拼接 HTML 片段，插入 `{{BODY}}` 占位符
- 公共布局（导航栏、页脚）：抽成 C++ 函数 `render_page(title, body_html)` 统一包裹
- `static/style.css`：独立 CSS 文件，直接通过 `<link>` 引用

---

## 7. 项目文件结构

```
vibe-oj/
├── server.cc                  # 主入口: 路由注册 + 启动服务
├── render.h / render.cc       # HTML 渲染工具: 模板读取/替换, 页面骨架函数
├── md.h / md.cc               # 简易 Markdown→HTML 转换器
├── log.h / log.cc               # 日志系统 (时间戳/级别/stderr 输出)
├── db.h / db.cc               # MySQL 连接 & CRUD 封装
├── auth.h / auth.cc           # Session 管理, 密码 bcrypt, 认证中间件
├── judge.h / judge.cc         # 判题引擎核心 (编译 + 沙箱执行)
├── config.h                   # 数据库连接串, 判题限制常量
├── static/
│   └── style.css              # 全局样式 (唯一静态文件)
├── templates/                 # HTML 模板文件 (含 {{PLACEHOLDER}})
│   ├── landing.html            # 首页落地页 ({{PROBLEM_COUNT}}, {{USER_COUNT}})
│   ├── login.html
│   ├── register.html
│   ├── problem_list.html       # 题目列表 ({{PROBLEM_ROWS}})
│   ├── problem_detail.html     # 题目详情+提交 ({{DESCRIPTION}}, {{RESULT}})
│   ├── admin_panel.html        # 管理面板 ({{PROBLEM_ROWS}})
│   ├── admin_problem_form.html # 新建/编辑题目表单
│   ├── admin_testcases.html    # 用例管理
│   └── admin_users.html        # 用户列表
├── deps/                      # 第三方库
│   ├── cpp-httplib/           # header-only HTTP server
│   ├── bcrypt/                # bcrypt 实现
│   └── json.hpp               # nlohmann/json (辅助 Markdown 或数据转换)
├── Makefile                   # 构建 (all / test / clean / run)
├── tests/                     # 单元测试 (Google Test, 每 Phase 分步运行)
│   ├── test_config.cc
│   ├── test_db.cc
│   ├── test_render.cc
│   ├── test_md.cc
│   ├── test_auth.cc            # Phase 2 加入
│   └── test_judge.cc           # Phase 5 加入
└── SPEC.md
```

**依赖库**:
- `cpp-httplib` (header-only HTTP server)
- `mysqlclient` (libmysqlclient-dev, `-lmysqlclient`)
- `bcrypt` (openwall/crypt_blowfish 或 libsodium)
- `gtest` (libgtest-dev, unit testing framework)
- `<sys/resource.h>`, `<seccomp.h>` (libseccomp-dev)

---

## 8. TODO 清单

> **测试策略**: 每个 Phase 完成后立即运行 `make test`，确保新增功能不破坏已有测试。测试分步执行，不积累到 Phase 8 统一运行。

### Phase 1: 基础设施
- [x] 安装依赖: `libmysqlclient-dev`, `libseccomp-dev`
- [x] 初始化 MySQL 数据库, 执行 3 张建表语句
- [x] 编写 `config.h` (DB 连接串, 判题限制常量)
- [x] 编写 `db.h/db.cc` (MySQL 连接, query 封装)
- [x] 编写 `render.h/render.cc` (读取模板文件, `{{KEY}}` 字符串替换, `render_page()` 骨架函数)
- [x] 编写 `md.h/md.cc` (简易 Markdown→HTML: 标题/代码块/加粗/列表/段落)
- [x] 编写 `Makefile` (编译链接, 含 `make test` 单元测试目标)
- [x] 编写 `tests/test_*.cc` 单元测试 (Google Test), 覆盖 db/render/md 模块
- [x] 运行 `make test` 全部通过

### Phase 2: 认证系统
- [x] 编写 `auth.h/auth.cc`:
  - [x] bcrypt 密码哈希 + 验证 (系统 `<crypt.h>`)
  - [x] Session Token 生成 (随机 hex) + 内存 `unordered_map<string, Session>`
  - [x] Cookie 读写 (`Set-Cookie` / `Cookie` header 解析)
  - [x] `CHECK_AUTH` 宏 (检查 session, 未登录重定向 `/login`)
  - [x] `CHECK_ADMIN` 宏 (检查 role=admin, 否则返回 403 HTML)
- [x] 实现: `GET/POST /login`
- [x] 实现: `GET/POST /register`
- [x] 实现: `GET /logout`
- [x] 编写 `tests/test_auth.cc` 单元测试覆盖 bcrypt/session/cookie
- [x] 运行 `make test` 全部通过 (62/62)

### Phase 3: 题目 CRUD (admin)
- [ ] 实现: `GET /admin` (管理面板, 列出所有题目)
- [ ] 实现: `GET /admin/problems/new` + `POST /admin/problems` (创建题目)
- [ ] 实现: `GET /admin/problems/:id/edit` + `POST /admin/problems/:id/edit` (编辑题目)
- [ ] 实现: `POST /admin/problems/:id/delete` (删除题目 + 级联用例)
- [ ] 实现: `GET /admin/problems/:id/testcases` + `POST` (用例管理)
- [ ] 实现: `POST /admin/testcases/:id/delete` (删除单个用例)
- [ ] 实现: `GET /admin/users` (用户列表)
- [ ] 更新 `tests/test_db.cc`: 补充 admin 操作的集成测试
- [ ] 运行 `make test` 全部通过

### Phase 4: 用户端页面
- [ ] 实现: `GET /` (首页落地页, 展示 OJ 简介 + 题目/用户统计 + CTA 按钮)
- [ ] 实现: `GET /problems` (题目列表, 从 DB 查询 → 拼装 HTML 表格)
- [ ] 实现: `GET /problem/:id` (题目详情页, 内含 `<form>` 提交区 + `<textarea name="code">`)
- [ ] 实现: `POST /problem/:id/submit`:
  - [ ] 接收 `code` 表单字段
  - [ ] 调用判题引擎
  - [ ] 将判题结果拼入同一页面的 `{{RESULT}}` 区, 返回完整 HTML
- [ ] 运行 `make test` 全部通过

### Phase 5: 判题引擎 (核心)
- [ ] 实现 `judge.h/judge.cc`:
  - [ ] `compile(src_path, bin_path)` → g++ 静态编译, 返回编译结果/错误信息
  - [ ] `judge(bin_path, test_case)` → fork + rlimit + seccomp + 比对
  - [ ] 资源限制: RLIMIT_CPU, RLIMIT_AS, RLIMIT_NPROC, RLIMIT_FSIZE
  - [ ] seccomp: 白名单 syscall (read/write/exit/brk/mmap/futex/...)
  - [ ] chroot: 创建临时空目录 `/tmp/vibe-oj/judge-XXXXXX/`, 拷贝 prog, `g++ -static` 编译
  - [ ] stdin/stdout 通过 pipe 传递, waitpid 超时 2000ms
  - [ ] 判断 exit code / signal → 映射到 AC/WA/TLE/MLE/RE
  - [ ] 比对输出: trim trailing whitespace, 精确匹配
- [ ] 编写 `tests/test_judge.cc` 单元测试 (编译/沙箱/超时/信号/输出比对)
- [ ] 运行 `make test` 全部通过

### Phase 6: HTML 模板
- [ ] `templates/landing.html` (首页落地页: OJ 名称/简介/功能亮点/统计数据/CTA 按钮, 含 `{{PROBLEM_COUNT}}`, `{{USER_COUNT}}`)
- [ ] `templates/login.html` (含 `{{ERROR}}` 占位符)
- [ ] `templates/register.html`
- [ ] `templates/problem_list.html` (含 `{{PROBLEM_ROWS}}` 占位符)
- [ ] `templates/problem_detail.html`:
  - [ ] `{{TITLE}}`, `{{DESCRIPTION}}` (Markdown→HTML), `{{TEMPLATE}}` (代码模板预填)
  - [ ] `<form method="POST" action="/problem/{{ID}}/submit">`
  - [ ] `{{RESULT}}` 判题结果区 (AC 绿色 / WA 红色 / CE 黄色 / ...)
- [ ] `templates/admin_panel.html` (题目列表 + 操作链接)
- [ ] `templates/admin_problem_form.html` (新建/编辑复用同一模板)
- [ ] `templates/admin_testcases.html` (用例列表 + 添加表单)
- [ ] `templates/admin_users.html` (用户表格)
- [ ] `static/style.css` (全局样式)
- [ ] 运行 `make test` 全部通过

### Phase 7: 主服务器整合
- [ ] `server.cc`: 注册所有路由 handler, 挂载 `static/` 目录, 启动 8080 端口
- [ ] 并发: cpp-httplib 默认多线程 (无需手动管理)
- [ ] 运行 `make test` 全部通过

### Phase 8: 测试 & 收尾
- [ ] 编写测试题目数据 (至少 2 道: Easy + Medium)
- [ ] 端到端测试: 浏览器访问 → 注册 → 登录 → 查看题目列表 → 提交代码 → 看判题结果
- [ ] 端到端测试: admin 创建/编辑/删除题目 + 管理用例
- [ ] 沙箱安全测试: 死循环 (TLE), malloc 炸弹 (MLE), fork 炸弹 (子进程限制), 网络调用 (seccomp 拦截)
- [ ] 验证: 未登录时访问 `/problem/1` 自动重定向到 `/login`
- [ ] 验证: 非 admin 访问 `/admin` 返回 403
- [ ] README.md (构建 & 运行说明)

---

## 9. 验收标准

### 功能验收
1. 未登录用户可访问首页落地页, 查看 OJ 简介和基本统计
2. 用户可注册、登录、登出; session 过期后需重新登录
3. 普通用户可浏览题目列表、查看题目详情 + 代码模板
4. 普通用户可提交 C++ 代码, 获得 AC/WA/TLE/MLE/RE/CE 判定
5. WA 时展示第几个测试点失败 + 期望 vs 实际输出
6. CE 时展示 g++ 原始错误信息
7. 管理员可创建/编辑/删除题目, 管理测试用例
8. 管理员可查看用户列表 (只读)
9. 提交记录在当前 session 内可查看 (内存存储)
10. 题目按难度 (Easy/Medium/Hard) 分级展示

### 性能 & 安全验收
1. 单题判题耗时 < 2s (含编译 + 10 个测试用例)
2. 死循环进程在 500ms 内被 kill (TLE)
3. malloc >128MB 被 kill (MLE)
4. fork() 调用在沙箱内返回失败 (子进程限制)
5. 网络 syscall 被 seccomp 拦截 (进程被 kill → RE)
6. 沙箱内文件系统操作无权限 (chroot)
7. 密码以 bcrypt 哈希存储, 无明文泄漏
8. 用户 A 无法访问用户 B 的 session

### 前端验收
1. 首页 `/` 对所有用户可见 (无需登录), 展示 OJ 名称/简介/统计/入口
2. 所有页面通过 `<form>` 提交 + 服务端重定向/渲染, 无需 JavaScript
3. 未登录时访问受保护页面自动重定向到 `/login`
4. admin 页面仅 admin 角色可访问 (后端中间件校验, 非 admin 返回 403 HTML)
5. UI 在 1280×720 以上分辨率正常显示
6. 导航栏根据登录状态/角色动态变化 (服务端渲染)

---

## 10. 不做的 (明确排除)

- ❌ 不持久化提交记录到 MySQL (仅内存存储, session 内可见)
- ❌ 不支持 SE / OLE 状态码 (仅 AC/WA/TLE/MLE/RE/CE)
- ❌ 不支持多语言 (仅 C++ g++)
- ❌ 不支持题目标签/分类
- ❌ 不支持排行榜/比赛
- ❌ 不支持独立的示例用例表 (所有用例统一在 test_cases 表, 前端可展示前 N 条)
- ❌ 不支持用户删除/重置密码 (管理员不可删除用户)
- ❌ 不做 Docker 化部署
- ❌ 不做定时任务/通知/邮件

---

## 11. 潜在风险 & 权衡

| 风险 | 缓解措施 |
|------|----------|
| seccomp 规则太严格导致正常代码被误杀 | 白名单需要覆盖基础 C++ 运行时 syscall (mmap, munmap, brk, futex 等) |
| chroot 后动态链接库缺失 | 使用 `g++ -static` 静态编译用户代码 |
| 并发判题内存占用 | 3 用户场景下同步判题足够, 不做任务队列 |
| bcrypt 需要第三方库 | 候选: openwall/crypt_blowfish 或 vendored libsodium |
| mysqlclient 链接兼容性 | Makefile 中 `mysql_config --cflags --libs` 动态获取 |
| Markdown 渲染 | 服务端 C++ 侧用简易正则或状态机转换 (标题/代码块/列表/加粗/段落) → HTML |

---

> **版本**: v1.0 | **日期**: 2026-06-02 | **下次评审**: MVP 完成后
