#include "plugin-meta.h"

#include <obs-module.h>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QSettings>

#include <algorithm>

namespace {

// Thumbnails are kept small on purpose ("low resolution" previews, fast to load).
constexpr int kThumbMaxWidth = 480;

QString configPath(const QString &name)
{
	char *p = obs_module_config_path(name.toUtf8().constData());
	QString path = QString::fromUtf8(p);
	bfree(p);
	return path;
}

// Moves a folder, falling back to copy + delete across drives.
bool moveTree(const QString &from, const QString &to)
{
	if (!QDir(from).exists())
		return true;
	QDir().mkpath(QFileInfo(to).path());
	if (QDir().rename(from, to))
		return true;
	QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		QString file = it.next();
		QString target = to + file.mid(from.size());
		QDir().mkpath(QFileInfo(target).path());
		QFile::remove(target);
		if (!QFile::copy(file, target))
			return false; // originals stay in place
	}
	QDir(from).removeRecursively();
	return true;
}

// The key used before copies of a plugin were told apart; thumbnails saved back then are still named after it.
QString legacyKey(const vstex::PluginDescription &d)
{
	return QString("%1|%2|%3").arg(vstex::formatName(d.format), QString::fromStdString(d.uid),
				       QString::fromStdString(d.name));
}

} // namespace

PluginMeta &PluginMeta::instance()
{
	static PluginMeta meta;
	return meta;
}

// One key per file, the way OBS tells plugins apart by path: copies in two folders are separate.
QString PluginMeta::keyOf(const vstex::PluginDescription &d)
{
	return legacyKey(d) + "|" + QDir::cleanPath(QString::fromStdString(d.path)).toLower();
}

// Hidden stays on every copy (nothing hidden reappears); favorites, memo and the recent list go to the first copy only.
void PluginMeta::adoptLegacyKeys(QVector<vstex::PluginDescription> plugins)
{
	std::sort(plugins.begin(), plugins.end(), [](const auto &a, const auto &b) {
		return QString::fromStdString(a.path).compare(QString::fromStdString(b.path), Qt::CaseInsensitive) < 0;
	});
	QSet<QString> adopted;
	for (const auto &d : plugins) {
		QString legacy = legacyKey(d);
		auto old = entries.constFind(legacy);
		if (old == entries.constEnd() && !recentKeys.contains(legacy))
			continue;
		if (old != entries.constEnd()) {
			Entry from = old.value();
			Entry &to = entries[keyOf(d)];
			to.hidden = to.hidden || from.hidden;
			if (!adopted.contains(legacy)) {
				to.favorites |= from.favorites;
				if (to.memo.isEmpty())
					to.memo = from.memo;
			}
		}
		int recent = (int)recentKeys.indexOf(legacy);
		if (recent >= 0)
			recentKeys[recent] = keyOf(d);
		adopted.insert(legacy);
	}
	if (adopted.isEmpty())
		return;
	QString path = configPath("plugin-meta.json");
	QFile::copy(path, path + ".before-file-keys"); // keeps the first backup: copy fails if it exists
	for (const QString &legacy : adopted)
		entries.remove(legacy);
	save();
}

PluginMeta::PluginMeta()
{
	load();
	// Parts picked in the installer's custom setup arrive once in this file; from then on Tools decides.
	QString picked = configPath("install-features.ini");
	if (QFile::exists(picked)) {
		QSettings ini(picked, QSettings::IniFormat);
		for (const char *key : {"featureChainView", "featureMixer", "featureSwitcher", "featureWaveline"})
			ui[key] = ini.value(QString("features/") + key, true).toBool();
		save();
		QFile::remove(picked);
	}
	// Full-size screenshots used to sit next to the thumbnails; they get their own folder so each can be
	// deleted on its own.
	const QStringList full = QDir(screenshotDir()).entryList({"*-full.png"}, QDir::Files);
	if (!full.isEmpty())
		QDir().mkpath(highResDir());
	for (const QString &name : full)
		QFile::rename(screenshotDir() + "/" + name, highResDir() + "/" + name);
}

