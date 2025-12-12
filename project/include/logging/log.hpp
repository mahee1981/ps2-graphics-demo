#ifndef LOG_HPP
#define LOG_HPP
#include <sstream>
#include <unordered_map>

namespace Log
{

enum class Severity
{
    INFO = 0,
    WARNING = 1,
    ERROR = 2,
    FATAL = 3,
};

class LogMessage : public std::ostringstream
{
  public:
    LogMessage(Severity severity, const char *fileName, const char *funcName, int line);
    ~LogMessage();

  protected:
    void printLogMessage();

  private:
    Severity severity_;
    const char *fileName_;
    const char *funcName_;
    int line_;
    static std::unordered_map<Severity, const char *> SeverityStr;
};

class LogMessageFatal : public LogMessage
{
  public:
    LogMessageFatal(const char *fileName, const char *funcName, int line);
    ~LogMessageFatal();
};

} // namespace Log
#define LOG_INFO(msg) Log::LogMessage(Log::Severity::INFO, __FILE__, __FUNCTION__, __LINE__).flush() << msg
#define LOG_WARNING(msg) Log::LogMessage(Log::Severity::WARNING, __FILE__, __FUNCTION__, __LINE__).flush() << msg
#define LOG_ERROR(msg) Log::LogMessage(Log::Severity::ERROR, __FILE__, __FUNCTION__, __LINE__).flush() << msg
#define LOG_FATAL(msg) Log::LogMessageFatal(__FILE__, __FUNCTION__, __LINE__).flush() << msg

#define CHECK(expr)                                                                                                    \
    if (!(expr))                                                                                                       \
    Log::LogMessageFatal(__FILE__, __FUNCTION__, __LINE__).flush() << "Check failed: " << #expr << " "

#define CHECK_EQ(val1, val2) CHECK(val1 == val2)
#define CHECK_NE(val1, val2) CHECK(val1 != val2)
#define CHECK_LE(val1, val2) CHECK(val1 <= val2)
#define CHECK_LT(val1, val2) CHECK(val1 < val2)
#define CHECK_GE(val1, val2) CHECK(val1 >= val2)
#define CHECK_GT(val1, val2) CHECK(val1 > val2)
#define CHECK_NOT_NULL(val) CHECK_NE(val, nullptr)
#define CHECK_POW2(val) CHECK((val != 0) && ((val & (val - 1)) == 0))

#ifndef NDEBUG

#define DCHECK(expr) CHECK(expr)
#define DCHECK_EQ(val1, val2) DCHECK(val1 == val2)
#define DCHECK_NE(val1, val2) DCHECK(val1 != val2)
#define DCHECK_LE(val1, val2) DCHECK(val1 <= val2)
#define DCHECK_LT(val1, val2) DCHECK(val1 < val2)
#define DCHECK_GE(val1, val2) DCHECK(val1 >= val2)
#define DCHECK_GT(val1, val2) DCHECK(val1 > val2)
#define DCHECK_NOT_NULL(val) DCHECK_NE(val, nullptr)
#define DCHECK_POW2(val) DCHECK((val != 0) && ((val & (val - 1)) == 0))

#else

#define DUD_STREAM                                                                                                     \
    if (false)                                                                                                         \
    LOG(FATAL)

#define DCHECK(expr) DUD_STREAM
#define DCHECK_EQ(val1, val2) DUD_STREAM
#define DCHECK_NE(val1, val2) DUD_STREAM
#define DCHECK_LE(val1, val2) DUD_STREAM
#define DCHECK_LT(val1, val2) DUD_STREAM
#define DCHECK_GE(val1, val2) DUD_STREAM
#define DCHECK_GT(val1, val2) DUD_STREAM
#define DCHECK_NOT_NULL(val) DUD_STREAM
#define DCHECK_POW2(val) DUD_STREAM

#endif

#endif
