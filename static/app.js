// === Vibe OJ Frontend ===
function initCodeEditor() {
  var ta = document.getElementById('code-area');
  if (!ta) return;

  // Create wrapper
  var wrapper = document.createElement('div');
  wrapper.className = 'code-editor-wrapper';
  ta.parentNode.insertBefore(wrapper, ta);

  // Line numbers gutter
  var gutter = document.createElement('div');
  gutter.className = 'code-editor-gutter';

  wrapper.appendChild(gutter);
  wrapper.appendChild(ta);

  function update() {
    var text = ta.value;
    var lines = text.split('\n');

    // Update gutter
    var gutterHtml = '';
    for (var i = 0; i < lines.length; i++) {
      gutterHtml += '<span>' + (i + 1) + '</span>';
    }
    gutter.innerHTML = gutterHtml;
  }

  function syncScroll() {
    gutter.scrollTop = ta.scrollTop;
  }

  // Tab key support
  ta.addEventListener('keydown', function(e) {
    if (e.key === 'Tab') {
      e.preventDefault();
      var start = ta.selectionStart;
      var end = ta.selectionEnd;
      if (start !== end) {
        var before = ta.value.substring(0, ta.selectionStart);
        var sel = ta.value.substring(ta.selectionStart, ta.selectionEnd);
        var after = ta.value.substring(ta.selectionEnd);
        var lines = sel.split('\n');
        var indented = lines.map(function(l) { return '  ' + l; }).join('\n');
        ta.value = before + indented + after;
        ta.selectionStart = start;
        ta.selectionEnd = start + indented.length;
      } else {
        document.execCommand('insertText', false, '  ');
      }
      update();
      syncScroll();
    }
    if (e.key === 'Enter') {
      setTimeout(function() {
        var pos = ta.selectionStart;
        var before = ta.value.substring(0, pos);
        var prevLine = before.split('\n').slice(-2, -1)[0] || '';
        var indent = prevLine.match(/^(\s*)/)[1];
        if (prevLine.trimRight().endsWith('{')) indent += '  ';
        ta.setRangeText(indent, pos, pos, 'end');
        update();
        syncScroll();
      }, 0);
    }
  });

  ta.addEventListener('input', function() { update(); syncScroll(); });
  ta.addEventListener('scroll', syncScroll);
  ta.addEventListener('mousewheel', function() { setTimeout(syncScroll, 0); });

  // Initial render
  update();
}

// Update submitCode to use the new editor
function submitCode(problemId) {
  var code = document.getElementById('code-area').value;
  var resultEl = document.getElementById('submit-result');
  resultEl.innerHTML = '<p>判题中...</p>';
  postJSON('/problem/' + problemId + '/submit', {code: code})
    .then(function(r) {
      var color = r.status === 'AC' ? 'green' : (r.status === 'CE' ? '#c0a000' : 'red');
      var html = '<h3>判题结果</h3><p style="color:' + color + ';font-size:1.2em;font-weight:bold">' + r.status + '</p>';
      if (r.status === 'CE') {
        html += '<pre>' + (r.compile_error || '') + '</pre>';
      } else if (r.status === 'WA') {
        html += '<p>第 ' + r.failed_case + ' 个测试点失败</p>';
        html += '<p><strong>期望输出:</strong></p><pre>' + r.expected_output + '</pre>';
        html += '<p><strong>实际输出:</strong></p><pre>' + r.actual_output + '</pre>';
      }
      if (r.time_ms > 0) html += '<p>耗时: ' + r.time_ms + 'ms</p>';
      resultEl.innerHTML = html;
    })
    .catch(function(e) {
      resultEl.innerHTML = '<p style="color:var(--color-danger)">错误: ' + e.message + '</p>';
    });
}

function postJSON(url, data) {
  return fetch(url, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  }).then(function(r) {
    if (!r.ok) return r.json().then(function(j) { throw new Error(j.error || r.statusText); });
    return r.json();
  });
}

function showError(elId, msg) {
  var el = document.getElementById(elId);
  if (el) el.textContent = msg;
}

function doLogin() {
  var u = document.getElementById('login-username').value;
  var p = document.getElementById('login-password').value;
  postJSON('/login', {username: u, password: p})
    .then(function(r) {
      if (r.success) window.location.href = r.redirect;
      else showError('login-error', r.error);
    })
    .catch(function(e) { showError('login-error', e.message); });
}

function doRegister() {
  var u = document.getElementById('register-username').value;
  var p = document.getElementById('register-password').value;
  postJSON('/register', {username: u, password: p})
    .then(function(r) {
      if (r.success) window.location.href = r.redirect;
      else showError('register-error', r.error);
    })
    .catch(function(e) { showError('register-error', e.message); });
}

function createProblem() {
  var data = {
    title: document.getElementById('prob-title').value,
    difficulty: document.getElementById('prob-difficulty').value,
    content: document.getElementById('prob-content').value,
    template: document.getElementById('prob-template').value
  };
  postJSON('/admin/problems', data)
    .then(function(r) {
      if (r.success) window.location.href = r.redirect;
      else showError('form-result', r.error);
    })
    .catch(function(e) { showError('form-result', e.message); });
}

function updateProblem(id) {
  var data = {
    title: document.getElementById('prob-title').value,
    difficulty: document.getElementById('prob-difficulty').value,
    content: document.getElementById('prob-content').value,
    template: document.getElementById('prob-template').value
  };
  postJSON('/admin/problems/' + id + '/edit', data)
    .then(function(r) {
      if (r.success) window.location.href = r.redirect;
      else showError('form-result', r.error);
    })
    .catch(function(e) { showError('form-result', e.message); });
}

function deleteProblem(id) {
  if (!confirm('确定删除此题目及所有测试用例？')) return;
  postJSON('/admin/problems/' + id + '/delete', {})
    .then(function(r) {
      if (r.success) window.location.href = r.redirect;
      else alert(r.error);
    })
    .catch(function(e) { alert('错误: ' + e.message); });
}

function addTestCase(problemId) {
  var data = {
    input: document.getElementById('tc-input').value,
    expected: document.getElementById('tc-expected').value,
    position: parseInt(document.getElementById('tc-position').value) || 0
  };
  postJSON('/admin/problems/' + problemId + '/testcases', data)
    .then(function(r) {
      if (r.success) location.reload();
      else showError('tc-result', r.error);
    })
    .catch(function(e) { showError('tc-result', e.message); });
}

function deleteTestCase(caseId, problemId) {
  if (!confirm('确定删除此测试用例？')) return;
  postJSON('/admin/testcases/' + caseId + '/delete', {problem_id: problemId})
    .then(function(r) {
      if (r.success) location.reload();
      else alert(r.error);
    })
    .catch(function(e) { alert('错误: ' + e.message); });
}

// Boot
document.addEventListener('DOMContentLoaded', function() {
  initCodeEditor();
});
