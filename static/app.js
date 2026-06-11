// === Vibe OJ Frontend v6 ===
var _ojTemplateCode = '';
var _ojProblemId = 0;
var _hlHighlight = null;

var CPP_KEYWORDS = /\b(if|else|for|while|do|switch|case|break|continue|return|goto|sizeof|new|delete|try|catch|throw|class|struct|enum|union|namespace|using|template|typename|typedef|public|private|protected|virtual|override|final|constexpr|consteval|static|const|mutable|volatile|explicit|noexcept|inline|friend|operator|this|decltype|alignas|alignof|static_cast|dynamic_cast|const_cast|reinterpret_cast|nullptr|true|false|and|or|not)\b/g;
var CPP_TYPES = /\b(int|char|bool|float|double|void|auto|long|short|unsigned|signed|size_t|wchar_t|int8_t|int16_t|int32_t|int64_t|uint8_t|uint16_t|uint32_t|uint64_t)\b/g;
var CPP_PREPROC = /^(\s*)(#\s*\w+.*)$/gm;
var CPP_BUILTIN = /\b(std|cin|cout|cerr|endl|string|vector|map|set|queue|stack|pair|tuple|array|deque|list|forward_list|unordered_map|unordered_set|priority_queue|unique_ptr|shared_ptr|weak_ptr|function|move|forward|make_shared|make_unique|optional|variant|string_view|span|initializer_list|max|min|sort|find|copy|swap|abs|sqrt|pow|log|sin|cos)\b/g;

function highlightCode(text) {
  if (!text) return '';

  var html = text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');

  html = html.replace(CPP_PREPROC, '$1<span class="hl-preproc">$2</span>');

  html = html.replace(CPP_KEYWORDS, '<span class="hl-keyword">$&</span>');
  html = html.replace(CPP_TYPES, '<span class="hl-type">$&</span>');
  html = html.replace(CPP_BUILTIN, '<span class="hl-builtin">$&</span>');

  html = html.replace(/"([^"\\]|\\.)*"/g, '<span class="hl-string">$&</span>');

  html = html.replace(/\/\/.*$/gm, '<span class="hl-comment">$&</span>');
  html = html.replace(/\/\*[\s\S]*?\*\//g, function(m) {
    return '<span class="hl-comment">' + m + '</span>';
  });

  html = html.replace(/\b(\d+\.?\d*(?:[eE][+-]?\d+)?)\b/g, '<span class="hl-number">$1</span>');

  return html;
}

function initCodeEditor() {
  var ta = document.getElementById('code-area');
  if (!ta) return;

  _ojTemplateCode = ta.value;

  var wrapper = document.createElement('div');
  wrapper.className = 'code-editor-wrapper';
  ta.parentNode.insertBefore(wrapper, ta);

  var gutter = document.createElement('div');
  gutter.className = 'code-editor-gutter';
  wrapper.appendChild(gutter);

  var content = document.createElement('div');
  content.className = 'code-editor-content';
  wrapper.appendChild(content);

  _hlHighlight = document.createElement('pre');
  _hlHighlight.className = 'code-editor-highlight';
  _hlHighlight.setAttribute('aria-hidden', 'true');
  var hlCode = document.createElement('code');
  _hlHighlight.appendChild(hlCode);
  content.appendChild(_hlHighlight);

  content.appendChild(ta);

  function update() {
    var text = ta.value;
    var lines = text.split('\n');
    var gutterHtml = '';
    for (var i = 0; i < lines.length; i++) {
      gutterHtml += '<span>' + (i + 1) + '</span>';
    }
    gutter.innerHTML = gutterHtml;

    var hl = highlightCode(text);
    if (!hl || hl.length === 0) hl = '\n';
    hlCode.innerHTML = hl + '\n';
    _hlHighlight.style.transform = 'translateY(-' + ta.scrollTop + 'px)';

    updateStatusBar();
  }

  function updateStatusBar() {
    var posEl = document.getElementById('editor-cursor-pos');
    var countEl = document.getElementById('editor-line-count');
    if (posEl) {
      var lines = ta.value.substr(0, ta.selectionStart).split('\n');
      var line = lines.length;
      var col = lines[lines.length - 1].length + 1;
      posEl.textContent = 'Ln ' + line + ', Col ' + col;
    }
    if (countEl) {
      var total = ta.value.split('\n').length;
      countEl.textContent = total + ' lines';
    }
  }

  function syncScroll() {
    gutter.scrollTop = ta.scrollTop;
    if (_hlHighlight) _hlHighlight.style.transform = 'translateY(-' + ta.scrollTop + 'px)';
  }

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
    if (e.ctrlKey && e.key === 'Enter') {
      e.preventDefault();
      if (_ojProblemId > 0) submitCode(_ojProblemId);
    }
    if (e.ctrlKey && e.key === 'l') {
      e.preventDefault();
      formatCode();
    }
    if (e.ctrlKey && e.key === 's') {
      e.preventDefault();
      formatCode();
    }
  });

  ta.addEventListener('input', function() { update(); syncScroll(); });
  ta.addEventListener('scroll', syncScroll);
  ta.addEventListener('click', updateStatusBar);
  ta.addEventListener('keyup', updateStatusBar);
  ta.addEventListener('mousewheel', function() { setTimeout(syncScroll, 0); });
  update();
}

