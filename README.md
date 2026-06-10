# Vibe OJ

A lightweight C++ Online Judge for small teams (3-20 people).

## Features

- C++ (g++) code judging with stdin/stdout comparison
- Sandboxed execution (rlimit + seccomp + chroot)
- bcrypt password hashing
- Admin panel for problem & test case management
- Server-side HTML rendering with `{{PLACEHOLDER}}` templates

## Quick Start

### Prerequisites

```bash
sudo apt install g++ libmysqlclient-dev libseccomp-dev nlohmann-json3-dev libgtest-dev
```

### Database

```sql
CREATE DATABASE vibe_oj CHARACTER SET utf8mb4;

CREATE TABLE problems (
  id INT PRIMARY KEY AUTO_INCREMENT,
  title VARCHAR(255) NOT NULL,
  difficulty ENUM('Easy','Medium','Hard') NOT NULL,
  content TEXT NOT NULL,
  template TEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_cases (
  id INT PRIMARY KEY AUTO_INCREMENT,
  problem_id INT NOT NULL,
  input TEXT NOT NULL,
  expected TEXT NOT NULL,
  position INT NOT NULL DEFAULT 0,
  FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);

CREATE TABLE users (
  id INT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(64) UNIQUE NOT NULL,
  password VARCHAR(128) NOT NULL,
  role ENUM('user','admin') DEFAULT 'user',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

MySQL root user: no password (configurable in `config.h`).

### Build & Run

```bash
make           # compile server
./server       # start on port 8080
```

Open http://localhost:8080 in a browser.

### Run Tests

```bash
make test                    # C++ unit tests (83 tests)
python3 tests/test_api.py    # API integration tests (32 tests, requires server running)
```

## Project Structure

```
vibe-oj/
├── server.cc              # Main entry point, route registration
├── config.h               # DB connection, judge limits
├── db.h/db.cc             # MySQL CRUD wrapper
├── render.h/render.cc     # HTML template loading & {{KEY}} replacement
├── md.h/md.cc             # Markdown to HTML converter
├── auth.h/auth.cc         # Session mgmt, bcrypt, auth middleware
├── judge.h/judge.cc       # Judge engine (compile + sandbox execution)
├── log.h/log.cc           # Logging system
├── static/
│   ├── style.css          # Global stylesheet
│   └── app.js             # Frontend fetch API, DOM operations
├── templates/             # HTML templates with {{PLACEHOLDER}} variables
│   ├── _base.html         # Base layout (nav + main + footer)
│   ├── landing.html       # Homepage
│   ├── login.html / register.html
│   ├── problem_list.html / problem_detail.html
│   └── admin_panel.html / admin_problem_form.html / admin_testcases.html / admin_users.html
├── tests/                 # Unit tests (Google Test) + API tests (Python)
├── Makefile
└── SPEC.md
```

## API

See [API.md](API.md) and [curl测试.md](curl测试.md).

## Security

- Passwords: bcrypt hashed (`<crypt.h>`)
- Execution: fork + rlimit + seccomp whitelist + chroot to `/tmp/vibe-oj/`
- Sessions: random hex tokens stored in memory, HttpOnly cookies
- Admin: role-based middleware (`CHECK_ADMIN` / `CHECK_ADMIN_JSON`)

## License

MIT
