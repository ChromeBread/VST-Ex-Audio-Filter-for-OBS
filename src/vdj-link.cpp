#include "vdj-link.h"

#include "dsp/os2l.h"
#include "plugin-meta.h"
#include "ui/ui-text.h"

#include <obs-frontend-api.h>
#include <util/platform.h>

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>

#include <algorithm>

namespace {
constexpr int kDefaultPort = 9996;              // what VirtualDJ's guides use for OS2L on one PC
constexpr int64_t kBeatStaleNs = 2000000000;    // no beat for this long: VirtualDJ stopped
constexpr int kMaxText = 200;     // a tag longer than this is cut, never trusted to be short
constexpr int kMaxMessage = 8000; // one message of the script; anything longer is not ours
}

VdjLink &VdjLink::instance()
{
	// Owned by the main window, so it goes away with Qt and never outlives the event loop.
	static VdjLink *link = [] {
		auto *l = new VdjLink();
		l->setParent(static_cast<QMainWindow *>(obs_frontend_get_main_window()));
		return l;
	}();
	return *link;
}

VdjLink::VdjLink()
{
	server = new QTcpServer(this);
	connect(server, &QTcpServer::newConnection, this, [this] { acceptPeer(); });
	portNumber = std::clamp(PluginMeta::instance().uiValue("wavelineVdjPort").toInt(kDefaultPort), 1024, 65535);
	connect(&PluginMeta::instance(), &PluginMeta::uiChanged, this, [this](const QString &key) {
		if (key == QLatin1String("wavelineVdj"))
			apply();
	});
	apply();
}

void VdjLink::apply()
{
	on = PluginMeta::instance().uiValue("wavelineVdj").toBool(false);
	listen();
}

void VdjLink::setPort(int port)
{
	if (port == portNumber)
		return;
	portNumber = port;
	PluginMeta::instance().setUiValue("wavelineVdjPort", port);
	listen();
}

void VdjLink::listen()
{
	dropPeer();
	server->close();
	failed = on && !server->listen(QHostAddress::LocalHost, quint16(portNumber));
	emit changed();
}

void VdjLink::acceptPeer()
{
	while (QTcpSocket *s = server->nextPendingConnection()) {
		dropPeer(); // the newest VirtualDJ wins
		peer = s;
		splitter.reset();
		connect(s, &QTcpSocket::readyRead, this, [this, s] {
			if (s != peer)
				return;
			QByteArray data = s->readAll();
			splitter.feed(data.constData(), size_t(data.size()), [this](const std::string &o) { onMessage(o); });
		});
		connect(s, &QTcpSocket::disconnected, this, [this, s] {
			if (s == peer)
				dropPeer();
		});
	}
	emit changed();
}

void VdjLink::dropPeer()
{
	if (peer) {
		peer->disconnect(this);
		peer->abort();
		peer->deleteLater();
		peer = nullptr;
	}
	lastBeatNs = 0;
	// Song info stays: a VJ must not see the titles vanish because VirtualDJ restarted or a button was stopped.
	emit changed();
}

void VdjLink::onMessage(const std::string &text)
{
	QJsonObject o = QJsonDocument::fromJson(QByteArray::fromStdString(text)).object();
	QString evt = o["evt"].toString();
	auto now = int64_t(os_gettime_ns());
	if (evt == QLatin1String("beat")) {
		double bpm = o["bpm"].toDouble();
		QJsonValue pos = o["pos"];
		if (!pos.isDouble() || !(bpm >= 20.0 && bpm <= 400.0))
			return; // values from another program: never trust them blindly
		lastPos = int64_t(pos.toDouble());
		lastBpm = bpm;
		lastBeatNs = now;
		return;
	}
	if (evt != QLatin1String("btn") || o["state"].toString() != QLatin1String("on"))
		return;
	QString name = o["name"].toString().left(kMaxMessage);
	if (!batch.feed(name.toStdString(), now))
		return;
	// One deck's button: it replaces everything of the groups it sends ("1" for deck 1's tags, "k", "m"), so a tag no
	// longer sent disappears, and leaves the other decks alone.
	QHash<QString, QString> sent;
	for (const auto &[code, value] : batch.result())
		sent.insert(QString::fromStdString(code), QString::fromStdString(value).left(kMaxText));
	bool change = false;
	QSet<QString> groups;
	for (auto it = sent.cbegin(); it != sent.cend(); ++it)
		groups.insert(groupOf(it.key()));
	if (!received.contains(groups)) {
		received.unite(groups);
		change = true;
	}
	for (auto it = values.begin(); it != values.end();) {
		if (groups.contains(groupOf(it.key())) && !sent.contains(it.key())) {
			it = values.erase(it);
			change = true;
		} else {
			++it;
		}
	}
	for (auto it = sent.cbegin(); it != sent.cend(); ++it) {
		if (values.value(it.key()) != it.value() || !values.contains(it.key()))
			change = true;
		values.insert(it.key(), it.value());
	}
	if (change)
		emit changed();
}

// "1cm" -> "1" (deck 1's tags); "k" and "m" are their own groups.
QString VdjLink::groupOf(const QString &code)
{
	return code.size() > 1 && code[0].isDigit() ? code.left(1) : code;
}

// Once received, kept until something newer comes: never cleared by time, stopped buttons or a lost connection.
bool VdjLink::fresh(const QString &group) const
{
	return received.contains(group);
}

QString VdjLink::key() const
{
	return fresh(QStringLiteral("k")) ? values.value(QStringLiteral("k")) : QString();
}