function formatCode() {
  var ta = document.getElementById('code-area');
  if (!ta) return;

  var text = ta.value;
  var lines = text.split('\n');
  var result = [];
  var indentLevel = 0;
  var indentSize = 2;

  for (var i = 0; i < lines.length; i++) {
    var line = lines[i];
    var trimmed = line.trim();

    if (trimmed.length === 0) {
      result.push('');
      continue;
    }

    if (trimmed.startsWith('}') || trimmed.startsWith(')') ||
        (trimmed.startsWith(']'))) {
      indentLevel = Math.max(0, indentLevel - 1);
    }

    if (trimmed.startsWith('#') || trimmed.startsWith('//') ||
        trimmed.startsWith('/*') || trimmed === '*/') {
      result.push(trimmed);
      continue;
    }

    var indent = '';
    for (var j = 0; j < indentLevel * indentSize; j++) indent += ' ';
    result.push(indent + trimmed);

    if (trimmed.endsWith('{') || trimmed.endsWith('(') ||
        (trimmed.endsWith('['))) {
      indentLevel++;
    }

    if (trimmed === 'private:' || trimmed === 'public:' ||
        trimmed === 'protected:') {
      indentLevel = Math.max(0, indentLevel - 1);
      var pkIndent = '';
      for (var k = 0; k < indentLevel * indentSize; k++) pkIndent += ' ';
      result[result.length - 1] = pkIndent + trimmed;
      indentLevel++;
    }
  }

  var formatted = result.join('\n');
  ta.value = formatted;

  var evt = new Event('input', {bubbles: true});
  ta.dispatchEvent(evt);

  showToast('代码已格式化');
}

function resetTemplate() {
  var ta = document.getElementById('code-area');
  if (!ta) return;

  if (ta.value === _ojTemplateCode) {
    showToast('已经是模板代码');
    return;
  }

  if (ta.value.trim() !== _ojTemplateCode.trim()) {
    if (!confirm('确定重置代码？未保存的修改将丢失。')) return;
  }

  ta.value = _ojTemplateCode;
  var evt = new Event('input', {bubbles: true});
  ta.dispatchEvent(evt);
  showToast('已重置为模板代码');
}

function copyCode() {
  var ta = document.getElementById('code-area');
  if (!ta) return;

  var code = ta.value;
  if (!code.trim()) {
    showToast('没有代码可复制');
    return;
  }

  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(code).then(function() {
      showToast('已复制到剪贴板');
    }).catch(function() {
      fallbackCopy(ta);
    });
  } else {
    fallbackCopy(ta);
  }
}

