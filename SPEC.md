# SPEC: Vibe OJ — 仿 LeetCode 在线判题系统

---

## 1. 项目概述

一个面向 3 -20人小团队的 C++ 在线判题系统（Online Judge），支持题目管理、代码提交、自动判题。

| 维度 | 决策 |
|------|------|
| 使用场景 | 个人/小团队自用刷题 |
| 判题模式 | stdin/stdout 比对 |
| 编程语言 | C++ (g++), MVP 仅此一种 |
| 前后端交互 | JSON (POST), HTML (GET) |
| 前端技术 | 原生 HTML + CSS + JS fetch, 无框架依赖 |
| 用户系统 | 注册/登录, admin + user 双角色 |
| 数据库 | MySQL (仅持久化题目 + 用户, 提交记录不持久化) |
| 部署 | 本地 Linux, `./server` 直接运行 |

---

## 2. 架构总览

```
┌────────────────────────────────────────────────┐
│                   Browser                      │
│  (原生 HTML + CSS + JS fetch, <form> + AJAX)    │
└────────────┬───────────────────────────────────┘
             │ HTTP (JSON 请求/响应)
             │ GET /problem/1  → 服务端返回完整 HTML 页面
             │ POST /problem/1/submit → JSON body, 服务端返回 JSON
             │ POST /login → JSON body, 服务端返回 JSON
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

**交互模式**: 混合模式 (MPA + AJAX)
- GET 请求 → 服务端返回完整 HTML 页面（服务端渲染）
- POST 请求 → `Content-Type: application/json`, `Accept: application/json`
- 前端通过 `fetch()` 发送 JSON 请求体，接收 JSON 响应
- 登录/注册/提交代码等操作通过 JSON API 实现
- 页面导航和表单展示仍通过 GET 请求获取完整 HTML
- 服务端通过 `nlohmann/json` 解析请求 JSON 和构造响应 JSON

**数据流 (判题路径)**:
1. 用户在前端页面填写代码，通过 `fetch()` 发送 `POST /problem/:id/submit`，请求体为 JSON `{"code": "..."}`
2. 后端解析 JSON 请求体，获取 `code` 字段
3. 沙箱外调用 `g++` 编译
4. 编译成功 → fork 子进程 → 限制 CPU/内存/IO/网络
5. 子进程读取 stdin (测试用例 input), 写入 stdout
6. 父进程比对 stdout 与 expected, 判定 AC/WA/TLE/MLE/RE
7. 服务端返回 JSON 响应 `{"status":"AC","time_ms":42,...}`

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

## 4. 路由设计 (服务端渲染 HTML + JSON API)

**Base URL**: `http://62.234.44.181:8080`
**GET 响应格式**: `text/html` (服务端拼装完整 HTML 页面)
**POST 请求/响应格式**: `application/json`
**认证**: Session Cookie (`session_id=xxx`)，由服务端中间件校验
**admin 路由**: 中间件校验 `role=admin`，非 admin 返回 `{"error":"Forbidden"}` (HTTP 403)

### 4.1 公开页面 (无需登录)

| Method | Path | 说明 | Request Body | Response Body |
|--------|------|------|-------------|---------------|
| GET | `/` | **首页落地页**。展示 OJ 简介、功能特性、统计数据，提供 登录/注册/浏览题目 入口 | — | `text/html` |
| GET | `/login` | 返回登录页 HTML (含登录表单) | — | `text/html` |
| POST | `/login` | 处理登录 | `{"username":"...","password":"..."}` | `{"success":true,"redirect":"/problems"}` 或 `{"success":false,"error":"Invalid credentials"}` |
| GET | `/register` | 返回注册页 HTML | — | `text/html` |
| POST | `/register` | 处理注册 | `{"username":"...","password":"..."}` | `{"success":true,"redirect":"/login"}` 或 `{"success":false,"error":"Username taken"}` |
| GET | `/logout` | 清除 session, 重定向 `/` | — | 302 redirect |

### 4.2 用户页面 (需登录)

| Method | Path | 说明 | Request Body | Response Body |
|--------|------|------|-------------|---------------|
| GET | `/problems` | 题目列表页。服务端查 DB → 拼装 HTML | — | `text/html` |
| GET | `/problem/:id` | 题目详情页。左侧题目描述(Markdown→HTML) + 示例用例, 右侧代码编辑区 + 提交按钮 | — | `text/html` |
| POST | `/problem/:id/submit` | 处理代码提交。编译+判题, 返回 JSON 结果 | `{"code":"#include <iostream>..."}` | `{"status":"AC","time_ms":42,"memory_kb":1024}` 或 `{"status":"WA","failed_case":3,"expected":"...","actual":"..."}` 或 `{"status":"CE","compile_error":"..."}` |

