/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <imagine/fs/FS.hh>
#include <imagine/logger/SystemLogger.hh>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#ifdef __APPLE__
#include <asl.h>
#include <unistd.h>
#endif
#define LOGTAG "Logger"
#include <imagine/logger/logger.h>
#include <stdio.h>
import std;

using namespace IG;

static const bool bufferLogLineOutput = Config::envIsAndroid || Config::envIsIOS;
static char logLineBuffer[512]{};
static uint8_t loggerVerbosity = loggerMaxVerbosity;
static std::FILE *logExternalFile{};
static bool logEnabled = Config::DEBUG_BUILD; // default logging off in release builds

static FS::PathString externalLogEnablePath(const char* dirStr)
{
	return FS::pathString(dirStr, "imagine_enable_log_file");
}

static FS::PathString externalLogPath(const char* dirStr)
{
	return FS::pathString(dirStr, "imagine_log.txt");
}

static bool shouldLogToExternalFile(const char* dirStr)
{
	return FS::exists(externalLogEnablePath(dirStr));
}

constexpr int toLogLevel([[maybe_unused]] LoggerSeverity severity)
{
	#ifdef __ANDROID__
	switch(severity)
	{
		case LOGGER_DEBUG_MESSAGE: return ANDROID_LOG_DEBUG;
		default: [[fallthrough]];
		case LOGGER_MESSAGE: return ANDROID_LOG_INFO;
		case LOGGER_WARNING: return ANDROID_LOG_WARN;
		case LOGGER_ERROR: return ANDROID_LOG_ERROR;
	}
	#elif defined __APPLE__
	switch(severity)
	{
		case LOGGER_DEBUG_MESSAGE: return ASL_LEVEL_DEBUG;
		default: [[fallthrough]];
		case LOGGER_MESSAGE: return ASL_LEVEL_NOTICE;
		case LOGGER_WARNING: return ASL_LEVEL_WARNING;
		case LOGGER_ERROR: return ASL_LEVEL_ERR;
	}
	#else
	return 0;
	#endif
}

static void printToLogLineBuffer(const char* msg, va_list args)
{
	std::vsnprintf(logLineBuffer + std::strlen(logLineBuffer), sizeof(logLineBuffer) - std::strlen(logLineBuffer), msg, args);
}

CLINK void logger_vprintf(LoggerSeverity severity, const char* msg, va_list args)
{
	if(!logEnabled)
		return;
	if(severity > loggerVerbosity) return;

	if(logExternalFile)
	{
		va_list args2;
		va_copy(args2, args);
		std::vfprintf(logExternalFile, msg, args2);
		va_end(args2);
		std::fflush(logExternalFile);
	}

	if(bufferLogLineOutput && !std::strchr(msg, '\n'))
	{
		printToLogLineBuffer(msg, args);
		return;
	}

	#ifdef __ANDROID__
	if(std::strlen(logLineBuffer))
	{
		printToLogLineBuffer(msg, args);
		__android_log_write(toLogLevel(severity), "imagine", logLineBuffer);
		logLineBuffer[0] = 0;
	}
	else
		__android_log_vprint(toLogLevel(severity), "imagine", msg, args);
	#elif defined __APPLE__
	if(std::strlen(logLineBuffer))
	{
		printToLogLineBuffer(msg, args);
		asl_log(nullptr, nullptr, toLogLevel(severity), "%s", logLineBuffer);
		logLineBuffer[0] = 0;
	}
	else
		asl_vlog(nullptr, nullptr, toLogLevel(severity), msg, args);
	#else
	std::fprintf(stderr, "%s", IG::Log::toColorCode(IG::Log::Level(severity)));
	std::vfprintf(stderr, msg, args);
	#endif
}

extern "C" void logger_printf(LoggerSeverity severity, const char* msg, ...)
{
	if(!logEnabled)
		return;
	va_list args;
	va_start(args, msg);
	logger_vprintf(severity, msg, args);
	va_end(args);
}

namespace IG::Log
{

static SystemLogger log{"Logger"};

void setLogDirectoryPrefix(const char *dirStr)
{
	if(!logEnabled)
		return;
	#if defined __APPLE__ && (defined __i386__ || defined __x86_64__)
	asl_add_log_file(nullptr, STDERR_FILENO); // output to stderr
	#endif
	if(shouldLogToExternalFile(dirStr) && !logExternalFile)
	{
		auto path = externalLogPath(dirStr);
		log.info("external log file:{}", path);
		if(logExternalFile)
		{
			fclose(logExternalFile);
		}
		logExternalFile = std::fopen(path.data(), "wb");
	}
}

void setEnabled(bool enable)
{
	logEnabled = enable;
}

bool isEnabled() { return logEnabled; }

void printMsg([[maybe_unused]] Level lv, const char* str, size_t strSize)
{
	const char newLine = '\n';
	if(logExternalFile)
	{
		std::fwrite(str, 1, strSize, logExternalFile);
		std::fwrite(&newLine, 1, 1, logExternalFile);
		std::fflush(logExternalFile);
	}
	#ifdef __ANDROID__
	__android_log_write(toLogLevel(LoggerSeverity(lv)), "imagine", str);
	#elif defined __APPLE__
	asl_log(nullptr, nullptr, toLogLevel(LoggerSeverity(lv)), "%s", str);
	#else
	std::fwrite(str, 1, strSize, stderr);
	std::fwrite(&newLine, 1, 1, stderr);
	#endif
}

void print(Level lv, std::string_view tag, std::string_view format, std::format_args args)
{
	if(!logEnabled || int(lv) > loggerVerbosity)
		return;
	StaticString<4096> str;
	Log::beginMsg(str, lv, tag, format, args);
	printMsg(lv, str.c_str(), str.size());
}

}
