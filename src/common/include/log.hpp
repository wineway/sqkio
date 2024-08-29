#ifndef SQK_LOG_HPP
#define SQK_LOG_HPP

#ifdef HAS_SPDLOG
    #include <spdlog/common.h>
    #undef SPDLOG_ACTIVE_LEVEL
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
    #include "spdlog/cfg/env.h"
    #include "spdlog/spdlog.h"
    #define S_INFO(...)    SPDLOG_INFO(__VA_ARGS__)
    #define S_DBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
    #define S_WARN(...)    SPDLOG_WARN(__VA_ARGS__)
    #define S_ERROR(...)   SPDLOG_ERROR(__VA_ARGS__)
    #define S_LOGGER_SETUP spdlog::cfg::load_env_levels()
#else
    #define S_INFO(...)
    #define S_DBUG(...)
    #define S_WARN(...)
    #define S_ERROR(...)
    #define S_LOGGER_SETUP
#endif
#define S_ASSERT(e) assert(e)
#endif // SQK_LOG_HPP