function fallbackCopy(ta) {
  ta.select();
  ta.setSelectionRange(0, ta.value.length);
  try {
    document.execCommand('copy');
    showToast('已复制到剪贴板');
  } catch (e) {
    showToast('复制失败，请手动复制');
  }
  ta.blur();
}

var _toastTimer = null;
function showToast(msg) {
  var existing = document.getElementById('editor-toast');
  if (existing) existing.remove();
  if (_toastTimer) clearTimeout(_toastTimer);

  var toast = document.createElement('div');
  toast.id = 'editor-toast';
  toast.className = 'editor-toast';
  toast.textContent = msg;
  document.body.appendChild(toast);

  requestAnimationFrame(function() {
    toast.classList.add('show');
  });

  _toastTimer = setTimeout(function() {
    toast.classList.remove('show');
    setTimeout(function() { if (toast.parentNode) toast.remove(); }, 300);
    _toastTimer = null;
  }, 1800);
}

function submitCode(problemId) {
  _ojProblemId = problemId;
  var code = document.getElementById('code-area').value;
  var resultEl = document.getElementById('submit-result');
  if (!resultEl) return;

  var submitBtn = document.querySelector('.editor-submit-btn');
  if (submitBtn) {
    submitBtn.disabled = true;
    submitBtn.classList.add('btn-loading');
  }

  resultEl.innerHTML = '<div class="result-card result-pending"><div class="result-spinner"></div><div class="result-msg">判题中...</div></div>';

  postJSON('/problem/' + problemId + '/submit', {code: code})
    .then(function(r) {
      var statusClass = '';
      var statusIcon = '';
      if (r.status === 'AC') {
        statusClass = 'result-ac';
        statusIcon = '<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg>';
      } else if (r.status === 'CE') {
        statusClass = 'result-ce';
        statusIcon = '<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>';
      } else if (r.status === 'WA' || r.status === 'TLE' || r.status === 'RE') {
        statusClass = 'result-wa';
        statusIcon = '<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>';
      }

      var html = '<div class="result-card ' + statusClass + '">';
      html += '<div class="result-header">' + statusIcon + '<span class="result-status">' + r.status + '</span></div>';

      if (r.status === 'CE') {
        html += '<div class="result-detail"><div class="result-label">编译错误</div><pre class="result-code">' + escapeHtml(r.compile_error || '') + '</pre></div>';
      } else if (r.status === 'WA') {
        html += '<div class="result-detail"><div class="result-label">第 ' + r.failed_case + ' 个测试点失败</div>';
        html += '<div class="result-diff"><div class="result-diff-col"><span class="result-diff-label">期望输出</span><pre class="result-code">' + escapeHtml(r.expected_output || '') + '</pre></div>';
        html += '<div class="result-diff-col"><span class="result-diff-label">实际输出</span><pre class="result-code">' + escapeHtml(r.actual_output || '') + '</pre></div></div></div>';
      }

      if (r.time_ms > 0) {
        html += '<div class="result-meta"><span class="result-meta-item"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg> ' + r.time_ms + 'ms</span></div>';
      }

      html += '</div>';
      resultEl.innerHTML = html;
    })
    .catch(function(e) {
      resultEl.innerHTML = '<div class="result-card result-error"><div class="result-msg">错误: ' + escapeHtml(e.message) + '</div></div>';
    })
    .finally(function() {
      if (submitBtn) {
        submitBtn.disabled = false;
        submitBtn.classList.remove('btn-loading');
      }
    });
}

function escapeHtml(str) {
  var div = document.createElement('div');
  div.appendChild(document.createTextNode(str));
  return div.innerHTML;
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

document.addEventListener('DOMContentLoaded', function() {
  initCodeEditor();
  var ta = document.getElementById('code-area');
  if (ta && ta.value.trim()) {
    _ojTemplateCode = ta.value;
  }
});
