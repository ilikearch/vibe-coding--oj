# Vibe OJ 部署指南

## 环境要求

- Ubuntu 20.04+ / Debian 11+
- g++ (支持 C++17)
- MySQL 8.0+
- Python 3.8+（仅 API 测试需要）

## 1. 安装依赖

```bash
sudo apt update
sudo apt install g++ libmysqlclient-dev libseccomp-dev nlohmann-json3-dev libgtest-dev
```

## 2. 初始化数据库

登录 MySQL：

```bash
sudo mysql -u root
```

执行建表：

```sql
CREATE DATABASE vibe_oj CHARACTER SET utf8mb4;
USE vibe_oj;

CREATE TABLE problems (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  title      VARCHAR(255) NOT NULL,
  difficulty ENUM('Easy','Medium','Hard') NOT NULL,
  content    TEXT NOT NULL,
  template   TEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_cases (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  problem_id INT NOT NULL,
  input      TEXT NOT NULL,
  expected   TEXT NOT NULL,
  position   INT NOT NULL DEFAULT 0,
  FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);

CREATE TABLE users (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  username   VARCHAR(64) UNIQUE NOT NULL,
  password   VARCHAR(128) NOT NULL,
  role       ENUM('user','admin') DEFAULT 'user',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

> 默认连接 `root` 用户无密码。如需修改，编辑 `config.h` 中的 `DB_USER` / `DB_PASS`。

## 3. 插入种子题目

```sql
USE vibe_oj;

INSERT INTO problems (title, difficulty, content, template) VALUES
('A+B Problem', 'Easy',
'## Description\n\nGiven two integers **A** and **B**, output their sum.\n\n## Input\n\nTwo space-separated integers A and B.\n\n## Output\n\nA single integer: A + B.\n\n## Example\n\n**Input:** 2 3\n**Output:** 5',
'#include <iostream>\nint main(){\n  int a,b;std::cin>>a>>b;std::cout<<a+b<<std::endl;return 0;\n}');

INSERT INTO problems (title, difficulty, content, template) VALUES
('Reverse String', 'Medium',
'## Description\n\nRead a string and output it reversed.\n\n## Input\n\nA single line (max 1000 chars).\n\n## Output\n\nThe reversed string.\n\n## Example\n\n**Input:** hello\n**Output:** olleh',
'#include <iostream>\n#include <string>\n#include <algorithm>\nint main(){\n  std::string s;std::getline(std::cin,s);\n  std::reverse(s.begin(),s.end());\n  std::cout<<s<<std::endl;return 0;\n}');

INSERT INTO test_cases (problem_id, input, expected, position) VALUES
(1, '2 3', '5', 0),
(1, '-10 20', '10', 1),
(1, '0 0', '0', 2),
(2, 'hello', 'olleh', 0),
(2, 'racecar', 'racecar', 1),
(2, 'a b c', 'c b a', 2);
```

## 4. 编译

```bash
cd vibe-oj/
make
```

## 5. 启动

```bash
./server
```

启动后访问 `http://<服务器IP>:8080`。

## 6. 运行测试

```bash
make test                        # C++ 单元测试（83 项）
python3 tests/test_api.py        # API 集成测试（32 项，需先启动 server）
```

> 注意：`make test` 会清空数据库测试数据。测试后需重新插入种子题目。

## 7. 管理后台

注册用户默认为普通用户。设置为管理员：

```sql
UPDATE users SET role='admin' WHERE username='你的用户名';
```

管理员可访问 `/admin` 进行题目和用例管理。

## 判题安全配置

判题参数在 `config.h` 中可调整：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `JUDGE_CPU_TIMEOUT_SEC` | 1s | CPU 时间限制 |
| `JUDGE_REAL_TIMEOUT_MS` | 2000ms | 真实时间限制 |
| `JUDGE_MEMORY_LIMIT_MB` | 128MB | 内存限制 |
| `JUDGE_TEMP_DIR` | `/tmp/vibe-oj/` | 沙箱临时目录 |