int VdjLink::masterDeck() const
{
	if (!fresh(QStringLiteral("m")))
		return 0;
	bool ok = false;
	int deck = values.value(QStringLiteral("m")).toInt(&ok);
	return ok && deck >= 1 && deck <= kDecks ? deck : 0;
}

bool VdjLink::deckLive(int deck) const
{
	return fresh(QString::number(deck));
}

bool VdjLink::has(int deck, const QString &code) const
{
	return deckLive(deck) && values.contains(QString::number(deck) + code);
}

QString VdjLink::value(int deck, const QString &code) const
{
	return deckLive(deck) ? values.value(QString::number(deck) + code) : QString();
}

const std::vector<VdjField> &VdjLink::fields()
{
	// Only names a real VirtualDJ answered (2026-11-21); an unknown one makes it skip the value, which costs every
	// send of that deck: "gain", "user1" and "user2" gave nothing and are left out. Always get_loaded_song: the shorter
	// get_title gave another tag than the TAG EDITOR's title on one deck, and the empty-deck hint text on another.
	static const std::vector<VdjField> list = {
		{"ar", "artist", "Artist"},       {"ti", "title", "Title"},         {"rx", "remix", "Remix"},
		{"al", "album", "Album"},         {"rm", "remixer", "Remixer"},     {"co", "composer", "Composer"},
		{"lb", "label", "Label"},         {"ge", "genre", "Genre"},         {"yr", "year", "Year"},
		{"tr", "track", "Track"},         {"gr", "grouping", "Grouping"},   {"bp", "bpm", "Bpm"},
		{"rt", "rating", "Rating"},       {"pc", "playcount", "Play Count"}, {"fs", "firstseen", "First Seen"},
		{"fy", "firstplay", "First Play"}, {"lp", "lastplay", "Last Play"}, {"cm", "comment", "Comment"},
		{"fn", "filename", "Filename"},   {"fp", "filepath", "File Path"},
	};
	return list;
}

bool VdjLink::isField(const QString &code)
{
	for (const VdjField &f : fields())
		if (code == QLatin1String(f.code))
			return true;
	return false;
}

bool VdjLink::beatLive(int64_t now) const
{
	return peer && lastBeatNs && now - lastBeatNs < kBeatStaleNs;
}

bool VdjLink::hasInfo() const
{
	return !received.isEmpty();
}

QString VdjLink::stateText() const
{
	auto now = int64_t(os_gettime_ns());
	if (failed)
		return T("Waveline.Vdj.Busy"); // no number: the menu can end up in a screenshot
	if (!peer)
		return T("Waveline.Vdj.Waiting");
	if (!beatLive(now))
		return T("Waveline.Vdj.Connected");
	return T("Waveline.Vdj.Live");
}

QString VdjLink::stateColor() const
{
	if (failed)
		return "#ef5350";
	if (!peer)
		return "#bbb";
	return beatLive(int64_t(os_gettime_ns())) ? "#66bb6a" : "#ffc107";
}

// Messages one send of deck `deck`'s button makes: header, key or master, the tags, end mark (0: no button).
static int messagesOf(const QStringList &codes, int deck)
{
	int tags = 0;
	for (const VdjField &f : VdjLink::fields())
		tags += codes.contains(QLatin1String(f.code));
	int extra = deck == 1 || deck == 2 ? 1 : 0;
	return tags + extra ? tags + extra + 2 : 0;
}

int VdjLink::intervalMs(const QStringList &codes, int decks)
{
	int total = 0;
	for (int d = 1; d <= std::clamp(decks, 1, kDecks); d++)
		total += messagesOf(codes, d);
	return std::clamp(total * kMsPerMessage, 1000, 20000);
}

// One custom button per deck; each press starts or stops it. Each send: the header naming the values, the values in
// exactly that order, and the end mark repeating the names (Os2lBatch). Header, sends and end mark come from the same
// list, so they always match. Deck 1's button also sends the master key (Key Wheel), deck 2's the master deck's
// number. get_text is not used: in a 4-deck VirtualDJ its buttons never ran.
// VirtualDJ pushes out only about 10 OS2L messages a second, its own beats included; all buttons together sending more
// than that piled up and starved the beats. So the buttons send every intervalMs(), keeping the total near 4 a second.
QString VdjLink::script(const QStringList &codes, int deck, int decks)
{
	QStringList names, sends;
	if (deck < 1 || deck > kDecks)
		return QString();
	if (deck == 1) {
		names << QStringLiteral("k");
		sends << QStringLiteral("os2l_button `deck master get_key`");
	} else if (deck == 2) {
		names << QStringLiteral("m");
		sends << QStringLiteral("os2l_button `deck master get_deck`");
	}
	for (const VdjField &f : fields())
		if (codes.contains(QLatin1String(f.code))) {
			names << QString::number(deck) + QLatin1String(f.code);
			sends << QStringLiteral("os2l_button `deck %1 get_loaded_song '%2'`").arg(deck).arg(QLatin1String(f.vdj));
		}
	if (names.isEmpty())
		return QString(); // nothing to send: this deck needs no button
	QString header = QStringLiteral("cb|") + names.join(',');
	QString end = QString::fromStdString(Os2lBatch::endOf(header.toStdString()));
	QString name = QStringLiteral("cb%1").arg(deck);
	return QStringLiteral("repeat_start '%1' ? repeat_stop '%1' : repeat_start '%1' %5ms & os2l_button \"%2\" & %3 & "
			      "os2l_button \"%4\"")
		.arg(name, header, sends.join(QStringLiteral(" & ")), end)
		.arg(intervalMs(codes, decks));
}