void PluginMeta::reload()
{
	entries.clear();
	favoriteNames.clear();
	favCount = 1;
	ui = QJsonObject();
	recentKeys.clear();
	load();
	emit changed();
}

void PluginMeta::load()
{
	QFile f(configPath("plugin-meta.json"));
	if (!f.open(QIODevice::ReadOnly))
		return;
	QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
	for (const auto &v : root["favoriteNames"].toArray())
		favoriteNames << v.toString();
	favCount = root["favoriteCount"].toInt(1);
	ui = root["ui"].toObject();
	for (const auto &v : root["recent"].toArray())
		recentKeys << v.toString();
	QJsonObject plugins = root["plugins"].toObject();
	for (auto it = plugins.begin(); it != plugins.end(); ++it) {
		QJsonObject o = it.value().toObject();
		quint8 favs = (quint8)o["favorites"].toInt();
		entries.insert(it.key(), {o["hidden"].toBool(), favs, o["memo"].toString()});
		for (int i = favCount; i < kFavorites; i++) // never hide a favorite that is in use
			if (favs & (1u << i))
				favCount = i + 1;
	}
	favCount = qBound(1, favCount, (int)kFavorites);
}

void PluginMeta::save() const
{
	QJsonObject plugins;
	for (auto it = entries.begin(); it != entries.end(); ++it) {
		const Entry &e = it.value();
		if (!e.hidden && !e.favorites && e.memo.isEmpty())
			continue;
		plugins.insert(it.key(), QJsonObject{{"hidden", e.hidden}, {"favorites", e.favorites}, {"memo", e.memo}});
	}
	QJsonObject root{{"version", 1},
			 {"favoriteNames", QJsonArray::fromStringList(favoriteNames)},
			 {"favoriteCount", favCount},
			 {"ui", ui},
			 {"recent", QJsonArray::fromStringList(recentKeys)},
			 {"plugins", plugins}};

	QString path = configPath("plugin-meta.json");
	QDir().mkpath(QFileInfo(path).path());
	QSaveFile f(path);
	if (f.open(QIODevice::WriteOnly)) {
		f.write(QJsonDocument(root).toJson());
		f.commit();
	}
}

void PluginMeta::setHidden(const QString &key, bool hidden)
{
	entries[key].hidden = hidden;
	save();
	emit changed();
}

void PluginMeta::setFavorite(const QString &key, int index, bool on)
{
	quint8 bit = quint8(1u << index);
	Entry &e = entries[key];
	e.favorites = on ? (e.favorites | bit) : (e.favorites & ~bit);
	save();
	emit changed();
}

void PluginMeta::setHidden(const QStringList &keys, bool hidden)
{
	for (const QString &key : keys)
		entries[key].hidden = hidden;
	save();
	emit changed();
}

void PluginMeta::setFavorite(const QStringList &keys, int index, bool on)
{
	quint8 bit = quint8(1u << index);
	for (const QString &key : keys) {
		Entry &e = entries[key];
		e.favorites = on ? (e.favorites | bit) : (e.favorites & ~bit);
	}
	save();
	emit changed();
}

void PluginMeta::setMemo(const QString &key, const QString &memo)
{
	entries[key].memo = memo;
	save();
	emit changed();
}

QString PluginMeta::favoriteName(int index) const
{
	return favoriteNames.value(index);
}

void PluginMeta::setFavoriteName(int index, const QString &name)
{
	while (favoriteNames.size() < kFavorites)
		favoriteNames << QString();
	favoriteNames[index] = name;
	save();
	emit changed();
}

void PluginMeta::addFavorite()
{
	if (favCount >= kFavorites)
		return;
	favCount++;
	save();
	emit changed();
}

void PluginMeta::removeLastFavorite()
{
	if (favCount <= 1)
		return;
	favCount--;
	quint8 bit = quint8(1u << favCount);
	for (Entry &e : entries)
		e.favorites &= ~bit;
	if (favCount < favoriteNames.size())
		favoriteNames[favCount].clear();
	save();
	emit changed();
}

void PluginMeta::setUiValue(const QString &key, const QJsonValue &value)
{
	ui.insert(key, value);
	save();
	emit uiChanged(key);
}

