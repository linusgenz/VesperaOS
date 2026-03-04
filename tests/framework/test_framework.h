// test_framework.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.03.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
#ifndef VESPERAOS_TEST_FRAMEWORK_H
#define VESPERAOS_TEST_FRAMEWORK_H

#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#define TF_RED "\x1b[31m"
#define TF_GREEN "\x1b[32m"
#define TF_CYAN "\x1b[36m"
#define TF_RESET "\x1b[0m"
#define TF_BOLD "\x1b[1m"

#define ASSERT_TRUE(expr)                                                             \
    do {                                                                              \
        if (!(expr)) {                                                                \
            ::TestFramework::fail_test(__FILE__, __LINE__, "ASSERT_TRUE(" #expr ")"); \
            return;                                                                   \
        }                                                                             \
    } while (0)

#define ASSERT_FALSE(expr)                                                             \
    do {                                                                               \
        if ((expr)) {                                                                  \
            ::TestFramework::fail_test(__FILE__, __LINE__, "ASSERT_FALSE(" #expr ")"); \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define ASSERT_EQ(expected, actual)                                                      \
    do {                                                                                 \
        auto _e = (expected);                                                            \
        auto _a = (actual);                                                              \
        if (!(_e == _a)) {                                                               \
            char _buf[256];                                                              \
            snprintf(                                                                    \
                _buf,                                                                    \
                sizeof(_buf),                                                            \
                "ASSERT_EQ failed: expected=%s, got=%s  [" #expected " == " #actual "]", \
                ::TestFramework::to_str(_e).c_str(),                                     \
                ::TestFramework::to_str(_a).c_str()                                      \
            );                                                                           \
            ::TestFramework::fail_test(__FILE__, __LINE__, _buf);                        \
            return;                                                                      \
        }                                                                                \
    } while (0)

#define ASSERT_NE(a, b)                                                                        \
    do {                                                                                       \
        if ((a) == (b)) {                                                                      \
            ::TestFramework::fail_test(__FILE__, __LINE__, "ASSERT_NE failed: " #a " != " #b); \
            return;                                                                            \
        }                                                                                      \
    } while (0)

#define ASSERT_NULL(ptr)                                                                                \
    do {                                                                                                \
        if ((ptr) != nullptr) {                                                                         \
            ::TestFramework::fail_test(__FILE__, __LINE__, "ASSERT_NULL failed: " #ptr " is not null"); \
            return;                                                                                     \
        }                                                                                               \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                                                            \
    do {                                                                                                \
        if ((ptr) == nullptr) {                                                                         \
            ::TestFramework::fail_test(__FILE__, __LINE__, "ASSERT_NOT_NULL failed: " #ptr " is null"); \
            return;                                                                                     \
        }                                                                                               \
    } while (0)

#define ASSERT_MEM_EQ(expected_ptr, actual_ptr, size)                                                       \
    do {                                                                                                    \
        if (memcmp((expected_ptr), (actual_ptr), (size)) != 0) {                                            \
            ::TestFramework::fail_test(__FILE__, __LINE__, "ASSERT_MEM_EQ failed: memory contents differ"); \
            return;                                                                                         \
        }                                                                                                   \
    } while (0)

#define ASSERT_STR_EQ(expected, actual)                                                                              \
    do {                                                                                                             \
        if (strcmp((expected), (actual)) != 0) {                                                                     \
            char _buf[256];                                                                                          \
            snprintf(_buf, sizeof(_buf), "ASSERT_STR_EQ failed: expected=\"%s\", got=\"%s\"", (expected), (actual)); \
            ::TestFramework::fail_test(__FILE__, __LINE__, _buf);                                                    \
            return;                                                                                                  \
        }                                                                                                            \
    } while (0)

#define ASSERT_GE(a, b)                                                      \
    do {                                                                     \
        if (!((a) >= (b))) {                                                 \
            char _buf[128];                                                  \
            snprintf(_buf, sizeof(_buf), "ASSERT_GE failed: " #a " >= " #b); \
            ::TestFramework::fail_test(__FILE__, __LINE__, _buf);            \
            return;                                                          \
        }                                                                    \
    } while (0)

#define ASSERT_LE(a, b)                                                      \
    do {                                                                     \
        if (!((a) <= (b))) {                                                 \
            char _buf[128];                                                  \
            snprintf(_buf, sizeof(_buf), "ASSERT_LE failed: " #a " <= " #b); \
            ::TestFramework::fail_test(__FILE__, __LINE__, _buf);            \
            return;                                                          \
        }                                                                    \
    } while (0)

extern jmp_buf g_panic_jmp;
extern bool g_panic_armed;
extern bool g_panic_fired;
extern char g_panic_msg[256];
extern int32_t g_panic_code;

#define ASSERT_PANICS(code_block)                                                                    \
    do {                                                                                             \
        g_panic_armed = true;                                                                        \
        g_panic_fired = false;                                                                       \
        g_panic_msg[0] = '\0';                                                                       \
        g_panic_code = 0;                                                                            \
        if (setjmp(g_panic_jmp) == 0) {                                                              \
            code_block;                                                                              \
        }                                                                                            \
        g_panic_armed = false;                                                                       \
        if (!g_panic_fired) {                                                                        \
            ::TestFramework::fail_test(__FILE__, __LINE__, "ASSERT_PANICS: no panic was triggered"); \
            return;                                                                                  \
        }                                                                                            \
    } while (0)

#define ASSERT_NO_PANIC(code_block)                                                                       \
    do {                                                                                                  \
        g_panic_armed = true;                                                                             \
        g_panic_fired = false;                                                                            \
        if (setjmp(g_panic_jmp) == 0) {                                                                   \
            code_block;                                                                                   \
        }                                                                                                 \
        g_panic_armed = false;                                                                            \
        if (g_panic_fired) {                                                                              \
            ::TestFramework::fail_test(__FILE__, __LINE__, "ASSERT_NO_PANIC: unexpected panic occurred"); \
            return;                                                                                       \
        }                                                                                                 \
    } while (0)

namespace TestFramework {

    inline bool current_test_failed = false;
    inline char current_failure_msg[512] = {};

    inline void fail_test(const char* file, int line, const char* msg) {
        current_test_failed = true;
        snprintf(current_failure_msg, sizeof(current_failure_msg), "    %s:%d  ->  %s", file, line, msg);
    }

    template <typename T>
    inline std::string to_str(T v) {
        return std::to_string(v);
    }
    inline std::string to_str(const char* s) {
        return s ? s : "(null)";
    }
    inline std::string to_str(std::string s) {
        return s;
    }
    inline std::string to_str(bool b) {
        return b ? "true" : "false";
    }
    inline std::string to_str(void* p) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%p", p);
        return buf;
    }

    // --- Test entry ---
    struct TestEntry {
        std::string suite;
        std::string test_id;
        std::string display_name;
        std::function<void()> fn;
    };

    inline std::vector<TestEntry>& registry() {
        static std::vector<TestEntry> r;
        return r;
    }

    inline void register_test(
        const char* suite, const char* test_id, const char* display_name, std::function<void()> fn
    ) {
        registry().push_back({suite, test_id, display_name, std::move(fn)});
    }

    inline int run_all_tests(const char* filter_suite = nullptr, const char* filter_id = nullptr) {
        int failed = 1;
        bool any_matched = false;

        for (auto& t : registry()) {
            if (filter_suite && t.suite != filter_suite) continue;
            if (filter_id && t.test_id != filter_id) continue;

            any_matched = true;

            current_test_failed = false;
            current_failure_msg[0] = '\0';

            t.fn();

            if (current_test_failed) {
                printf("  " TF_RED "FAIL" TF_RESET "  %s\n%s\n", t.display_name.c_str(), current_failure_msg);
            } else {
                printf("  " TF_GREEN "PASS" TF_RESET "  %s\n", t.display_name.c_str());
                failed = 0;
            }
        }

        if ((filter_suite || filter_id) && !any_matched) {
            printf(
                "  " TF_CYAN "SKIP" TF_RESET "  Registered test does not exist (suite='%s', test='%s')\n",
                filter_suite ? filter_suite : "<any>",
                filter_id ? filter_id : "<any>"
            );
            failed = 1;
        }

        return failed;
    }

}  // namespace TestFramework

#define TEST(suite_name, test_id, description)                                                               \
    static void _test_fn_##suite_name##_##test_id();                                                         \
    static bool _test_reg_##suite_name##_##test_id = [] {                                                    \
        TestFramework::register_test(#suite_name, #test_id, description, _test_fn_##suite_name##_##test_id); \
        return true;                                                                                         \
    }();                                                                                                     \
    static void _test_fn_##suite_name##_##test_id()

#define TEST_MAIN()                                                   \
    int main(int argc, char** argv) {                                 \
        const char* filter_suite = (argc > 1) ? argv[1] : nullptr;    \
        const char* filter_id = (argc > 2) ? argv[2] : nullptr;       \
        return TestFramework::run_all_tests(filter_suite, filter_id); \
    }

#endif  // VESPERAOS_TEST_FRAMEWORK_H
