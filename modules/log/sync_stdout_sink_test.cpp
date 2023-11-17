/*
 *     .============.
 *    //  M A K E  / \
 *   //  C++ DEV  /   \
 *  //  E A S Y  /  \/ \
 * ++ ----------.  \/\  .
 *  \\     \     \ /\  /
 *   \\     \     \   /
 *    \\     \     \ /
 *     -============'
 *
 * Copyright (c) 2023 Hevake and contributors, all rights reserved.
 *
 * This file is part of cpp-tbox (https://github.com/cpp-main/cpp-tbox)
 * Use of this source code is governed by MIT license that can be found
 * in the LICENSE file in the root of the source tree. All contributing
 * project authors may be found in the CONTRIBUTORS.md file in the root
 * of the source tree.
 */
#include <iostream>
#include <chrono>
#include <gtest/gtest.h>

#include "sync_stdout_sink.h"

using namespace std;
using namespace tbox::log;

TEST(SyncStdoutSink, DefaultLevel)
{
    SyncStdoutSink ch;
    ch.enable();
    cout << "Should print INFO level" << endl;

    LogFatal("fatal");
    LogErr("err");
    LogWarn("warn");
    LogNotice("notice");
    LogInfo("info");
    LogDbg("debug");
    LogTrace("trace");
    LogUndo();
    LogTag();
}

TEST(SyncStdoutSink, TraceLevel)
{
    SyncStdoutSink ch;
    ch.enable();
    ch.setLevel("test.log", LOG_LEVEL_TRACE);
    cout << "Should print all level" << endl;

    LogFatal("fatal");
    LogErr("err");
    LogWarn("warn");
    LogNotice("notice");
    LogInfo("info");
    LogDbg("debug");
    LogTrace("trace");
    LogUndo();
    LogTag();
}

TEST(SyncStdoutSink, AllLevel)
{
    SyncStdoutSink ch;

    ch.enable();
    ch.enableColor(true);
    ch.setLevel("", LOG_LEVEL_TRACE);

    LogFatal("fatal");
    LogErr("err");
    LogWarn("warn");
    LogNotice("notice");
    LogImportant("important");
    LogInfo("info");
    LogDbg("debug");
    LogTrace("trace");
    LogUndo();
    LogTag();
}

TEST(SyncStdoutSink, NullString)
{
    SyncStdoutSink ch;
    ch.enable();

    LogInfo(nullptr);
    LogPuts(LOG_LEVEL_INFO, nullptr);
}

TEST(SyncStdoutSink, WillNotPrint)
{
    SyncStdoutSink ch;
    cout << "Should not print" << endl;

    LogFatal("fatal");
    LogErr("err");
    LogWarn("warn");
    LogNotice("notice");
    LogInfo("info");
    LogDbg("debug");
    LogTrace("trace");
    LogUndo();
    LogTag();
}

TEST(SyncStdoutSink, EnableColor)
{
    SyncStdoutSink ch;
    ch.enable();
    ch.enableColor(true);
    ch.setLevel("test.log", LOG_LEVEL_TRACE);
    cout << "Should with color" << endl;

    LogFatal("fatal");
    LogErr("err");
    LogWarn("warn");
    LogNotice("notice");
    LogInfo("info");
    LogDbg("debug");
    LogTrace("trace");
    LogUndo();
    LogTag();
}

TEST(SyncStdoutSink, Format)
{
    SyncStdoutSink ch;
    ch.enable();

    LogInfo("%s, %d, %f", "hello", 123456, 12.345);
    LogInfo("%d, %f, %s", 123456, 12.345, "world");
}

TEST(SyncStdoutSink, LongString)
{
    SyncStdoutSink ch;
    ch.enable();
    std::string tmp(4096, 'x');
    LogInfo("%s", tmp.c_str());
}
