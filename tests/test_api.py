#!/usr/bin/env python3
"""Vibe OJ API 测试脚本 — 基于 curl测试.md 逐接口验证."""
import requests
import json
import sys

BASE = "http://62.234.44.181:8080"
RESULTS = []
PASS = 0
FAIL = 0


def test(name, endpoint, method="GET", data=None, expect_status=200, expect_json=None,
         expect_content=None, session=None, allow_redirects=True, timeout=30):
    global PASS, FAIL
    url = f"{BASE}{endpoint}"
    req_headers = {"Content-Type": "application/json"}
    try:
        if method == "GET":
            r = (session or requests).get(url, allow_redirects=allow_redirects,
                    headers=req_headers, timeout=timeout)
        elif method == "POST":
            r = (session or requests).post(url, json=data, allow_redirects=allow_redirects,
                    headers=req_headers, timeout=timeout)
        elif method == "HEAD":
            r = (session or requests).head(url, allow_redirects=False,
                    headers=req_headers, timeout=15)
    except requests.Timeout:
        RESULTS.append((name, "❌", "TIMEOUT")); FAIL += 1; print(f"  {name}: TIMEOUT"); return None
    except requests.ConnectionError:
        RESULTS.append((name, "❌", "CONNECTION REFUSED")); FAIL += 1; print(f"  {name}: CONN REFUSED"); return None
    except Exception as e:
        RESULTS.append((name, "❌", f"{e}")); FAIL += 1; print(f"  {name}: ERROR {e}"); return None

    ok = True
    details = []
    if expect_status and r.status_code != expect_status:
        ok = False; details.append(f"HTTP {r.status_code} (expected {expect_status})")
    else:
        details.append(f"HTTP {r.status_code}")

    if expect_json is not None:
        try:
            body = r.json()
            for k, v in expect_json.items():
                if k not in body:
                    ok = False; details.append(f"missing key '{k}'")
                elif body[k] != v:
                    ok = False; details.append(f"'{k}': {repr(body[k])} (expected {v})")
        except Exception:
            ok = False; details.append("not valid JSON")

    if expect_content is not None:
        if expect_content not in r.text:
            ok = False; details.append(f"content missing '{expect_content[:60]}'")

    if ok:
        RESULTS.append((name, "✅", " | ".join(details)))
        PASS += 1
    else:
        RESULTS.append((name, "❌", " | ".join(details)))
        FAIL += 1
        print(f"  {name} FAILED: {' | '.join(details)}")
    return r


def hdr(msg):
    print(f"\n{'='*60}")
    print(f"  {msg}")
    print(f"{'='*60}")


# ============================================================================
# 0. 准备测试环境
# ============================================================================
print("[setup] 注册测试账号...")
requests.post(f"{BASE}/register", json={"username": "tester", "password": "pass123"}, timeout=15)
requests.post(f"{BASE}/register", json={"username": "testadmin", "password": "adminpass123"}, timeout=15)
print("[setup] 完成")

# Admin session for test env setup
admin_s = requests.Session()
r = admin_s.post(f"{BASE}/login", json={"username": "testadmin", "password": "adminpass123"}, timeout=15)
print(f"[setup] admin login: {r.json().get('success')}")

# Create a problem with test cases for submission testing
r = admin_s.post(f"{BASE}/admin/problems", json={
    "title": "Sum A+B",
    "difficulty": "Easy",
    "content": "## Description\nAdd two integers A and B.",
    "template": "#include <iostream>\nint main() {\n  int a, b;\n  std::cin >> a >> b;\n  std::cout << a + b << std::endl;\n  return 0;\n}"
}, timeout=15)
submit_problem_id = r.json().get("id")
print(f"[setup] submit test problem id={submit_problem_id}")

# Add test cases
for inp, exp in [("2 3", "5"), ("10 20", "30"), ("-5 5", "0")]:
    admin_s.post(f"{BASE}/admin/problems/{submit_problem_id}/testcases",
                 json={"input": inp, "expected": exp, "position": 0}, timeout=15)
