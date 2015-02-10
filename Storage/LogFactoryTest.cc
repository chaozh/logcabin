/* Copyright (c) 2014 Stanford University
 * Copyright (c) 2015 Diego Ongaro
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>

#include "Core/Debug.h"
#include "Core/StringUtil.h"
#include "Storage/LogFactory.h"
#include "Storage/MemoryLog.h"
#include "Storage/SegmentedLog.h"
#include "Storage/SimpleFileLog.h"

namespace LogCabin {
namespace Storage {

namespace {

class StorageLogFactoryTest : public ::testing::Test {
  public:
    StorageLogFactoryTest()
        : tmpdir()
        , config()
    {
        std::string path = FilesystemUtil::mkdtemp();
        tmpdir = FilesystemUtil::File(open(path.c_str(), O_RDONLY|O_DIRECTORY),
                                      path);
    }
    ~StorageLogFactoryTest() {
        FilesystemUtil::remove(tmpdir.path);
    }
    FilesystemUtil::File tmpdir;
    Core::Config config;
};

TEST_F(StorageLogFactoryTest, makeLog_Memory)
{
    config.set("storageModule", "Memory");
    std::unique_ptr<Log> log = LogFactory::makeLog(config, tmpdir);
    EXPECT_STREQ(typeid(MemoryLog).name(), typeid(*log).name());
}

TEST_F(StorageLogFactoryTest, makeLog_SimpleFile)
{
    // expect warning
    Core::Debug::setLogPolicy({
        {"Storage/SimpleFileLog.cc", "ERROR"}
    });

    // default
    std::unique_ptr<Log> log = LogFactory::makeLog(config, tmpdir);
    EXPECT_STREQ(typeid(SimpleFileLog).name(), typeid(*log).name());
    log.reset();

    config.set("storageModule", "SimpleFile");
    log = LogFactory::makeLog(config, tmpdir);
    EXPECT_STREQ(typeid(SimpleFileLog).name(), typeid(*log).name());
}

TEST_F(StorageLogFactoryTest, makeLog_Segmented_Binary)
{
    // expect warning
    Core::Debug::setLogPolicy({
        {"Storage/SegmentedLog.cc", "ERROR"}
    });

    config.set("storageModule", "Segmented");
    std::unique_ptr<Log> log = LogFactory::makeLog(config, tmpdir);
    EXPECT_STREQ(typeid(SegmentedLog).name(), typeid(*log).name());
    log.reset();

    config.set("storageModule", "Segmented-Binary");
    log = LogFactory::makeLog(config, tmpdir);
    EXPECT_STREQ(typeid(SegmentedLog).name(), typeid(*log).name());
}

TEST_F(StorageLogFactoryTest, makeLog_Segmented_Text)
{
    // expect warning
    Core::Debug::setLogPolicy({
        {"Storage/SegmentedLog.cc", "ERROR"}
    });

    config.set("storageModule", "Segmented-Text");
    std::unique_ptr<Log> log = LogFactory::makeLog(config, tmpdir);
    EXPECT_STREQ(typeid(SegmentedLog).name(), typeid(*log).name());
}

TEST_F(StorageLogFactoryTest, makeLog_notfound)
{
    config.set("storageModule", "punchcard");
    EXPECT_DEATH(LogFactory::makeLog(config, tmpdir),
                 "Unknown storage module");
}

} // namespace LogCabin::Storage::<anonymous>
} // namespace LogCabin::Storage
} // namespace LogCabin