void PluginMeta::addRecent(const QString &key)
{
	recentKeys.removeAll(key);
	recentKeys.prepend(key);
	while (recentKeys.size() > kMaxRecent)
		recentKeys.removeLast();
	save();
	emit changed();
}

void PluginMeta::clearRecent()
{
	recentKeys.clear();
	save();
	emit changed();
}

QString PluginMeta::dataDir() const
{
	QString dir = ui.value("dataDir").toString();
	return dir.isEmpty() ? QDir::cleanPath(configPath("")) : dir;
}

bool PluginMeta::setDataDir(const QString &dir)
{
	QString from = dataDir(), to = QDir::cleanPath(dir);
	if (QDir(from) != QDir(to))
#ifdef VST_ENIGMAX_LOADER
		for (const char *sub : {"thumbnails", "screenshots"}) // each module moves only its own folders
#else
		for (const char *sub : {"history", "presets"})
#endif
			if (!moveTree(from + "/" + sub, to + "/" + sub))
				return false;
	setUiValue("dataDir", to);
	emit changed();
	return true;
}

QString PluginMeta::screenshotDir() const
{
	return dataDir() + "/thumbnails";
}

QString PluginMeta::highResDir() const
{
	return dataDir() + "/screenshots";
}

void PluginMeta::deleteThumbnails()
{
	QFile::moveToTrash(screenshotDir()); // Recycle Bin, so a mistaken click can be undone
	emit changed();
}

void PluginMeta::deleteHighRes()
{
	QFile::moveToTrash(highResDir());
	emit changed();
}

QString PluginMeta::filterShotPath(const QString &filterUuid, bool full) const
{
	return (full ? highResDir() : screenshotDir()) + QString("/filter-%1%2.png").arg(filterUuid, full ? "-full" : "");
}

QString PluginMeta::userThumbnailPath(const QString &key, const char *suffix) const
{
	QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
	return (*suffix ? highResDir() : screenshotDir()) +
	       QString("/%1%2.png").arg(QString::fromLatin1(hash), QString::fromLatin1(suffix));
}

// This file's own picture, else one saved under the legacy key (shared by its copies until each gets its own).
QString PluginMeta::userShot(const vstex::PluginDescription &d, const char *suffix) const
{
	for (const QString &key : {keyOf(d), legacyKey(d)}) {
		QString path = userThumbnailPath(key, suffix);
		if (QFileInfo::exists(path))
			return path;
	}
	return QString();
}

QString PluginMeta::thumbnailPath(const vstex::PluginDescription &d) const
{
	QString user = userShot(d);
	if (!user.isEmpty())
		return user;
	QString bundled = QString::fromStdString(d.snapshot);
	return !bundled.isEmpty() && QFileInfo::exists(bundled) ? bundled : QString();
}

bool PluginMeta::hasUserThumbnail(const vstex::PluginDescription &d) const
{
	return !userShot(d).isEmpty();
}

QString PluginMeta::fullImagePath(const vstex::PluginDescription &d) const
{
	QString full = userShot(d, "-full");
	return !full.isEmpty() ? full : thumbnailPath(d);
}

void PluginMeta::setThumbnail(const vstex::PluginDescription &d, const QImage &image)
{
	saveShot(image, userThumbnailPath(keyOf(d)), userThumbnailPath(keyOf(d), "-full"));
	emit changed();
}

void PluginMeta::setFilterShot(const QString &filterUuid, const QImage &image)
{
	saveShot(image, filterShotPath(filterUuid, false), filterShotPath(filterUuid, true));
	emit changed();
}

// Small copy for lists; the full-size one only when it differs.
void PluginMeta::saveShot(const QImage &image, const QString &smallPath, const QString &fullPath)
{
	QDir().mkpath(QFileInfo(smallPath).path());
	QDir().mkpath(QFileInfo(fullPath).path());
	QImage small = image.width() > kThumbMaxWidth ? image.scaledToWidth(kThumbMaxWidth, Qt::SmoothTransformation)
						      : image;
	small.save(smallPath, "PNG");
	if (small.size() != image.size())
		image.save(fullPath, "PNG");
	else
		QFile::remove(fullPath);
}
