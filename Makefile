CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I. -DCPPHTTPLIB_OPENSSL_SUPPORT -DCPPHTTPLIB_ZLIB_SUPPORT -DCPPHTTPLIB_BROTLI_SUPPORT

MYSQL_CFLAGS := $(shell mysql_config --cflags 2>/dev/null || pkg-config --cflags mysqlclient 2>/dev/null || echo "")
MYSQL_LIBS := $(shell mysql_config --libs 2>/dev/null || pkg-config --libs mysqlclient 2>/dev/null || echo "-lmysqlclient")

GTEST_LIBS = -lgtest -lgtest_main -lpthread

DEPS = deps/cpp-httplib/httplib.h deps/json.hpp deps/bcrypt/bcrypt.h

OBJS = db.o render.o md.o auth.o judge.o server.o
LIB_OBJS = db.o render.o md.o auth.o judge.o
TEST_OBJS = tests/test_config.o tests/test_render.o tests/test_md.o tests/test_db.o
# Phase 2 添加: tests/test_auth.o
# Phase 5 添加: tests/test_judge.o

TARGET = server
TEST_TARGET = tests/run_tests

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(MYSQL_LIBS) -lcpp-httplib -lseccomp -lpthread -lcrypt

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(LIB_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(MYSQL_LIBS) $(GTEST_LIBS)

tests/test_config.o: tests/test_config.cc config.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/test_render.o: tests/test_render.cc render.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/test_md.o: tests/test_md.cc md.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/test_db.o: tests/test_db.cc db.h config.h
	$(CXX) $(CXXFLAGS) $(MYSQL_CFLAGS) -c $< -o $@

tests/test_auth.o: tests/test_auth.cc auth.h config.h $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/test_judge.o: tests/test_judge.cc judge.h config.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

server.o: server.cc config.h db.h render.h md.h auth.h judge.h $(DEPS)
	$(CXX) $(CXXFLAGS) $(MYSQL_CFLAGS) -c $< -o $@

db.o: db.cc db.h config.h
	$(CXX) $(CXXFLAGS) $(MYSQL_CFLAGS) -c $< -o $@

render.o: render.cc render.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

md.o: md.cc md.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

auth.o: auth.cc auth.h config.h $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

judge.o: judge.cc judge.h config.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(MYSQL_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET)

run: $(TARGET)
	./$(TARGET)
