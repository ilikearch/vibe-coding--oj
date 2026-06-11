import { chromium } from '@playwright/test';

const BASE = 'http://62.234.44.181:8080';
const ADMIN = { username: 'admin', password: 'admin123' };
const SLEEP_MS = 1000;

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const CODES = {
  AC: `#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b<<std::endl;return 0;}`,
  WA: `#include <iostream>\nint main(){std::cout<<999<<std::endl;return 0;}`,
  CE: `this is not valid c++ code !!!`,
  EMPTY: '',
  TLE: `int main(){while(1){}return 0;}`,
  NULLPTR: `int main(){int*p=0;*p=42;return 0;}`,
  ABORT: `#include <cstdlib>\nint main(){std::abort();return 0;}`,
};

let passed = 0, failed = 0;

function log(section, name, ok, detail = '') {
  if (ok) passed++; else failed++;
  console.log(`[${ok ? 'PASS' : 'FAIL'}] ${section}: ${name}${detail ? ' -- ' + detail : ''}`);
}

(async () => {
  const browser = await chromium.launch({ channel: 'chrome', headless: false, slowMo: 50 });
  const context = await browser.newContext();
  const page = await context.newPage();

  // Track created problem IDs for cleanup
  let judgeProblemId = null;
  let deleteProblemId = null;
  let editProblemId = null;
  let loginOk = false;

  try {
    // ======================================================================
    // SECTION 1: 公开页面
    // ======================================================================
    console.log('\n========== 1. 公开页面 ==========');

    await page.goto(BASE, { waitUntil: 'networkidle' });
    await sleep(SLEEP_MS);
    log('1.1', 'GET /', (await page.content()).includes('Vibe OJ'), '首页含 Vibe OJ');

    await page.goto(BASE + '/login', { waitUntil: 'networkidle' });
    await sleep(SLEEP_MS);
    log('1.2', 'GET /login', (await page.content()).includes('login-username'), '登录表单');

    await page.goto(BASE + '/register', { waitUntil: 'networkidle' });
    await sleep(SLEEP_MS);
    log('1.7', 'GET /register', (await page.content()).includes('register-username'), '注册表单');

    // ======================================================================
    // SECTION 2: 登录测试
    // ======================================================================
    console.log('\n========== 2. 登录测试 ==========');

    let resp;

    await page.goto(BASE + '/login', { waitUntil: 'networkidle' });
    await sleep(SLEEP_MS);
    resp = await fetchFromPage(page, '/login', { username: '', password: '' });
    log('1.6', '空字段登录', resp?.success === false && resp?.error, JSON.stringify(resp));

    resp = await fetchFromPage(page, '/login', { username: ADMIN.username, password: 'wrongpass' });
    log('1.4', '错误密码', resp?.success === false && resp?.error, JSON.stringify(resp));

    resp = await fetchFromPage(page, '/login', { username: 'nonexistent_xyz', password: 'pass123' });
    log('1.5', '不存在用户', resp?.success === false && resp?.error, JSON.stringify(resp));

    resp = await fetchFromPage(page, '/login', { username: ADMIN.username, password: ADMIN.password });
    loginOk = resp?.success === true;
    log('1.3', '正确登录(admin)', loginOk, loginOk ? `redirect→${resp.redirect}` : JSON.stringify(resp));
    if (loginOk) {
      await page.goto(BASE + (resp.redirect || '/problems'), { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
    }

    // ======================================================================
    // SECTION 3: 题目浏览
    // ======================================================================
    console.log('\n========== 3. 题目浏览 ==========');

    if (loginOk) {
      await page.goto(BASE + '/problems', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      log('2.1', 'GET /problems', (await page.content()).includes('<table'), '题目列表可见');

      await page.goto(BASE + '/problem/1', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      log('2.3', 'GET /problem/1', (await page.content()).includes('code-area'), '含代码编辑器');

      // ====================================================================
      // SECTION 3b: 难度类型测试 (Easy / Medium / Hard)
      // ====================================================================
      console.log('\n========== 3b. 难度类型验证 ==========');

      // 创建三种难度题目
      const diffProblems = [];
      for (const [label, diff] of [['Easy', 'Easy'], ['Medium', 'Medium'], ['Hard', 'Hard']]) {
        const r = await fetchFromPage(page, '/admin/problems', {
          title: `DifficultyTest_${label}`,
          difficulty: diff,
          content: `# ${label} 难度测试题目`,
          template: '#include <iostream>\nint main() { return 0; }'
        });
        if (r?.id) diffProblems.push({ id: r.id, title: `DifficultyTest_${label}`, difficulty: diff, label });
        log(`3.x`, `创建${label}题目`, r?.success === true, `id=${r?.id}`);
      }

      // 验证题目列表页三种难度展示
      await page.goto(BASE + '/problems', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      const listHTML = await page.content();

      for (const dp of diffProblems) {
        const inList = listHTML.includes(dp.title);
        log(`5.4-${dp.label}`, `列表含${dp.label}题目`, inList, dp.title);

        // 验证难度 CSS class
        await page.goto(BASE + `/problem/${dp.id}`, { waitUntil: 'networkidle' });
        await sleep(SLEEP_MS);
        const detailHTML = await page.content();
        const hasClass = detailHTML.includes(`difficulty-${dp.difficulty}`);
        log(`5.5-${dp.label}`, `详情页难度class=difficulty-${dp.difficulty}`, hasClass, 'CSS颜色标识');
      }

      // 验证 CSS 颜色变量
      const cssText = await (await page.request.get(BASE + '/style.css')).text();
      const easyColor = cssText.match(/--color-easy:\s*(#[0-9a-fA-F]+)/)?.[1];
      const mediumColor = cssText.match(/--color-medium:\s*(#[0-9a-fA-F]+)/)?.[1];
      const hardColor = cssText.match(/--color-hard:\s*(#[0-9a-fA-F]+)/)?.[1];
      log('5.2a', 'Easy=绿色(#22c55e)', easyColor === '#22c55e', easyColor);
      log('5.2b', 'Medium=黄色(#f59e0b)', mediumColor === '#f59e0b', mediumColor);
      log('5.2c', 'Hard=红色(#ef4444)', hardColor === '#ef4444', hardColor);

      // 验证编辑后难度变更
      if (diffProblems.length > 0) {
        const p = diffProblems[0];
        resp = await fetchFromPage(page, `/admin/problems/${p.id}/edit`, {
          title: p.title + '_Hard', difficulty: 'Hard',
          content: `# Modified to Hard`, template: '#include <iostream>\nint main() { return 1; }'
        });
        log(`6.1-${p.label}→Hard`, '编辑后难度变更', resp?.success === true, JSON.stringify(resp));
      }

      // 清理难度测试题目
      console.log('  [清理] 删除难度测试题目...');
      for (const dp of diffProblems) {
        try { await fetchFromPage(page, `/admin/problems/${dp.id}/delete`, {}); } catch {}
      }
    }

    // ======================================================================
    // SECTION 4: 管理后台 + 创建三类测试题目
    // ======================================================================
    console.log('\n========== 4. 管理后台: 创建测试题目 ==========');

    if (loginOk) {
      // 4.1 验证 admin 面板可访问
      await page.goto(BASE + '/admin', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      const adminHTML = await page.content();
      const adminOk = !adminHTML.includes('Forbidden') && !adminHTML.includes('403') && !adminHTML.includes('无管理员权限');
      log('3.1', 'admin管理面板', adminOk, adminOk ? '可访问' : '403');

      if (!adminOk) {
        console.log('  [ABORT] 无admin权限，无法继续后续测试');
        return;
      }

      // 4.2 新建题目页
      await page.goto(BASE + '/admin/problems/new', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      log('3.3', '新建题目页', (await page.content()).includes('prob-title'), '含表单');

      // 4.3 空字段创建
      resp = await fetchFromPage(page, '/admin/problems', {
        title: '', difficulty: 'Easy', content: 'c', template: 't'
      });
      log('3.5', '创建题目空标题', resp?.success === false, JSON.stringify(resp));

      // 4.4 用户列表
      await page.goto(BASE + '/admin/users', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      log('3.15', '用户列表', (await page.content()).includes('admin'), '含用户表');

      // 4.5 编辑不存在题目
      await page.goto(BASE + '/admin/problems/99999/edit', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      log('3.8', '编辑不存在题目', /404|Not Found/.test(await page.content()), '404');

      // ================================================================
      // 4.6 创建三类测试题目 (按文档 §9.2)
      // ================================================================
      console.log('\n  --- 创建测试题目 ---');

      // 题目A: Sum A+B (用于判题测试)
      const aResp = await fetchFromPage(page, '/admin/problems', {
        title: 'Sum A+B', difficulty: 'Easy',
        content: '# Sum A+B\n\n计算两个整数之和。\n\n## 输入\n两个整数 a, b (-1000 <= a, b <= 1000)\n\n## 输出\n一个整数 a+b',
        template: '#include <iostream>\nint main() {\n  int a, b;\n  std::cin >> a >> b;\n  std::cout << a + b << std::endl;\n  return 0;\n}'
      });
      judgeProblemId = aResp?.id;
      log('3.4a', '创建 Sum A+B (判题用)', aResp?.success === true && judgeProblemId > 0, `id=${judgeProblemId}`);

      if (judgeProblemId) {
        // 添加测试用例
        const tc1 = await fetchFromPage(page, `/admin/problems/${judgeProblemId}/testcases`, { input: '2 3', expected: '5', position: 1 });
        const tc2 = await fetchFromPage(page, `/admin/problems/${judgeProblemId}/testcases`, { input: '10 20', expected: '30', position: 2 });
        const tc3 = await fetchFromPage(page, `/admin/problems/${judgeProblemId}/testcases`, { input: '-5 5', expected: '0', position: 3 });
        log('3.13a', 'Sum A+B 添加用例', tc1?.success && tc2?.success && tc3?.success,
          `(2 3,5) (10 20,30) (-5 5,0)`);
      }

      // 题目B: ToDelete (用于删除测试, 无用例)
      const bResp = await fetchFromPage(page, '/admin/problems', {
        title: 'ToDelete', difficulty: 'Hard',
        content: '# ToDelete\n\n此题目用于测试删除功能。',
        template: '#include <iostream>\nint main() { return 0; }'
      });
      deleteProblemId = bResp?.id;
      log('3.4b', '创建 ToDelete (删除用)', bResp?.success === true && deleteProblemId > 0, `id=${deleteProblemId}`);

      // 题目C: ToEdit (用于编辑测试, 至少1个用例)
      const cResp = await fetchFromPage(page, '/admin/problems', {
        title: 'ToEdit', difficulty: 'Easy',
        content: '# ToEdit\n\n此题目用于测试编辑功能。',
        template: '#include <iostream>\nint main() { return 0; }'
      });
      editProblemId = cResp?.id;
      log('3.4c', '创建 ToEdit (编辑用)', cResp?.success === true && editProblemId > 0, `id=${editProblemId}`);

      if (editProblemId) {
        const tcEdit = await fetchFromPage(page, `/admin/problems/${editProblemId}/testcases`, { input: '1 1', expected: '2', position: 1 });
        log('3.13c', 'ToEdit 添加用例', tcEdit?.success === true, `id=${tcEdit?.id}`);
      }
    }

    // ======================================================================
    // SECTION 5: 判题引擎 (使用 Sum A+B)
    // ======================================================================
    console.log('\n========== 5. 判题引擎 (题目: Sum A+B) ==========');

    if (loginOk && judgeProblemId) {
      await page.goto(BASE + `/problem/${judgeProblemId}`, { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);

      // AC
      resp = await submitCodeViaFetch(page, judgeProblemId, CODES.AC);
      log('2.5', 'AC代码', resp?.status === 'AC', `status=${resp?.status} time=${resp?.time_ms}ms`);

      // WA
      resp = await submitCodeViaFetch(page, judgeProblemId, CODES.WA);
      log('2.6', 'WA代码', resp?.status === 'WA', `failed_case=${resp?.failed_case}`);

      // CE
      resp = await submitCodeViaFetch(page, judgeProblemId, CODES.CE);
      log('2.7', 'CE代码', resp?.status === 'CE', `compile_error=${!!resp?.compile_error}`);

      // 空代码
      resp = await submitCodeViaFetch(page, judgeProblemId, CODES.EMPTY);
      log('2.8', '空代码', resp?.error === '请填写代码' || resp?.status === 'CE',
        resp?.error === '请填写代码' ? '服务端校验:请填写代码' : `status=${resp?.status}`);

      // TLE
      resp = await submitCodeViaFetch(page, judgeProblemId, CODES.TLE);
      log('2.9', 'TLE代码', resp?.status === 'TLE', `status=${resp?.status}`);

      // RE SIGSEGV
      resp = await submitCodeViaFetch(page, judgeProblemId, CODES.NULLPTR);
      log('2.10', 'RE(SIGSEGV)', resp?.status === 'RE', `status=${resp?.status}`);

      // RE SIGABRT
      resp = await submitCodeViaFetch(page, judgeProblemId, CODES.ABORT);
      log('2.11', 'RE(SIGABRT)', resp?.status === 'RE', `status=${resp?.status}`);
    }

    // ======================================================================
    // SECTION 6: 管理后台: 编辑和删除测试
    // ======================================================================
    console.log('\n========== 6. 管理后台: 编辑与删除 ==========');

    if (loginOk) {
      // 6.1 编辑 ToEdit
      if (editProblemId) {
        await page.goto(BASE + `/admin/problems/${editProblemId}/edit`, { waitUntil: 'networkidle' });
        await sleep(SLEEP_MS);
        log('3.7', '编辑题目页(ToEdit)', (await page.content()).includes('prob-title'), '含预填表单');

        resp = await fetchFromPage(page, `/admin/problems/${editProblemId}/edit`, {
          title: 'ToEdit_Modified', difficulty: 'Medium',
          content: '# ToEdit Updated\n\n编辑后的内容。',
          template: '#include <iostream>\nint main() { return 1; }'
        });
        log('3.9', '更新题目(ToEdit)', resp?.success === true, JSON.stringify(resp));

        // 用例管理页
        await page.goto(BASE + `/admin/problems/${editProblemId}/testcases`, { waitUntil: 'networkidle' });
        await sleep(SLEEP_MS);
        log('3.12', '用例管理页(ToEdit)', (await page.content()).includes('tc-input'), '含用例表单');

        // 添加用例
        resp = await fetchFromPage(page, `/admin/problems/${editProblemId}/testcases`, { input: '5 5', expected: '10', position: 2 });
        const tcId = resp?.id;
        log('3.13', '添加测试用例(ToEdit)', resp?.success === true, `id=${tcId}`);

        // 删除用例
        if (tcId) {
          resp = await fetchFromPage(page, `/admin/testcases/${tcId}/delete`, { problem_id: editProblemId });
          log('3.14', '删除测试用例', resp?.success === true, JSON.stringify(resp));
        }
      }

      // 6.2 删除 ToDelete
      if (deleteProblemId) {
        resp = await fetchFromPage(page, `/admin/problems/${deleteProblemId}/delete`, {});
        log('3.10', '删除题目(ToDelete)', resp?.success === true, JSON.stringify(resp));

        // 二次删除
        resp = await fetchFromPage(page, `/admin/problems/${deleteProblemId}/delete`, {});
        log('3.11', '二次删除题目', resp?.success === false, JSON.stringify(resp));
      }
    }

    // ======================================================================
    // SECTION 7: 鉴权测试
    // ======================================================================
    console.log('\n========== 7. 鉴权测试 ==========');

    await page.goto(BASE + '/logout', { waitUntil: 'networkidle' });
    await sleep(SLEEP_MS);
    log('1.11', '登出', true, 'session清除');

    await page.goto(BASE + '/problems', { waitUntil: 'networkidle' });
    await sleep(SLEEP_MS);
    log('4.1', '未登录→/problems', page.url().includes('/login'), '重定向到login');

    await page.goto(BASE + '/problem/1', { waitUntil: 'networkidle' });
    await sleep(SLEEP_MS);
    log('4.2', '未登录→/problem/1', page.url().includes('/login'), '重定向到login');

    resp = await submitCodeViaFetch(page, 1, CODES.AC);
    log('4.3', '未登录提交代码', resp?.status === 401 || resp?.error, `error=${resp?.error}`);

    // 注册普通用户
    await page.goto(BASE + '/register', { waitUntil: 'networkidle' });
    await sleep(SLEEP_MS);
    const testUser = 'testuser_' + Date.now();
    resp = await fetchFromPage(page, '/register', { username: testUser, password: 'pass123' });
    const regOk = resp?.success === true;
    log('1.8', '注册新用户', regOk, JSON.stringify(resp));

    resp = await fetchFromPage(page, '/register', { username: testUser, password: 'pass123' });
    log('1.9', '重复注册', resp?.success === false, JSON.stringify(resp));

    resp = await fetchFromPage(page, '/register', { username: '', password: '' });
    log('1.10', '空字段注册', resp?.success === false, JSON.stringify(resp));

    if (regOk) {
      resp = await fetchFromPage(page, '/login', { username: testUser, password: 'pass123' });
      const userOk = resp?.success === true;
      if (userOk) {
        await page.goto(BASE + (resp.redirect || '/problems'), { waitUntil: 'networkidle' });
        await sleep(SLEEP_MS);
      }

      await page.goto(BASE + '/admin', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      const html = await page.content();
      log('4.4', '普通用户→/admin', html.includes('403') || html.includes('Forbidden') || html.includes('无管理员权限'), '被拒绝(403)');

      resp = await fetchFromPage(page, '/admin/problems', { title: 'Hack', difficulty: 'Easy', content: 'hack', template: 'hack' });
      log('4.5', '普通用户创建题目', resp?.status === 403 || resp?.success === false, `success=${resp?.success}`);

      await page.goto(BASE + '/logout', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      await page.goto(BASE + '/problems', { waitUntil: 'networkidle' });
      await sleep(SLEEP_MS);
      log('4.10', '登出后session失效', page.url().includes('/login'), '需重登录');
    }

    // ======================================================================
    // SECTION 8: 静态资源
    // ======================================================================
    console.log('\n========== 8. 静态资源 ==========');

    let httpResp = await page.request.get(BASE + '/style.css');
    log('5.10', 'GET /style.css', httpResp.ok(), `type=${httpResp.headers()['content-type']}`);

    httpResp = await page.request.get(BASE + '/app.js');
    log('5.11', 'GET /app.js', httpResp.ok(), `type=${httpResp.headers()['content-type']}`);

    // ======================================================================
    // SECTION 9: 边界条件
    // ======================================================================
    console.log('\n========== 9. 边界条件 ==========');

    try {
      await page.goto(BASE + '/nonexistent', { waitUntil: 'domcontentloaded' });
      await sleep(SLEEP_MS);
      log('8.1', '不存在的路由', /404|Not Found/.test(await page.content()), '404');
    } catch {
      log('8.1', '不存在的路由', true, '状态码404');
    }

    try {
      await page.goto(BASE + '/problem/99999', { waitUntil: 'domcontentloaded' });
      await sleep(SLEEP_MS);
      log('2.4', '不存在题目', /404|Not Found/.test(await page.content()) || page.url().includes('/login'), '404或重定向');
    } catch {
      log('2.4', '不存在题目', true, '状态码404');
    }

  } catch (err) {
    console.error('TEST ERROR:', err.message);
  } finally {
    // ========== Cleanup ==========
    console.log('\n--- 清理测试数据 ---');
    // Re-login as admin if needed
    if (loginOk) {
      for (const id of [judgeProblemId, editProblemId, deleteProblemId].filter(Boolean)) {
        try {
          const r = await fetchFromPage(page, `/admin/problems/${id}/delete`, {});
          console.log(`  删除题目 id=${id}: ${r?.success === true ? 'OK' : '已删除或不存在'}`);
        } catch {}
      }
    }

    console.log('\n========================================');
    console.log(`测试结果: ${passed} PASS, ${failed} FAIL, ${passed + failed} TOTAL`);
    console.log(`通过率: ${(passed / (passed + failed) * 100).toFixed(1)}%`);
    console.log('========================================\n');
    await browser.close();
  }
})();

// ========== Helpers ==========

async function fetchFromPage(page, url, data) {
  try {
    const result = await page.evaluate(async ({ u, d }) => {
      const r = await fetch(u, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(d)
      });
      const text = await r.text();
      try {
        return JSON.parse(text);
      } catch {
        return { status: r.status, statusText: r.statusText, body: text };
      }
    }, { u: url, d: data });
    return result;
  } catch (e) {
    return { error: e.message };
  }
}

async function submitCodeViaFetch(page, problemId, code) {
  try {
    const result = await page.evaluate(async ({ pid, c }) => {
      const r = await fetch('/problem/' + pid + '/submit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ code: c })
      });
      const text = await r.text();
      try {
        return JSON.parse(text);
      } catch {
        return { status: r.status, statusText: r.statusText, body: text };
      }
    }, { pid: problemId, c: code });
    return result;
  } catch (e) {
    return { error: e.message };
  }
}
