#include "trace_sink.h"

#include <unistd.h>
#include <fcntl.h>

#include <cstring>
#include <iostream>
#include <vector>
#include <sstream>

#include <sys/syscall.h>
#include <tbox/base/log.h>
#include <tbox/base/defines.h>
#include <tbox/util/fs.h>
#include <tbox/util/scalable_integer.h>

namespace tbox {
namespace trace {

#define ENDLINE "\r\n"

TrackSink& TrackSink::GetInstance()
{
    static TrackSink instance;
    return instance;
}

TrackSink::TrackSink()
{
    using namespace std::placeholders;
    async_pipe_.setCallback(std::bind(&TrackSink::onBackendRecvData, this, _1, _2));
}

TrackSink::~TrackSink()
{
    disable();
}

void TrackSink::setPathPrefix(const std::string &path_prefix)
{
    path_ = path_prefix + '.' + std::to_string(::getpid());
    name_list_filename_ = path_ + "/name_list.txt";
    thread_list_filename_ = path_ + "/thread_list.txt";
}

bool TrackSink::enable()
{
    if (is_enabled_)
        return true;

    if (path_.empty()) {
        std::cerr << "Warn: no path_prefix" << std::endl;
        return false;
    }

    util::AsyncPipe::Config config;
    async_pipe_.initialize(config);
    is_enabled_ = true;

    return true;
}

void TrackSink::disable()
{
    if (is_enabled_) {
        is_enabled_ = false;
        async_pipe_.cleanup();
        CHECK_CLOSE_RESET_FD(curr_record_fd_);
    }
}

void TrackSink::commitRecord(const char *name, uint64_t end_timepoint_us, uint64_t duration_us)
{
    if (!is_enabled_)
        return;

    RecordHeader header = {
        .thread_id = syscall(SYS_gettid),
        .end_ts_us = end_timepoint_us,
        .duration_us = duration_us,
        .name_size = ::strlen(name) + 1
    };
    async_pipe_.append(&header, sizeof(header));
    async_pipe_.append(name, header.name_size);
}

void TrackSink::onBackendRecvData(const void *data, size_t size)
{
    buffer_.append(data, size);

    while (buffer_.readableSize() > sizeof(RecordHeader)) {
        RecordHeader header;
        ::memcpy(&header, buffer_.readableBegin(), sizeof(header));
        //! Q: 为什么不直接引用，像这样 auto *phead = static_cast<const RecordHeader*>(buffer_.readableBegin())
        //     而是要先copy到header再使用？
        //! A: 因为直接指针引用会有对齐的问题。

        //! 如果长度不够，就跳过，等下一次
        if ((header.name_size + sizeof(header)) > buffer_.readableSize())
            break;

        buffer_.hasRead(sizeof(header));

        onBackendRecvRecord(header, reinterpret_cast<const char*>(buffer_.readableBegin()));
        buffer_.hasRead(header.name_size);
    }
}

void TrackSink::onBackendRecvRecord(const RecordHeader &record, const char *name)
{
    if (!checkAndCreateRecordFile())
        return;

    auto thread_index = allocThreadIndex(record.thread_id);
    auto name_index = allocNameIndex(name);
    auto time_diff = record.end_ts_us - last_timepoint_us_;

    constexpr size_t kBufferSize = 40;
    uint8_t buffer[kBufferSize];
    size_t  data_size = 0;

    data_size += util::DumpScalableInteger(time_diff, (buffer + data_size), (kBufferSize - data_size));
    data_size += util::DumpScalableInteger(record.duration_us, (buffer + data_size), (kBufferSize - data_size));
    data_size += util::DumpScalableInteger(thread_index, (buffer + data_size), (kBufferSize - data_size));
    data_size += util::DumpScalableInteger(name_index, (buffer + data_size), (kBufferSize - data_size));

    auto wsize = ::write(curr_record_fd_, buffer, data_size);
    if (wsize != static_cast<ssize_t>(data_size)) {
        std::cerr << "Err: write file error" << std::endl;
        return;
    }

    last_timepoint_us_ = record.end_ts_us;
    total_write_size_ += wsize;

    if (total_write_size_ >= record_file_max_size_)
        CHECK_CLOSE_RESET_FD(curr_record_fd_);
}

bool TrackSink::checkAndCreateRecordFile()
{
    if (curr_record_fd_ >= 0) {
        if (util::fs::IsFileExist(curr_record_filename_))
            return true;
        CHECK_CLOSE_RESET_FD(curr_record_fd_);
    }

    std::string records_path = path_ + "/records";

    //!检查并创建路径
    if (!util::fs::MakeDirectory(records_path, false)) {
        std::cerr << "Err: create director " << records_path << " fail. error:" << errno << ',' << strerror(errno) << std::endl;
        return false;
    }

    char timestamp[16]; //! 固定长度16B，"20220414_071023"
    {
        time_t ts_sec = time(nullptr);
        struct tm tm;
        localtime_r(&ts_sec, &tm);
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);
    }

