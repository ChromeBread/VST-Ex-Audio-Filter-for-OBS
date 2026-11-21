#pragma once

#include "dsp/os2l.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

class QTcpServer;
class QTcpSocket;

// A song tag VirtualDJ can send: our short code in the message, VirtualDJ's get_loaded_song name, the TAG EDITOR label.
struct VdjField {
	const char *code;
	const char *vdj;
	const char *label;
};

// The VirtualDJ link (OS2L), shared by the WaveLine and Key Wheel docks. Listens on this PC only (127.0.0.1) while
// "VirtualDJ link" is on in Tools. VirtualDJ sends beats by itself. The chosen tags come from scripts the user pastes
// into VirtualDJ custom buttons, one button per deck (deck 1's also sends the master key, deck 2's the master deck's
// number). Each send is a header naming the values, the values, and an end mark (Os2lBatch), so a script copied
// before the tag choice changed, a skipped value or two crossing sends can never mix fields up.
class VdjLink : public QObject {
	Q_OBJECT

public:
	static VdjLink &instance();

	bool enabled() const { return on; }
	bool listenFailed() const { return failed; }
	bool connected() const { return peer != nullptr; }
	int port() const { return portNumber; }
	void setPort(int port);

	// Beats: VirtualDJ's beat number `lastPos` came at `lastBeatNs` (os_gettime_ns), at `lastBpm`.
	bool beatLive(int64_t now) const;
	int64_t lastBeatNs = 0, lastPos = 0;
	double lastBpm = 120.0;

	// Song info, kept as last received (never cleared by time or a lost connection). Decks are 1..kDecks.
	static constexpr int kDecks = 4;
	bool hasInfo() const;             // anything received since OBS started
	QString key() const;              // of the master deck, from deck 1's button
	int masterDeck() const;           // from deck 2's button; 0: unknown
	bool deckLive(int deck) const;    // that deck has been received (kept until replaced)
	// false: that deck never sent, or its script was copied before this tag was chosen
	bool has(int deck, const QString &code) const;
	QString value(int deck, const QString &code) const;

	// Every tag, in TAG EDITOR order.
	static const std::vector<VdjField> &fields();
	static bool isField(const QString &code);

	// State for the VDJ button: one line, and a colour.
	QString stateText() const;
	QString stateColor() const;

	// The script for deck `deck`'s custom button sending `codes`; each press starts or stops it. Empty: the deck needs
	// no button. A VirtualDJ custom button holds at most kMaxScript characters (measured 2026-11-21).
	static constexpr int kMaxScript = 500;
	static QString script(const QStringList &codes, int deck, int decks);
	// How often each button sends, for `decks` buttons with `codes`: slow enough to leave VirtualDJ room for its beats.
	static constexpr int kMsPerMessage = 250;
	static int intervalMs(const QStringList &codes, int decks);

signals:
	void changed(); // connection state, key or tags

private:
	VdjLink();
	void apply();
	void listen();
	void acceptPeer();
	void dropPeer();
	void onMessage(const std::string &text);
	static QString groupOf(const QString &code);
	bool fresh(const QString &group) const;

	QTcpServer *server = nullptr;
	QTcpSocket *peer = nullptr;
	Os2lSplitter splitter;
	Os2lBatch batch;
	bool on = false, failed = false;
	int portNumber = 9996;
	QHash<QString, QString> values; // "k" key, "m" master deck, "1cm" deck 1's comment, ...
	QSet<QString> received;         // groups ever received: "k", "m", "1".."4"
};
