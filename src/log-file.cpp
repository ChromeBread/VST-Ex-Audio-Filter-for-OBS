#include "log-file.h"

#include <obs-module.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>

#include <atomic>

namespace {

constexpr qint64 kMaxBytes = 512 * 1024; // then the file becomes .old, so at most about 1 MB stays on the PC
std::atomic<bool> enabled{false};
QMutex mutex;

// The home folder holds the Windows user name, which must not reach a log someone may send to the developer.
QString masked(QString text)
{
	QString home = QDir::homePath();
	text.replace(QDir::toNativeSeparators(home), "%USERPROFILE%", Qt::CaseInsensitive);
	text.replace(home, "%USERPROFILE%", Qt::CaseInsensitive);
	return text;
}

const char *levelName(int level)
{
	switch (level) {
	case LOG_ERROR:
		return "error";
	case LOG_WARNING:
		return "warning";
	case LOG_DEBUG:
		return "debug";
	default:
		return "info";
	}
}

} // namespace

QString log_file_path()
{
	char *p = obs_module_config_path("vst-ex.log");
	QString path = QString::fromUtf8(p);
	bfree(p);
	return path;
}

bool log_file_enabled()
{
	return enabled;
}

void log_file_set_enabled(bool on)
{
	enabled = on;
}

void log_file_write(int log_level, const char *text)
{
	if (!enabled || !text)
		return;
	QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss ") + levelName(log_level) + ": " +
		       masked(QString::fromUtf8(text)) + "\r\n";
	QMutexLocker lock(&mutex);
	QString path = log_file_path();
	QDir().mkpath(QFileInfo(path).path());
	if (QFileInfo(path).size() > kMaxBytes) {
		QFile::remove(path + ".old"); // our own rotated log, not user data
		QFile::rename(path, path + ".old");
	}
	QFile f(path);
	if (f.open(QIODevice::Append))
		f.write(line.toUtf8());
}
