#ifndef STARSIGHT_LOG_HPP
#define STARSIGHT_LOG_HPP

#include "quill/Quill.h"

quill::Logger* GetQuillLogger();

#undef LOG_INFO
#define LOG_INFO(fmt, ...) QUILL_LOG_INFO(GetQuillLogger(), fmt, ##__VA_ARGS__)

#undef LOG_WARNING
#define LOG_WARNING(fmt, ...) QUILL_LOG_WARNING(GetQuillLogger(), fmt, ##__VA_ARGS__)

#undef LOG_DEBUG
#define LOG_DEBUG(fmt, ...) QUILL_LOG_DEBUG(GetQuillLogger(), fmt, ##__VA_ARGS__)

#undef LOG_ERROR
#define LOG_ERROR(fmt, ...) QUILL_LOG_ERROR(GetQuillLogger(), fmt, ##__VA_ARGS__)

#undef LOG_CRITICAL
#define LOG_CRITICAL(fmt, ...) QUILL_LOG_CRITICAL(GetQuillLogger(), fmt, ##__VA_ARGS__)

#endif //STARSIGHT_LOG_HPP