    std::string new_record_filename;
    int postfix = 0;
    do {
        new_record_filename = records_path + '/' + timestamp + ".bin";
        if (postfix != 0) {
            new_record_filename += '.';
            new_record_filename += std::to_string(postfix);
        }
        ++postfix;
    } while (util::fs::IsFileExist(new_record_filename));    //! 避免在同一秒多次创建日志文件，都指向同一日志名
    curr_record_filename_ = std::move(new_record_filename);

    int flags = O_CREAT | O_WRONLY | O_APPEND | O_DSYNC;

    curr_record_fd_ = ::open(curr_record_filename_.c_str(), flags, S_IRUSR | S_IWUSR);
    if (curr_record_fd_ < 0) {
        std::cerr << "Err: open file " << curr_record_filename_ << " fail. error:" << errno << ',' << strerror(errno) << std::endl;
        return false;
    }

    total_write_size_ = 0;
    last_timepoint_us_ = 0;
    return true;
}

TrackSink::Index TrackSink::allocNameIndex(const std::string &name)
{
    auto iter = name_to_index_map_.find(name);
    if (iter != name_to_index_map_.end())
        return iter->second;

    auto new_index = next_name_index_++;
    name_to_index_map_[name] = new_index;

    if (util::fs::IsFileExist(name_list_filename_)) {
        util::fs::AppendStringToTextFile(name_list_filename_, name + ENDLINE);

    } else {
        //! 文件不存在了，则重写所有的名称列表
        std::vector<std::string> name_vec;
        name_vec.resize(name_to_index_map_.size());
        for (auto &item : name_to_index_map_)
            name_vec[item.second] = item.first;

        std::ostringstream oss;
        for (auto &name : name_vec)
            oss << name << ENDLINE;

        util::fs::WriteStringToTextFile(name_list_filename_, oss.str());
    }

    return new_index;
}

TrackSink::Index TrackSink::allocThreadIndex(long thread_id)
{
    auto iter = thread_to_index_map_.find(thread_id);
    if (iter != thread_to_index_map_.end())
        return iter->second;

    auto new_index = next_thread_index_++;
    thread_to_index_map_[thread_id] = new_index;

    if (util::fs::IsFileExist(thread_list_filename_)) {
        util::fs::AppendStringToTextFile(thread_list_filename_, std::to_string(thread_id) + ENDLINE);

    } else {
        //! 文件不存在了，则重写所有的线程列表
        std::vector<int> thread_vec;
        thread_vec.resize(thread_to_index_map_.size());
        for (auto &item : thread_to_index_map_)
            thread_vec[item.second] = item.first;

        std::ostringstream oss;
        for (auto thread_id : thread_vec)
            oss << thread_id << ENDLINE;

        util::fs::WriteStringToTextFile(thread_list_filename_, oss.str());
    }

    return new_index;
}

}
}
