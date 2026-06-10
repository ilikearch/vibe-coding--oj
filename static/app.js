function postJSON(url, data) {
  return fetch(url, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  }).then(r => {
    if (!r.ok) return r.json().then(j => { throw new Error(j.error || r.statusText); });
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

function submitCode(problemId) {
  var code = document.getElementById('code-area').value;
  var resultEl = document.getElementById('submit-result');
  resultEl.innerHTML = '<p>Judging...</p>';
  postJSON('/problem/' + problemId + '/submit', {code: code})
    .then(function(r) {
      var color = r.status === 'AC' ? 'green' : (r.status === 'CE' ? '#c0a000' : 'red');
      var html = '<h3>Result</h3><p style="color:' + color + ';font-size:1.2em;font-weight:bold">' + r.status + '</p>';
      if (r.status === 'CE') {
        html += '<pre>' + (r.compile_error || '') + '</pre>';
      } else if (r.status === 'WA') {
        html += '<p>Failed on test case #' + r.failed_case + '</p>';
        html += '<p><strong>Expected:</strong></p><pre>' + r.expected_output + '</pre>';
        html += '<p><strong>Actual:</strong></p><pre>' + r.actual_output + '</pre>';
      }
      if (r.time_ms > 0) html += '<p>Time: ' + r.time_ms + 'ms</p>';
      resultEl.innerHTML = html;
    })
    .catch(function(e) {
      resultEl.innerHTML = '<p style="color:red">Error: ' + e.message + '</p>';
    });
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
  if (!confirm('Delete this problem and all its test cases?')) return;
  postJSON('/admin/problems/' + id + '/delete', {})
    .then(function(r) {
      if (r.success) window.location.href = r.redirect;
      else alert(r.error);
    })
    .catch(function(e) { alert('Error: ' + e.message); });
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
  if (!confirm('Delete this test case?')) return;
  postJSON('/admin/testcases/' + caseId + '/delete', {problem_id: problemId})
    .then(function(r) {
      if (r.success) location.reload();
      else alert(r.error);
    })
    .catch(function(e) { alert('Error: ' + e.message); });
}