### 4.3 管理后台 (需 role=admin)

| Method | Path | 说明 | Request Body | Response Body |
|--------|------|------|-------------|---------------|
| GET | `/admin` | 管理面板主页 | — | `text/html` |
| GET | `/admin/problems/new` | 新建题目表单页 | — | `text/html` |
| POST | `/admin/problems` | 创建题目 | `{"title":"...","difficulty":"Easy","content":"...","template":"..."}` | `{"success":true,"id":1}` |
| GET | `/admin/problems/:id/edit` | 编辑题目表单页 | — | `text/html` |
| POST | `/admin/problems/:id/edit` | 更新题目 | `{"title":"...","difficulty":"Medium","content":"...","template":"..."}` | `{"success":true}` |
| POST | `/admin/problems/:id/delete` | 删除题目 (级联删用例) | `{}` | `{"success":true}` |
| GET | `/admin/problems/:id/testcases` | 用例管理页 | — | `text/html` |
| POST | `/admin/problems/:id/testcases` | 添加测试用例 | `{"input":"...","expected":"...","position":0}` | `{"success":true,"id":1}` |
| POST | `/admin/testcases/:id/delete` | 删除单个用例 | `{"problem_id":1}` | `{"success":true}` |
| GET | `/admin/users` | 用户列表 (只读) | — | `text/html` |

### 4.4 JSON API 规范

**POST 请求格式**: `Content-Type: application/json`
```json
{"field1": "value1", "field2": "value2"}
```

**POST 响应格式**: `Content-Type: application/json`
```json
{"success": true, "data": {...}}
// 或错误
{"success": false, "error": "描述信息"}
```

**通用响应字段**:
| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | bool | 操作是否成功 |
| `error` | string | 错误描述 (success=false 时) |
| `redirect` | string | 成功后前端跳转路径 |
| `id` | int | 创建资源时返回的 ID |

**判题结果 JSON 结构**:
```json
{
  "status": "AC",
  "time_ms": 42,
  "memory_kb": 1024,
  "failed_case": 0,
  "expected_output": "",
  "actual_output": ""
}
```

**GET 渲染**: 服务端 C++ 代码拼装完整 HTML 页面，字符串模板替换 `{{KEY}}` 占位符，通过 `render_page()` 包裹公共布局。

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
| `/login` | 登录 | 无 | `<input>` + `<button>` JS `fetch()` 提交 JSON |
| `/register` | 注册 | 无 | `<input>` + `<button>` JS `fetch()` 提交 JSON |
| `/problems` | 题目列表 | 需登录 | 表格展示 id/标题/难度, 点击进入详情 |
| `/problem/:id` | 题目详情 + 提交 | 需登录 | 左侧题目描述, 右侧代码 `<textarea>` + 提交按钮, 下方判题结果 |
| `/admin` | 管理面板 | admin | 题目列表 + 新建题目入口 + 管理用户入口 |
| `/admin/problems/new` | 新建题目 | admin | 表单: title/difficulty/content/template |
| `/admin/problems/:id/edit` | 编辑题目 | admin | 表单预填 |
| `/admin/problems/:id/testcases` | 管理用例 | admin | 用例列表 + 添加用例表单 |
| `/admin/users` | 用户列表 | admin | 表格只读展示 |

### 6.2 技术约束

- **纯原生 HTML + CSS + JS**：不引入任何 JS/CSS 框架或 CDN
- **交互模式**：GET 请求返回完整 HTML 页面；POST 请求通过 `fetch()` 发送 JSON，接收 JSON 响应，前端 JS 处理结果
- **代码编辑器**：`<textarea>` (无语法高亮)
- **Markdown 渲染**：服务端 C++ 侧做简易 Markdown→HTML 转换（支持标题/代码块/段落/加粗/列表即可）
- **导航栏**：服务端根据登录状态和 role 动态渲染（登录前显示 首页/登录/注册，登录后显示 题目列表/用户名/登出，admin 额外显示 管理后台）
- **JSON 解析**：服务端使用 `nlohmann/json` 解析请求体和构造响应体

### 6.3 UI 布局 (题目详情页 — 服务端渲染 + JS JSON 交互)

```
┌──────────────────────────────────────────────┐
│  [OJ Logo] 首页 题目列表  |  admin (登出)          │  ← 导航栏 (服务端拼)
├──────────────────────┬───────────────────────┤
│  题目描述 (HTML)      │  <textarea            │
│                      │   id="code-editor">    │
│  # 两数之和           │  </textarea>           │
│                      │  <button               │
│  给定一个整数数组...   │   onclick="submitCode()"│
│                      │  >提交</button>         │
│  **示例:**            │                       │
│  输入: 1 2           │  ── 判题结果 ──         │  ← JS 动态注入
│  输出: 3             │  状态: WA             │
│                      │  用时: 42ms           │
│                      │  用例 #3 失败:        │
│                      │   期望: 3             │
│                      │   实际: 4             │
└──────────────────────┴───────────────────────┘
```