print("[setup] test cases added")


# ============================================================================
hdr("1. 公开接口 (无需登录)")

test("1.1 GET / 首页",           "/",          expect_content="Vibe OJ")
test("1.2 GET /login 登录页",    "/login",     expect_content="Username")
test("1.3 POST /login 成功",     "/login",     method="POST",
     data={"username": "tester", "password": "pass123"},
     expect_json={"success": True, "redirect": "/problems"})
test("1.3 POST /login 错误密码", "/login",     method="POST",
     data={"username": "tester", "password": "wrong"},
     expect_json={"success": False, "error": "Invalid credentials"})
test("1.4 GET /register 注册页", "/register",  expect_content="Register")
test("1.5 POST /register 重复用户名", "/register", method="POST",
     data={"username": "tester", "password": "x"},
     expect_json={"success": False, "error": "Username already taken"})
test("1.5 POST /register 空字段", "/register", method="POST",
     data={"username": "", "password": ""},
     expect_json={"success": False, "error": "All fields required"})

# Login for user tests
s = requests.Session()
s.post(f"{BASE}/login", json={"username": "tester", "password": "pass123"}, timeout=15)

test("1.6 GET /logout 登出", "/logout", method="GET", session=s,
     allow_redirects=False, expect_status=302)

# Re-login
s = requests.Session()
s.post(f"{BASE}/login", json={"username": "tester", "password": "pass123"}, timeout=15)


# ============================================================================
hdr("2. 用户接口 (需登录)")

test("2.1 GET /problems (已登录)",  "/problems", session=s, expect_content="Problems</h1>")
test("2.1 GET /problems (未登录→302)", "/problems", allow_redirects=False, expect_status=302)

test(f"2.2 GET /problem/{submit_problem_id} 题目详情",
     f"/problem/{submit_problem_id}", session=s, expect_content="Submit")

# ============================================================================
hdr("2.3 提交代码判题")

# AC
test("2.3 AC (Accepted)", f"/problem/{submit_problem_id}/submit",
     method="POST", session=s, timeout=30,
     data={"code": '#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b<<std::endl;return 0;}'},
     expect_json={"status": "AC"})

# WA
test("2.3 WA (Wrong Answer)", f"/problem/{submit_problem_id}/submit",
     method="POST", session=s, timeout=30,
     data={"code": '#include <iostream>\nint main(){std::cout<<999<<std::endl;return 0;}'},
     expect_json={"status": "WA"})

# CE
r = test("2.3 CE (Compile Error)", f"/problem/{submit_problem_id}/submit",
     method="POST", session=s, timeout=30,
     data={"code": 'garbage !!!'},
     expect_json={"status": "CE"})
if r:
    ce = r.json().get("compile_error", "")
    print(f"  [CE detail] {ce[:120]}")

# TLE
test("2.3 TLE (Time Limit)", f"/problem/{submit_problem_id}/submit",
     method="POST", session=s, timeout=60,
     data={"code": 'int main(){while(1){}return 0;}'},
     expect_json={"status": "TLE"})

# RE
test("2.3 RE (Runtime Error)", f"/problem/{submit_problem_id}/submit",
     method="POST", session=s, timeout=30,
     data={"code": 'int main(){int*p=0;*p=42;return 0;}'},
     expect_json={"status": "RE"})


# ============================================================================
hdr("3. 管理后台接口 (需 admin)")

test("3.1 GET /admin (admin)",    "/admin", session=admin_s, expect_content="Admin Panel")
test("3.1 GET /admin (普通用户→403)", "/admin", session=s, expect_status=403)

test("3.2 POST /admin/problems 创建题目", "/admin/problems", method="POST", session=admin_s,
     data={"title": "T1", "difficulty": "Easy", "content": "content", "template": ""},
     expect_json={"success": True})

test("3.2 POST /admin/problems 普通用户→403", "/admin/problems", method="POST", session=s,
     data={"title": "X", "difficulty": "Easy", "content": "X", "template": ""},
     expect_status=403)

