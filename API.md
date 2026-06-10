# Vibe OJ API 文档

基于代码实现 (`server.cc`) 的完整接口交互文档。

---
## 通用约定

| 项目 | 说明 |
|------|------|
| Base URL | `http://62.234.44.181:8080` |
| GET 响应 | `text/html` (服务端渲染完整页面) |
| POST 请求 | `Content-Type: application/json` |
| POST 响应 | `Content-Type: application/json` |
| 认证方式 | Cookie `session_id` (bcrypt + Session Token) |
| Admin 鉴权 | 中间件检查 `role=admin`, 非 admin 返回 `{"success":false,"error":"Forbidden"}` (403) |
| 未登录访问 | POST 接口返回 `{"success":false,"error":"Unauthorized"}` (401); GET 接口 302 重定向 `/login` |

### 通用响应结构

```json
// 成功
{"success": true}
{"success": true, "redirect": "/problems", "id": 1}

// 失败
{"success": false, "error": "错误描述"}
```

---
## 1. 公开接口 (无需登录)

### `GET /`
首页落地页。返回完整 HTML 页面，展示 OJ 简介、功能特性、题目/用户统计、登录/注册入口。

| 属性 | 值 |
|------|-----|
| 认证 | 否 |
| 请求体 | — |
| 响应 | `text/html` |

---

### `GET /login`
登录页。返回完整 HTML 页面，包含 `username` / `password` 输入框和登录按钮。

| 属性 | 值 |
|------|-----|
| 认证 | 否 |
| 请求体 | — |
| 响应 | `text/html` |

---

### `POST /login`
处理登录请求。验证用户名密码，成功后设置 session cookie。

**请求体**:
```json
{
  "username": "alice",
  "password": "secret123"
}
```

**响应**:

成功 (200):
```json
{
  "success": true,
  "redirect": "/problems"
}
```
> 同时设置 `Set-Cookie: session_id=xxxx; Path=/; HttpOnly`

失败 (200):
```json
{
  "success": false,
  "error": "Invalid credentials"
}
```

---

### `GET /register`
注册页。返回完整 HTML 页面，包含 `username` / `password` 输入框和注册按钮。

| 属性 | 值 |
|------|-----|
| 认证 | 否 |
| 请求体 | — |
| 响应 | `text/html` |

---

### `POST /register`
处理注册请求。创建新用户，默认 `role=user`，密码 bcrypt 哈希存储。

**请求体**:
```json
{
  "username": "bob",
  "password": "mypass"
}
```

**响应**:

成功 (200):
```json
{
  "success": true,
  "redirect": "/login"
}
```

失败 (200):
```json
{"success": false, "error": "All fields required"}
{"success": false, "error": "Username already taken"}
{"success": false, "error": "Bad request: ..."}
```

---

### `GET /logout`
清除 session，重定向首页。

| 属性 | 值 |
|------|-----|
| 认证 | 否 |
| 请求体 | — |
| 响应 | 302 重定向 `/`, 同时清除 Cookie |

---

## 2. 用户接口 (需登录)

所有接口需携带有效 Cookie `session_id=xxx`。

---

### `GET /problems`
题目列表页。返回完整 HTML 页面，以表格展示所有题目 (ID / 标题 / 难度)，标题可点击跳转详情页。

| 属性 | 值 |
|------|-----|
| 认证 | 是 |
| 请求体 | — |
| 响应 | `text/html` |

未登录: 302 重定向 `/login`

---

### `GET /problem/:id`
题目详情页。返回完整 HTML 页面。

- 左侧: 题目描述 (Markdown→HTML) + 示例测试用例
- 右侧: 代码编辑区 (`<textarea>`)、提交按钮 (`onclick="submitCode(id)"`)、判题结果区 (`<div id="submit-result">`)

| 属性 | 值 |
|------|-----|
| 认证 | 是 |
| 路径参数 | `id` (int) — 题目 ID |
| 请求体 | — |
| 响应 | `text/html` |

`id` 不存在时返回 404。

---

### `POST /problem/:id/submit`
提交代码进行判题。编译 + 沙箱执行每个测试用例，返回判题结果。

**请求体**:
```json
{
  "code": "#include <iostream>\nint main() {\n  int a,b;\n  std::cin>>a>>b;\n  std::cout<<a+b<<std::endl;\n  return 0;\n}"
}
```

**响应**:

编译错误 (200):
```json
{
  "status": "CE",
  "time_ms": 0,
  "memory_kb": 0,
  "compile_error": "code.cpp:1:1: error: ...",
  "failed_case": 0,
  "expected_output": "",
  "actual_output": ""
}
```

全部通过 (200):
```json
{
  "status": "AC",
  "time_ms": 42,
  "memory_kb": 1024,
  "compile_error": "",
  "failed_case": 0,
  "expected_output": "",
  "actual_output": ""
}
```

答案错误 (200):
```json
{
  "status": "WA",
  "time_ms": 15,
  "memory_kb": 0,
  "compile_error": "",
  "failed_case": 3,
  "expected_output": "42",
  "actual_output": "99"
}
```

超时/超内存/运行错误 (200):
```json
{"status": "TLE", "time_ms": 2012, ...}
{"status": "MLE", "time_ms": 1120, ...}
{"status": "RE",  "time_ms": 5, ...}
```

无测试用例 (200):
```json
{"success": false, "error": "No test cases for this problem"}
```

代码为空 (400):
```json
{"success": false, "error": "Code is required"}
```

未登录 (401):
```json
{"success": false, "error": "Unauthorized"}
```

### 判题状态码说明