### 6.4 渲染方式

- **GET 页面**: 服务端 C++ 读取 `.html` 模板 → 替换 `{{KEY}}` 占位符 → 返回完整 HTML
- **POST 接口**: 服务端解析 JSON 请求 → 执行业务逻辑 → 返回 JSON 响应，前端 JS 根据响应更新 DOM
- **公共布局**（导航栏、页脚）：C++ 函数 `render_page(title, body_html)` 统一包裹
- `static/style.css`：独立 CSS 文件，通过 `<link>` 引用
- `static/app.js`：前端交互逻辑 (fetch JSON API, DOM 更新)

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
│   ├── style.css              # 全局样式 (唯一 CSS 文件)
│   └── app.js                 # 前端交互逻辑 (fetch API, DOM 操作)
├── templates/                 # HTML 模板文件 (含 {{PLACEHOLDER}})
│   ├── _base.html              # 基础布局骨架 ({{TITLE}}, {{NAV}}, {{BODY}})
│   ├── landing.html            # 首页落地页 ({{PROBLEM_COUNT}}, {{USER_COUNT}})
│   ├── login.html
│   ├── register.html
│   ├── problem_list.html       # 题目列表 ({{PROBLEM_ROWS}})
│   ├── problem_detail.html     # 题目详情+提交 ({{TITLE}}, {{DESCRIPTION}}, {{RESULT}})
│   ├── admin_panel.html        # 管理面板 ({{PROBLEM_ROWS}})
│   ├── admin_problem_form.html # 新建/编辑题目表单
│   ├── admin_testcases.html    # 用例管理
│   └── admin_users.html        # 用户列表
├── deps/                      # 第三方库 (header-only)
│   └── cpp-httplib/           # header-only HTTP server
├── Makefile                   # 构建 (all / test / clean / run)
├── tests/                     # 单元测试 + API 集成测试
│   ├── test_config.cc
│   ├── test_db.cc
│   ├── test_render.cc
│   ├── test_md.cc
│   ├── test_auth.cc            # Phase 2 加入
│   ├── test_judge.cc           # Phase 5 加入
│   └── test_api.py             # API 集成测试 (32 项, Python)
└── SPEC.md
```

**依赖库**:
- `cpp-httplib` (header-only HTTP server, vendored in `deps/`)
- `mysqlclient` (libmysqlclient-dev, `-lmysqlclient`)
- `nlohmann/json` (nlohmann-json3-dev, 系统包, JSON 解析/序列化)
- `crypt` (系统 `<crypt.h>`, bcrypt 密码哈希)
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
- [x] 实现: `GET /admin` (管理面板, 列出所有题目)
- [x] 实现: `GET /admin/problems/new` + `POST /admin/problems` (创建题目)
- [x] 实现: `GET /admin/problems/:id/edit` + `POST /admin/problems/:id/edit` (编辑题目)
- [x] 实现: `POST /admin/problems/:id/delete` (删除题目 + 级联用例)
- [x] 实现: `GET /admin/problems/:id/testcases` + `POST` (用例管理)
- [x] 实现: `POST /admin/testcases/:id/delete` (删除单个用例)
- [x] 实现: `GET /admin/users` (用户列表)
- [x] 更新 `tests/test_db.cc`: 补充 admin 操作的集成测试
- [x] 运行 `make test` 全部通过 (68/68)

### Phase 4: 用户端页面
- [x] 实现: `GET /` (首页落地页, 展示 OJ 简介 + 题目/用户统计 + CTA 按钮)
- [x] 实现: `GET /problems` (题目列表, 从 DB 查询 → 拼装 HTML 表格)
- [x] 实现: `GET /problem/:id` (题目详情页, 内含 `<form>` 提交区 + `<textarea name="code">`)
- [x] 实现: `POST /problem/:id/submit`:
  - [x] 接收 `code` 表单字段
  - [x] 调用判题引擎
  - [x] 将判题结果拼入同一页面的 `{{RESULT}}` 区, 返回完整 HTML
- [x] 运行 `make test` 全部通过 (68/68)

### Phase 5: 判题引擎 (核心)
- [x] 实现 `judge.h/judge.cc`:
  - [x] `compile(src_path, bin_path)` → g++ 静态编译, 返回编译结果/错误信息
  - [x] `judge(bin_path, test_case)` → fork + rlimit + seccomp + 比对
  - [x] 资源限制: RLIMIT_CPU, RLIMIT_AS, RLIMIT_NPROC, RLIMIT_FSIZE
  - [x] seccomp: 白名单 syscall (read/write/exit/brk/mmap/futex/...)
  - [x] chroot: 创建临时空目录 `/tmp/vibe-oj/judge-XXXXXX/`, 拷贝 prog, `g++ -static` 编译
  - [x] stdin/stdout 通过 pipe 传递, waitpid 超时 2000ms
  - [x] 判断 exit code / signal → 映射到 AC/WA/TLE/MLE/RE
  - [x] 比对输出: trim trailing whitespace, 精确匹配
- [x] 编写 `tests/test_judge.cc` 单元测试 (编译/沙箱/超时/信号/输出比对)
- [x] 运行 `make test` 全部通过 (83/83)

### Phase 6: HTML 模板
- [x] `templates/_base.html` (基础布局: DOCTYPE + nav + main + footer, 含 `{{TITLE}}`, `{{NAV}}`, `{{BODY}}`)
- [x] `templates/landing.html` (首页落地页: 含 `{{PROBLEM_COUNT}}`, `{{USER_COUNT}}`, hero/features/stats)
- [x] `templates/login.html` (登录表单, JS fetch 提交 JSON)
- [x] `templates/register.html` (注册表单, JS fetch 提交 JSON)
- [x] `templates/problem_list.html` (含 `{{PROBLEM_ROWS}}` 占位符)
- [x] `templates/problem_detail.html`:
  - [x] `{{TITLE}}`, `{{DESCRIPTION}}` (Markdown→HTML), `{{TEMPLATE}}`, `{{ID}}`, `{{DIFFICULTY}}`, `{{SAMPLE_CASES}}`
  - [x] `<textarea>` 代码编辑区 + Submit 按钮 + `{{RESULT}}`
- [x] `templates/admin_panel.html` (含 `{{PROBLEM_ROWS}}`, 含操作按钮)
- [x] `templates/admin_problem_form.html` (新建/编辑复用: `{{TITLE}}`, `{{DIFFICULTY_OPTIONS}}`, `{{CONTENT}}`, `{{TEMPLATE}}`)
- [x] `templates/admin_testcases.html` (用例列表 + 添加表单, 含 `{{PROBLEM_ID}}`, `{{TESTCASE_ROWS}}`)
- [x] `templates/admin_users.html` (含 `{{USER_ROWS}}`)
- [x] `static/style.css` (全局样式: 导航栏、表格、表单、按钮、代码编辑器、难度徽标、响应式)
- [x] 运行 `make test` 全部通过 (83/83)
- [x] API 集成测试全部通过 (32/32)

### Phase 7: 主服务器整合
- [x] `server.cc`: 重构所有 GET 路由使用 `read_template()` + `replace_all()` 加载模板
- [x] 保留辅助函数: `build_table_rows`, `build_admin_rows`, `build_user_rows`, `build_tc_rows`, `build_sample_cases`, `build_difficulty_options`
- [x] 静态文件挂载 `static/` → `/`，启动 8080 端口
- [x] 并发: cpp-httplib 默认多线程 (无需手动管理)
- [x] 运行 `make test` 全部通过 (83/83)
- [x] API 集成测试全部通过 (32/32)

### Phase 8: 测试 & 收尾
- [x] 编写测试题目数据 (2 道: Easy A+B Problem + Medium Reverse String, 各含 3 个测试用例)
- [x] 端到端测试: test_api.py 覆盖 注册→登录→浏览题目→提交代码→判题(AC/WA/CE/TLE/RE)
- [x] 端到端测试: test_api.py 覆盖 admin 创建/编辑/删除题目 + 管理用例 + 用户列表
- [x] 沙箱安全测试: test_judge.cc 覆盖 TLE (死循环), MLE (malloc 炸弹), RE (SIGSEGV/SIGABRT), 网络拦截 (seccomp)
- [x] 验证: 未登录访问 `/problems` → 302 `/login` (test_api.py 4.2)
- [x] 验证: 非 admin 访问 `/admin` → 403 (test_api.py 4.3, 4.4)
- [x] README.md (构建 & 运行说明)
- [x] 运行 `make test` 全部通过 (83/83)
- [x] API 集成测试全部通过 (32/32)

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
- ❌ 不引入前端框架 (React/Vue 等), 仅原生 JS fetch
- ❌ 不做 JSON Web Token (JWT), 使用 Cookie + Session

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

> **版本**: v1.0 | **日期**: 2026-06-10 | **状态**: MVP 完成 (Phases 1-8)
  