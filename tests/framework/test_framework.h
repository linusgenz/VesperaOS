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
        std::string display_name;
        std::function<void()> fn;
    };

    inline std::vector<TestEntry>& registry() {
        static std::vector<TestEntry> r;
        return r;
    }

    inline void register_test(const char* suite, const char* display_name, std::function<void()> fn) {
        registry().push_back({suite, display_name, std::move(fn)});
    }

    inline int run_all_tests(const char* filter_suite = nullptr) {
        int passed = 0, failed = 0, skipped = 0;
        std::string last_suite;

        for (auto& t : registry()) {
            if (filter_suite && t.suite != filter_suite) {
                skipped++;
                continue;
            }

            if (t.suite != last_suite) {
                printf("\n" TF_BOLD TF_CYAN "[ %s ]" TF_RESET "\n", t.suite.c_str());
                last_suite = t.suite;
            }

            current_test_failed = false;
            current_failure_msg[0] = '\0';

            t.fn();

            if (!current_test_failed) {
                printf("  " TF_GREEN "PASS" TF_RESET "  %s\n", t.display_name.c_str());
                passed++;
            } else {
                printf("  " TF_RED "FAIL" TF_RESET "  %s\n%s\n", t.display_name.c_str(), current_failure_msg);
                failed++;
            }
        }

        printf(
            "\n" TF_BOLD
            "-------------------------------------------\n"
            "  Results: " TF_GREEN "%d passed" TF_RESET "  " TF_RED "%d failed" TF_RESET "  %d skipped\n" TF_BOLD
            "-------------------------------------------\n" TF_RESET,
            passed,
            failed,
            skipped
        );

        return failed > 0 ? 1 : 0;
    }

}  // namespace TestFramework

#define TEST(suite_name, test_id, description)                                                     \
    static void _test_fn_##suite_name##_##test_id();                                               \
    static bool _test_reg_##suite_name##_##test_id = [] {                                          \
        TestFramework::register_test(#suite_name, description, _test_fn_##suite_name##_##test_id); \
        return true;                                                                               \
    }();                                                                                           \
    static void _test_fn_##suite_name##_##test_id()

#define TEST_MAIN()                                          \
    int main(int argc, char** argv) {                        \
        const char* filter = (argc > 1) ? argv[1] : nullptr; \
        return TestFramework::run_all_tests(filter);         \
    }

#endif  // VESPERAOS_TEST_FRAMEWORK_H
