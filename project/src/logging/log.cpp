#include "logging/log.hpp"
#include <stdio.h>

namespace Log
{

std::unordered_map<Severity, const char *> LogMessage::SeverityStr{{Severity::INFO, "INFO"},
                                                                   {Severity::WARNING, "WARNING"},
                                                                   {Severity::ERROR, "ERROR"},
                                                                   {Severity::FATAL, "FATAL"}};

LogMessage::LogMessage(Severity severity, const char *fileName, const char *funcName, int line)
    : severity_(severity), fileName_(fileName), funcName_(funcName), line_(line)
{
}

LogMessage::~LogMessage()
{
    printLogMessage();
}

void LogMessage::printLogMessage()
{
    fprintf(stdout,
            "---- %s ---- In file %s: function: %s line:%d '%s'\n",
            SeverityStr[severity_],
            fileName_,
            funcName_,
            line_,
            str().c_str());
}

LogMessageFatal::LogMessageFatal(const char *fileName, const char *funcName, int line)
    : LogMessage(Severity::FATAL, fileName, funcName, line)
{
}

LogMessageFatal::~LogMessageFatal()
{
    printLogMessage();
    abort();
}

} // namespace Log