| 状态 | 含义 | 判定条件 |
|------|------|----------|
| `AC` | Accepted | 所有测试用例输出匹配 |
| `WA` | Wrong Answer | 输出不匹配 (精确比对，末尾空白已 trim) |
| `TLE` | Time Limit Exceeded | 单用例 > 2s 真实时间, 或 SIGXCPU |
| `MLE` | Memory Limit Exceeded | SIGKILL (内存超限) |
| `RE` | Runtime Error | SIGSEGV / SIGABRT / 非零退出码 |
| `CE` | Compilation Error | g++ 编译失败 |

---

## 3. 管理后台接口 (需 role=admin)

所有接口需登录 + `role=admin`。

---

### `GET /admin`
管理面板主页。返回完整 HTML 页面，展示题目列表 (ID/标题/难度) + 操作入口 (编辑/用例/删除) + 新建题目/用户列表链接。

| 属性 | 值 |
|------|-----|
| 认证 | 是 (admin) |
| 请求体 | — |
| 响应 | `text/html` |

非 admin: 403 Forbidden

---

### `GET /admin/problems/new`
新建题目表单页。返回完整 HTML 页面。

| 属性 | 值 |
|------|-----|
| 认证 | admin |
| 请求体 | — |
| 响应 | `text/html` |

---

### `POST /admin/problems`
创建题目。

**请求体**:
```json
{
  "title": "Two Sum",
  "difficulty": "Easy",
  "content": "Given an array...\n\n**Example:**\n ...",
  "template": "#include <iostream>\n..."
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `title` | string | 是 | 题目标题 |
| `difficulty` | string | 是 | `Easy` / `Medium` / `Hard` |
| `content` | string | 是 | 题目描述 (Markdown) |
| `template` | string | 否 | 代码模板 (预填在编辑器) |

**响应**:

成功 (200):
```json
{"success": true, "id": 1, "redirect": "/admin"}
```

失败 (400):
```json
{"success": false, "error": "Title, difficulty, and content are required"}
```

---

### `GET /admin/problems/:id/edit`
编辑题目表单页。返回完整 HTML 页面，表单预填现有数据。

| 属性 | 值 |
|------|-----|
| 认证 | admin |
| 路径参数 | `id` (int) |
| 请求体 | — |
| 响应 | `text/html` |

---

### `POST /admin/problems/:id/edit`
更新题目。

**请求体**:
```json
{
  "title": "Two Sum (Updated)",
  "difficulty": "Medium",
  "content": "Updated description...",
  "template": "#include <iostream>\nusing namespace std;\n..."
}
```

**响应**:

成功 (200):
```json
{"success": true, "redirect": "/admin"}
```

失败 (400):
```json
{"success": false, "error": "Title, difficulty, and content are required"}
```

---

### `POST /admin/problems/:id/delete`
删除题目 (级联删除所有测试用例)。

| 属性 | 值 |
|------|-----|
| 认证 | admin |
| 路径参数 | `id` (int) |

**请求体**:
```json
{}
```

**响应**:

成功 (200):
```json
{"success": true, "redirect": "/admin"}
```

不存在 (404):
```json
{"success": false, "error": "Problem not found"}
```

---

### `GET /admin/problems/:id/testcases`
该题目的测试用例管理页。返回完整 HTML 页面，展示用例列表 + 添加用例表单。

| 属性 | 值 |
|------|-----|
| 认证 | admin |
| 路径参数 | `id` (int) — 题目 ID |
| 请求体 | — |
| 响应 | `text/html` |

---

### `POST /admin/problems/:id/testcases`
添加测试用例。

**请求体**:
```json
{
  "input": "2 3",
  "expected": "5",
  "position": 0
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `input` | string | 是 | stdin 输入数据 |
| `expected` | string | 是 | 期望 stdout 输出 |
| `position` | int | 否 | 用例执行序号，默认 0 |

**响应**:

成功 (200):
```json
{"success": true, "id": 42}
```
> 前端收到成功后执行 `location.reload()` 刷新页面

失败 (400):
```json
{"success": false, "error": "Input and expected output are required"}
```

---

### `POST /admin/testcases/:id/delete`
删除单个测试用例。

| 属性 | 值 |
|------|-----|
| 认证 | admin |
| 路径参数 | `id` (int) — 测试用例 ID |

**请求体**:
```json
{
  "problem_id": 1
}
```
> `problem_id` 供前端回调使用，后端不做额外校验。

**响应**:

成功 (200):
```json
{"success": true}
```
> 前端成功后 `location.reload()` 刷新页面

---

### `GET /admin/users`
用户列表页 (只读)。返回完整 HTML 页面，以表格展示所有用户 (ID / 用户名 / 角色 / 创建时间)。

| 属性 | 值 |
|------|-----|
| 认证 | admin |
| 请求体 | — |
| 响应 | `text/html` |

---

## 4. 静态资源

| 路径 | 文件 | 说明 |
|------|------|------|
| `/style.css` | `static/style.css` | 全局样式 |
| `/app.js` | `static/app.js` | 前端交互逻辑 (fetch JSON API, DOM 更新) |

静态资源通过 `svr.set_mount_point("/", "./static")` 映射，直接请求即可。

---

## 5. 错误码速查

| HTTP Status | 场景 |
|-------------|------|
| 200 | 正常响应 (含业务错误，通过 `success` 字段区分) |
| 302 | GET 页面未登录 → 重定向 `/login`; 登出 → 重定向 `/` |
| 400 | 请求格式错误 / JSON 解析失败 |
| 401 | POST 接口未登录 (`Unauthorized`) |
| 403 | 非 admin 访问管理接口 (`Forbidden`) |
| 404 | 资源不存在 (题目/用例未找到) |