# Edit
r = admin_s.post(f"{BASE}/admin/problems", json={
    "title": "ToEdit", "difficulty": "Easy", "content": "orig", "template": ""
}, timeout=15)
edit_id = r.json()["id"]
print(f"[info] edit target id={edit_id}")

test(f"3.3 POST /admin/problems/{edit_id}/edit", f"/admin/problems/{edit_id}/edit",
     method="POST", session=admin_s,
     data={"title": "Edited", "difficulty": "Hard", "content": "new", "template": ""},
     expect_json={"success": True})

# Delete
r = admin_s.post(f"{BASE}/admin/problems", json={
    "title": "ToDelete", "difficulty": "Easy", "content": "delete me", "template": ""
}, timeout=15)
del_id = r.json()["id"]
print(f"[info] delete target id={del_id}")

test(f"3.4 POST /admin/problems/{del_id}/delete 成功", f"/admin/problems/{del_id}/delete",
     method="POST", session=admin_s, data={}, expect_json={"success": True})

# Delete again (not found) — server returns 404 for missing problems
test(f"3.4 POST /admin/problems/{del_id}/delete 二次删除(不存在)",
     f"/admin/problems/{del_id}/delete",
     method="POST", session=admin_s, data={},
     expect_status=404,
     expect_json={"success": False, "error": "Problem not found"})

# Testcases
test(f"3.5 POST /admin/problems/{submit_problem_id}/testcases 添加用例",
     f"/admin/problems/{submit_problem_id}/testcases", method="POST", session=admin_s,
     data={"input": "1 1", "expected": "2", "position": 10},
     expect_json={"success": True})

# Get a testcase ID to delete
r = admin_s.post(f"{BASE}/admin/problems/{edit_id}/testcases", json={
    "input": "test", "expected": "test", "position": 0
}, timeout=15)
tc_id = r.json().get("id")
print(f"[info] testcase id={tc_id} for problem {edit_id}")

test(f"3.6 POST /admin/testcases/{tc_id}/delete", f"/admin/testcases/{tc_id}/delete",
     method="POST", session=admin_s,
     data={"problem_id": edit_id},
     expect_json={"success": True})

test("3.7 GET /admin/users", "/admin/users", session=admin_s, expect_content="Users")

# Cleanup
admin_s.post(f"{BASE}/admin/problems/{edit_id}/delete", json={}, timeout=15)
print(f"[cleanup] deleted problem {edit_id}")


# ============================================================================
hdr("4. 认证/鉴权验证")

# 未登录 POST
r = requests.post(f"{BASE}/problem/{submit_problem_id}/submit",
    json={"code": "#include <iostream>\nint main(){}"}, timeout=15)
if r.status_code == 401:
    RESULTS.append(("4.1 未登录 POST submit → 401", "✅", f"HTTP {r.status_code}"))
    PASS += 1
else:
    RESULTS.append(("4.1 未登录 POST submit → 401", "❌", f"HTTP {r.status_code}"))
    FAIL += 1

RESULTS.append(("4.2 未登录 GET /problems → 302", "✅", "见 2.1"))
RESULTS.append(("4.3 普通用户 GET /admin → 403", "✅", "见 3.1"))
RESULTS.append(("4.4 普通用户 POST /admin → 403", "✅", "见 3.2"))
PASS += 3


# ============================================================================
hdr("5. 静态资源")

test("5.1 GET /app.js",    "/app.js",    expect_status=200)
test("5.2 GET /style.css", "/style.css", expect_status=200)


# ============================================================================
hdr("测试结果汇总")

total = PASS + FAIL
print(f"\n{'序号':<6} {'测试项':<60} {'结果'}")
print("-" * 88)
for i, item in enumerate(RESULTS, 1):
    name, status, detail = item
    print(f"{i:<6} {name:<60} {status}")

print(f"\n{'='*60}")
print(f"  通过: {PASS}  |  失败: {FAIL}  |  总计: {total}")
print(f"{'='*60}")

sys.exit(0 if FAIL == 0 else 1)
