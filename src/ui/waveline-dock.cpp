#include "ui/waveline-dock.h"

#include "dsp/beat-grid.h"
#include "dsp/os2l.h"
#include "dsp/peak-ring.h"
#include "master-bus.h"
#include "plugin-meta.h"
#include "ui/ui-text.h"
#include "vdj-link.h"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/platform.h>

#include <QActionGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace {

constexpr const char *kDockId = "vst-ex-waveline-dock";
constexpr const char *kMasterId = "master"; // stands for the master bus in the saved list
constexpr uint64_t kBinNs = 5000000;        // 5 ms; PeakRing::kBins of these is about 80 s of history
constexpr int kFrameMs = 33;
constexpr int kSyncFrames = 15; // sources and routing are checked every half second
constexpr uint64_t kHeadNs = 60000000; // audio arrives a little after "now"; keeps the right edge from flickering
constexpr double kWidestSeconds = 60.0, kNarrowestSeconds = 2.0;
constexpr float kFloorDb = -60.0f;
constexpr int kFaintAlpha = 70;
constexpr int kControlHeight = 14; // three rows of these (the Tr buttons) set how thin the dock can get
constexpr double kDefaultBpm = 120.0;
constexpr int kMinOpacity = 10; // a source at 0% could not be found again on screen
constexpr int kBeatRowHeight = 2 * kControlHeight + 1;
constexpr int kBeatRowWidth = kBeatRowHeight + 3 + 4 * 9; // circle, gap, the narrowest 4x2
constexpr int kEdgeGrab = 4;         // pixels either side of a waveform edge that start a resize
constexpr int kNarrowestWave = 60;   // the waveform never gets narrower than this

const QColor kPalette[] = {{255, 76, 76},  {0, 188, 212},  {255, 193, 7},  {156, 204, 101},
			   {186, 104, 200}, {255, 138, 101}, {100, 181, 246}, {240, 98, 146}};

double spanSeconds(int zoomPercent)
{
	return kWidestSeconds * std::pow(kNarrowestSeconds / kWidestSeconds, zoomPercent / 100.0);
}

struct Track {
	QString id;
	OBSWeakSource weak;
	PeakRing left, right; // a mono source writes the same peaks to both
	QString name;
	bool hooked = false;
	bool onAir = false;
	int color = 0;
	int opacity = 100; // percent
};

void onAudio(void *param, obs_source_t *source, const struct audio_data *audio, bool)
{
	auto *track = static_cast<Track *>(param);
	const struct audio_output_info *info = audio_output_get_info(obs_get_audio());
	size_t planes = audio_output_get_channels(obs_get_audio());
	uint32_t binFrames = std::max<uint32_t>(1, uint32_t(uint64_t(info->samples_per_sec) * kBinNs / 1000000000ULL));
	float volume = obs_source_get_volume(source);
	for (uint32_t start = 0; start < audio->frames; start += binFrames) {
		uint32_t end = std::min(audio->frames, start + binFrames);
		float peak[2] = {0.0f, 0.0f}; // even channels count as left, odd as right (surround folds in)
		for (size_t c = 0; c < planes; c++) {
			auto *data = reinterpret_cast<const float *>(audio->data[c]);
			if (!data)
				continue;
			for (uint32_t i = start; i < end; i++)
				peak[c % 2] = std::max(peak[c % 2], std::fabs(data[i]));
		}
		if (planes < 2)
			peak[1] = peak[0];
		uint64_t at = audio->timestamp + uint64_t(start) * 1000000000ULL / info->samples_per_sec;
		track->left.put(at / kBinNs, peak[0] * volume);
		track->right.put(at / kBinNs, peak[1] * volume);
	}
}

float heightOf(float peak)
{
	if (peak <= 0.0f)
		return 0.0f;
	return std::clamp((20.0f * std::log10(peak) - kFloorDb) / -kFloorDb, 0.0f, 1.0f);
}

// The waveform sits in the middle of this widget with the same empty margin on both sides; dragging either edge
// changes both margins.
class WaveView : public QWidget {
public:
	WaveView(QWidget *parent, const std::vector<Track *> &list) : QWidget(parent), tracks(list)
	{
		setMinimumSize(kNarrowestWave, 16);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		setAttribute(Qt::WA_OpaquePaintEvent);
		setMouseTracking(true);
	}

	int zoom = 25;
	int64_t beatNs = 0; // 0: no beat dots
	int64_t beatAnchor = 0;
	bool stereo = false;
	int inset = 0;
	std::function<void(int)> insetChanged; // called when a drag ends
	// VirtualDJ decks under the source names (drawDecks); no label: nothing.
	QString tagLabel;
	QStringList tagLabelChoices; // every chosen tag's name, for a label width that does not change
	QString deckText[VdjLink::kDecks];
	int decks = 2, masterDeck = 0;
	std::function<void()> tagClicked;

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		int margin = usableInset();
		p.fillRect(rect(), palette().color(QPalette::Window));
		p.translate(margin, 0);
		int w = width() - 2 * margin, base = height() - 6, top = 2;
		p.fillRect(0, 0, w, height(), Qt::black);
		p.setPen(QColor(70, 70, 70));
		p.drawLine(0, base, w, base);
		int mid = (top + base) / 2;
		if (stereo)
			p.drawLine(0, mid, w, mid);
		double binsPerPixel = spanSeconds(zoom) * 1e9 / double(kBinNs) / double(w);
		auto nowNs = int64_t(os_gettime_ns() - kHeadNs);
		// Columns are fixed to absolute time, so old peaks keep their shape while they scroll.
		int64_t newest = int64_t(std::floor(double(nowNs / int64_t(kBinNs)) / binsPerPixel));
		if (beatNs > 0)
			drawBeats(p, w, nowNs, newest, binsPerPixel, base);
		if (tracks.empty()) {
			p.setPen(QColor(150, 150, 150));
			p.drawText(QRect(0, 0, w, height()), Qt::AlignCenter | Qt::TextWordWrap, T("Waveline.Empty"));
			return;
		}
		QVector<QLine> lines;
		lines.reserve(2 * w);
		for (Track *track : tracks) { // the list is the layer order, bottom first
			lines.clear();
			for (int x = w - 1; x >= 0; x--) {
				int64_t column = newest - (w - 1 - x);
				auto from = uint64_t(std::ceil(double(column) * binsPerPixel));
				auto to = std::max(from + 1, uint64_t(std::ceil(double(column + 1) * binsPerPixel)));
				float l = 0.0f, r = 0.0f;
				for (uint64_t b = from; b < to; b++) {
					l = std::max(l, track->left.get(b));
					r = std::max(r, track->right.get(b));
				}
				if (stereo) {
					int half = mid - top;
					if (float h = heightOf(l); h > 0.0f)
						lines.push_back(QLine(x, mid, x, mid - int(h * float(half))));
					if (float h = heightOf(r); h > 0.0f)
						lines.push_back(QLine(x, mid, x, mid + int(h * float(half))));
				} else if (float h = heightOf(std::max(l, r)); h > 0.0f) {
					lines.push_back(QLine(x, base, x, base - int(h * float(base - top))));
				}
			}
			QColor color = kPalette[track->color % std::size(kPalette)];
			color.setAlpha((track->onAir ? 255 : kFaintAlpha) * track->opacity / 100);
			p.setPen(color);
			p.drawLines(lines);
		}
		// Source names in their colours.
		QFont f = font();
		f.setPixelSize(10);
		p.setFont(f);
		int x = 4;
		for (Track *track : tracks) {
			if (x >= w)
				break;
			QColor color = kPalette[track->color % std::size(kPalette)];
			color.setAlpha(track->onAir ? 255 : 120);
			p.setPen(color);
			p.drawText(x, 11, track->name);
			x += QFontMetrics(f).horizontalAdvance(track->name) + 12;
		}
		drawDecks(p, f, margin, w);
	}

	// The 4 VirtualDJ decks' tag (e.g. the anime title in the comment) on the line under the source names: which tag
	// in grey, then the decks side by side in equal columns. Everything has a fixed place, so switching the tag never
	// moves a column. The master deck is marked and bright, the others faint. Clicking the line shows the next tag.
	void drawDecks(QPainter &p, QFont f, int margin, int w)
	{
		tagRect = QRect();
		if (tagLabel.isEmpty())
			return;
		const int top = 14, h = 14, left = 4;
		QFontMetrics fm(f);
		int labelWidth = 0; // the widest of the chosen tags' names, so the columns stay put
		for (const QString &name : tagLabelChoices)
			labelWidth = std::max(labelWidth, fm.horizontalAdvance(name + ": "));
		labelWidth = std::max(labelWidth, fm.horizontalAdvance(tagLabel + ": "));
		p.setFont(f);
		p.setPen(QColor(150, 150, 150));
		p.drawText(QRect(left, top, labelWidth, h), Qt::AlignLeft | Qt::AlignVCenter, tagLabel + ": ");
		int x0 = left + labelWidth;
		double column = double(w - x0 - left) / std::max(1, decks);
		if (column < 24)
			return; // too narrow to say anything
		QFont bold = f;
		bold.setBold(true);
		int markWidth = std::max(fm.horizontalAdvance(QStringLiteral("▶ ")), QFontMetrics(bold).horizontalAdvance(QStringLiteral("▶ ")));
		int numberWidth = QFontMetrics(bold).horizontalAdvance(QStringLiteral("4 "));
		for (int d = 1; d <= decks; d++) {
			bool master = d == masterDeck;
			int cx = x0 + int(column * (d - 1));
			int room = int(column) - markWidth - numberWidth - 6;
			p.setFont(master ? bold : f);
			p.setPen(master ? QColor(255, 255, 255) : QColor(125, 125, 125));
			if (master)
				p.drawText(QRect(cx, top, markWidth, h), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("▶"));
			p.drawText(QRect(cx + markWidth, top, numberWidth, h), Qt::AlignLeft | Qt::AlignVCenter, QString::number(d));
			QString text = deckText[d - 1].isEmpty() ? QStringLiteral("-") : deckText[d - 1];
			p.drawText(QRect(cx + markWidth + numberWidth, top, std::max(0, room), h), Qt::AlignLeft | Qt::AlignVCenter,
				   QFontMetrics(p.font()).elidedText(text, Qt::ElideRight, std::max(0, room)));
		}
		tagRect = QRect(margin + left, top, w - 2 * left, h);
	}

	void mousePressEvent(QMouseEvent *e) override
	{
		QPoint at = e->position().toPoint();
		dragging = e->button() == Qt::LeftButton && nearEdge(at.x());
		if (!dragging && e->button() == Qt::LeftButton && tagRect.contains(at) && tagClicked)
			tagClicked();
	}

	void mouseMoveEvent(QMouseEvent *e) override
	{
		int x = int(e->position().x());
		if (dragging) {
			inset = std::clamp(x < width() / 2 ? x : width() - x, 0, maxInset());
			update();
		}
		bool overTag = tagRect.contains(e->position().toPoint());
		setCursor(dragging || nearEdge(x) ? Qt::SizeHorCursor : overTag ? Qt::PointingHandCursor : Qt::ArrowCursor);
		setToolTip(overTag ? T("Waveline.Vdj.TagClick.Tip") : QString());
	}

	void mouseReleaseEvent(QMouseEvent *) override
	{
		if (dragging && insetChanged)
			insetChanged(inset);
		dragging = false;
	}

private:
	int maxInset() const { return std::max(0, (width() - kNarrowestWave) / 2); }
	int usableInset() const { return std::min(inset, maxInset()); } // a narrower dock squeezes the margin first

	bool nearEdge(int x) const
	{
		int m = usableInset();
		return std::abs(x - m) <= kEdgeGrab || std::abs(x - (width() - m)) <= kEdgeGrab;
	}

	// Dots in the strip under the baseline, where no waveform can cover them; a bigger one every 4 beats.
	void drawBeats(QPainter &p, int w, int64_t nowNs, int64_t newest, double binsPerPixel, int base)
	{
		for (int64_t beat = beat_at_or_before(nowNs, beatAnchor, beatNs);; beat--) {
			double bin = double(beatAnchor + beat * beatNs) / double(kBinNs);
			int x = w - 1 - int(newest - int64_t(std::floor(bin / binsPerPixel)));
			if (x < 0)
				break;
			if (beat_starts_bar(beat))
				p.fillRect(x - 1, base + 1, 4, 4, QColor(255, 255, 255));
			else
				p.fillRect(x, base + 2, 2, 2, QColor(170, 170, 170));
		}
	}

	const std::vector<Track *> &tracks;
	bool dragging = false;
	QRect tagRect; // where the tag was drawn last, in widget coordinates
};

// One beat row: the place in the 16-beat phrase as a number in a circle that fills one step per beat, and a 4x2 bar
// whose lit cell moves one step per beat. Off: grey and still, no number.
class BeatRow : public QWidget {
public:
	// Grows sideways to fill what the left side's width leaves free, so the cells get bigger instead of a gap.
	explicit BeatRow(QWidget *parent) : QWidget(parent)
	{
		setMinimumSize(kBeatRowWidth, kBeatRowHeight);
		setMaximumHeight(kBeatRowHeight);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	bool on = false;
	int64_t beat = 0;

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		int h = height();
		int phrase = beat_in_cycle(beat, 16), eight = beat_in_cycle(beat, 8);
		const QColor lit(76, 175, 80), accent(255, 193, 7), dark(48, 48, 48); // accent: first beat of a bar
		QRectF ring(2.0, 2.0, h - 4.0, h - 4.0);
		p.setPen(QPen(dark, 3.0));
		p.drawEllipse(ring);
		if (on) {
			p.setPen(QPen(phrase % 4 == 0 ? accent : lit, 3.0, Qt::SolidLine, Qt::FlatCap));
			p.drawArc(ring, 90 * 16, -(phrase + 1) * 360); // 1/16 of a turn (360*16/16) per beat
			QFont f = font();
			f.setPixelSize(10);
			f.setBold(true);
			p.setFont(f);
			p.setPen(QColor(230, 230, 230));
			p.drawText(QRect(0, 0, h, h), Qt::AlignCenter, QString::number(phrase + 1));
		}
		p.setRenderHint(QPainter::Antialiasing, false);
		int x0 = h + 3;
		double cellW = double(width() - x0) / 4.0, cellH = double(height()) / 2.0;
		for (int i = 0; i < 8; i++) {
			QColor c = on && i == eight ? (i % 4 == 0 ? accent : lit) : dark;
			p.fillRect(QRectF(x0 + i % 4 * cellW, i / 4 * cellH + 0.5, cellW - 1.0, cellH - 1.0), c);
		}
	}
};

// Sound of several sources side by side in time. Clicking Tr buttons changes what is shown strong, never the routing.
// Right side: the beat. The upper row runs on the BPM typed here; the lower row, shown only with the VirtualDJ link
// switched on in Tools, follows the beats VirtualDJ sends (OS2L).
class WavelineDock : public QWidget {
public:
	WavelineDock() : QWidget(nullptr)
	{
		auto &meta = PluginMeta::instance();
		for (const auto &v : meta.uiValue("wavelineSources").toArray())
			wanted << v.toString();
		styles = meta.uiValue("wavelineStyles").toObject();

		auto *zoom = new QSlider(Qt::Vertical, this);
		zoom->setMinimumHeight(10);
		zoom->setRange(0, 100);
		zoom->setValue(std::clamp(meta.uiValue("wavelineZoom").toInt(25), 0, 100));
		zoom->setToolTip(T("Waveline.Zoom.Tip"));
		zoomLabel = new QLabel(this);
		zoomLabel->setAlignment(Qt::AlignCenter);
		QFont small = zoomLabel->font();
		small.setPixelSize(10);
		zoomLabel->setFont(small);
		zoomLabel->setFixedWidth(30);
		view = new WaveView(this, tracks);
		view->zoom = zoom->value();
		view->stereo = meta.uiValue("wavelineStereo").toBool(false);
		view->inset = std::max(0, meta.uiValue("wavelineInset").toInt(0));
		view->insetChanged = [](int v) { PluginMeta::instance().setUiValue("wavelineInset", v); };
		view->tagClicked = [this] { nextTag(); };
		connect(zoom, &QSlider::valueChanged, this, [this](int v) {
			PluginMeta::instance().setUiValue("wavelineZoom", v);
			view->zoom = v;
			showZoom(v);
		});
		showZoom(zoom->value());

		auto *pick = new QToolButton(this);
		pick->setText(QStringLiteral("+"));
		pick->setToolTip(T("Waveline.Sources"));
		pick->setPopupMode(QToolButton::InstantPopup);
		auto *menu = new QMenu(pick);
		pick->setMenu(menu);
		connect(menu, &QMenu::aboutToShow, this, [this, menu] { fillMenu(menu); });

		auto compact = [](QToolButton *b, int width) {
			b->setFixedSize(width, kControlHeight);
			b->setStyleSheet("QToolButton{padding:0;font-size:10px;}QToolButton::menu-indicator{image:none;}"
					 "QToolButton:checked{background:#2e7d32;color:white;}");
		};
		compact(pick, 18);
		auto *viewButton = new QToolButton(this);
		viewButton->setText(QStringLiteral("≡"));
		viewButton->setToolTip(T("Waveline.ViewMenu"));
		viewButton->setPopupMode(QToolButton::InstantPopup);
		compact(viewButton, 18);
		viewButton->setMenu(buildViewMenu(viewButton));

		QJsonValue saved = meta.uiValue("wavelineTracks");
		followStream = saved.isUndefined();
		mask = followStream ? 0 : uint32_t(saved.toInt());
		auto *controls = new QGridLayout();
		controls->setSpacing(1);
		controls->addWidget(pick, 0, 0);
		controls->addWidget(viewButton, 1, 0);
		for (int t = 0; t < 6; t++) {
			auto *b = new QToolButton(this);
			b->setText(QStringLiteral("Tr%1").arg(t + 1));
			b->setToolTip(T("Waveline.Track.Tip").arg(t + 1));
			b->setCheckable(true);
			compact(b, 24);
			connect(b, &QToolButton::clicked, this, [this] { readButtons(); });
			trackButtons[t] = b;
			controls->addWidget(b, t / 2, 1 + t % 2); // two per row: a narrower left side
		}

		// Upper beat row: number + 4x2, then BPM over [● ↺].
		ownRow = new BeatRow(this);
		bpmBox = new QDoubleSpinBox(this);
		bpmBox->setRange(20.0, 300.0);
		bpmBox->setDecimals(1);
		bpmBox->setValue(std::clamp(meta.uiValue("wavelineBpm").toDouble(kDefaultBpm), 20.0, 300.0));
		bpmBox->setToolTip(T("Waveline.Bpm.Tip"));
		bpmBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
		bpmBox->setKeyboardTracking(false); // typing "128" must not jump through 1 and 12
		bpmBox->setFixedSize(37, kControlHeight);
		bpmBox->setStyleSheet("QDoubleSpinBox{padding:0;margin:0;border:0;font-size:10px;min-height:0;}");
		connect(bpmBox, &QDoubleSpinBox::valueChanged, this,
			[](double v) { PluginMeta::instance().setUiValue("wavelineBpm", v); });
		auto *beats = new QToolButton(this);
		beats->setText(QStringLiteral("●"));
		beats->setToolTip(T("Waveline.Beat.Tip"));
		beats->setCheckable(true);
		compact(beats, 18);
		auto *reset = new QToolButton(this);
		reset->setText(QStringLiteral("↺"));
		reset->setToolTip(T("Waveline.Reset.Tip"));
		compact(reset, 18);
		connect(reset, &QToolButton::clicked, this, [this] { resetBeat(); });
		beats->setChecked(meta.uiValue("wavelineBeats").toBool(false));
		beatsOn = beats->isChecked();
		connect(beats, &QToolButton::toggled, this, [this](bool on) {
			PluginMeta::instance().setUiValue("wavelineBeats", on);
			beatsOn = on;
		});

		// Lower beat row: VirtualDJ. Number + 4x2, then its BPM over the VDJ button.
		vdjRow = new BeatRow(this);
		vdjBpm = new QLabel(QStringLiteral("---"), this);
		vdjBpm->setFont(small);
		vdjBpm->setFixedSize(37, kControlHeight);
		vdjBpm->setToolTip(T("Waveline.VdjBpm.Tip"));
		vdjButton = new QToolButton(this);
		vdjButton->setText(QStringLiteral("VDJ"));
		vdjButton->setPopupMode(QToolButton::InstantPopup);
		compact(vdjButton, 37);
		auto *vdjMenu = new QMenu(vdjButton);
		vdjButton->setMenu(vdjMenu);
		connect(vdjMenu, &QMenu::aboutToShow, this, [this, vdjMenu] { fillVdjMenu(vdjMenu); });

		// Each row's BPM sits over its buttons in a column as wide as the BPM box.
		auto column = [this](QWidget *top, std::initializer_list<QWidget *> bottom) {
			auto *box = new QWidget(this);
			box->setFixedWidth(37);
			auto *v = new QVBoxLayout(box);
			v->setContentsMargins(0, 0, 0, 0);
			v->setSpacing(1);
			v->addWidget(top);
			auto *h = new QHBoxLayout();
			h->setSpacing(1);
			for (QWidget *w : bottom)
				h->addWidget(w);
			h->addStretch(1);
			v->addLayout(h);
			return box;
		};
		vdjSide = column(vdjBpm, {vdjButton});
		auto *beatGrid = new QGridLayout();
		beatGrid->setHorizontalSpacing(3);
		beatGrid->setVerticalSpacing(1);
		beatGrid->addWidget(ownRow, 0, 0);
		beatGrid->addWidget(column(bpmBox, {beats, reset}), 0, 1);
		beatGrid->addWidget(vdjRow, 1, 0);
		beatGrid->addWidget(vdjSide, 1, 1);
		beatGrid->setColumnStretch(0, 1);
		beatGrid->setAlignment(Qt::AlignTop);

		anchorNs = int64_t(os_gettime_ns());

		// Left: buttons, zoom bar, zoom %. Both sides get the same width so the waveform sits in the middle.
		auto *leftBox = new QWidget(this);
		auto *left = new QHBoxLayout(leftBox);
		left->setContentsMargins(0, 0, 0, 0);
		left->setSpacing(1);
		left->addLayout(controls);
		left->addWidget(zoom);
		left->addWidget(zoomLabel);
		controls->setAlignment(Qt::AlignTop);
		auto *rightBox = new QWidget(this);
		auto *right = new QHBoxLayout(rightBox);
		right->setContentsMargins(0, 0, 0, 0);
		right->addLayout(beatGrid);
		int side = std::max(leftBox->sizeHint().width(), rightBox->sizeHint().width());
		leftBox->setFixedWidth(side);
		rightBox->setFixedWidth(side);
		auto *layout = new QHBoxLayout(this);
		layout->setContentsMargins(1, 1, 1, 1);
		layout->setSpacing(3);
		layout->addWidget(leftBox);
		layout->addWidget(view, 1);
		layout->addWidget(rightBox);

		resetHotkey = obs_hotkey_register_frontend("vst_ex_waveline_reset_beat", T("Waveline.Hotkey.Reset").toUtf8().constData(),
							   onResetHotkey, this);
		loadHotkey();
		obs_frontend_add_save_callback(onSave, this);

		connect(&VdjLink::instance(), &VdjLink::changed, this, [this] { applyVdjMode(); });
		applyVdjMode();
		sync();
		updateBeat();
		connect(&timer, &QTimer::timeout, this, &WavelineDock::frame);
		timer.start(kFrameMs);
	}

	~WavelineDock() override
	{
		obs_frontend_remove_save_callback(onSave, this);
		obs_hotkey_unregister(resetHotkey); // after this no hotkey callback can still be running with `this`
		for (auto &track : owned)
			unhook(*track);
	}

private:
	QMenu *buildViewMenu(QWidget *parent)
	{
		auto *menu = new QMenu(parent);
		connect(menu->addAction(T("Waveline.Layers")), &QAction::triggered, this, [this] { showLayers(); });
		menu->addSeparator();
		auto *group = new QActionGroup(menu);
		for (bool stereo : {false, true}) {
			QAction *a = menu->addAction(T(stereo ? "Waveline.Style.Stereo" : "Waveline.Style.Normal"));
			a->setCheckable(true);
			a->setChecked(view->stereo == stereo);
			group->addAction(a);
			connect(a, &QAction::triggered, this, [this, stereo] {
				view->stereo = stereo;
				PluginMeta::instance().setUiValue("wavelineStereo", stereo);
			});
		}
		return menu;
	}

	// ---------- beat ----------

	static void onResetHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
	{
		if (!pressed)
			return;
		auto *dock = static_cast<WavelineDock *>(data);
		QMetaObject::invokeMethod(dock, [dock] { dock->resetBeat(); }, Qt::QueuedConnection); // hotkey thread
	}

	// Frontend hotkeys are not saved by OBS; keep the binding with the plugin's settings, written only on change.
	static void onSave(obs_data_t *, bool saving, void *data)
	{
		if (saving)
			static_cast<WavelineDock *>(data)->saveHotkey();
	}

	void saveHotkey()
	{
		OBSDataArrayAutoRelease keys = obs_hotkey_save(resetHotkey);
		OBSDataAutoRelease holder = obs_data_create();
		obs_data_set_array(holder, "keys", keys);
		QString json = QString::fromUtf8(obs_data_get_json(holder));
		auto &meta = PluginMeta::instance();
		if (meta.uiValue("wavelineResetHotkey").toString() != json)
			meta.setUiValue("wavelineResetHotkey", json);
	}

	void loadHotkey()
	{
		QString json = PluginMeta::instance().uiValue("wavelineResetHotkey").toString();
		if (json.isEmpty())
			return;
		OBSDataAutoRelease holder = obs_data_create_from_json(json.toUtf8().constData());
		if (!holder)
			return;
		OBSDataArrayAutoRelease keys = obs_data_get_array(holder, "keys");
		if (keys)
			obs_hotkey_load(resetHotkey, keys);
	}

	int64_t ownBeatNs() const { return int64_t(60e9 / bpmBox->value()); }
	static int64_t vdjBeatNs() { return int64_t(60e9 / VdjLink::instance().lastBpm); }

	// The beat nearest to now becomes beat 1, in both rows.
	void resetBeat()
	{
		auto now = int64_t(os_gettime_ns());
		anchorNs = now;
		VdjLink &link = VdjLink::instance();
		if (link.beatLive(now))
			vdjOffset = os2l_reset_offset(link.lastPos, link.lastBeatNs, now, vdjBeatNs());
		updateBeat();
	}

	void updateBeat()
	{
		auto now = int64_t(os_gettime_ns());
		VdjLink &link = VdjLink::instance();
		ownRow->on = beatsOn;
		ownRow->beat = beat_at_or_before(now, anchorNs, ownBeatNs());
		ownRow->update();
		bool live = link.enabled() && link.beatLive(now);
		int64_t vdjAnchor = live ? link.lastBeatNs - (link.lastPos - vdjOffset) * vdjBeatNs() : 0;
		vdjRow->on = live;
		if (live)
			vdjRow->beat = beat_at_or_before(now, vdjAnchor, vdjBeatNs());
		vdjRow->update();
		vdjBpm->setText(live ? QString::number(link.lastBpm, 'f', 1) : QStringLiteral("---"));
		if (live != wasLive) {
			wasLive = live;
			showVdjState();
		}
		// Dots under the waveform: VirtualDJ's beat while it plays, otherwise the upper row's.
		view->beatNs = !beatsOn ? 0 : live ? vdjBeatNs() : ownBeatNs();
		view->beatAnchor = live ? vdjAnchor : anchorNs;
		// The decks' tag, under the source names.
		view->decks = deckCount();
		QStringList chosen = chosenTags();
		bool showTag = link.enabled() && PluginMeta::instance().uiValue("wavelineVdjTag").toBool(false) &&
			       link.hasInfo() && !chosen.isEmpty();
		QString field = tagField();
		view->tagLabel = showTag ? tagFieldName(field) : QString();
		view->tagLabelChoices.clear();
		for (const QString &c : chosen)
			view->tagLabelChoices << tagFieldName(c);
		view->masterDeck = showTag ? link.masterDeck() : 0;
		for (int d = 1; d <= deckCount(); d++)
			// Deck's button off (or the deck empty): "-". Sending, but not this tag: its script is older than the
			// tag choice.
			view->deckText[d - 1] = !showTag || !link.deckLive(d) ? QString()
						: link.has(d, field)          ? link.value(d, field)
									      : T("Waveline.Vdj.TagMissing");
	}

	// The longest of the decks' button scripts; each must fit in a VirtualDJ button.
	static int longestScript(const QStringList &codes, int decks)
	{
		int longest = 0;
		for (int d = 1; d <= decks; d++)
			longest = std::max(longest, int(VdjLink::script(codes, d, decks).size()));
		return longest;
	}

	// How many VirtualDJ decks the user plays with (2 by default, as VirtualDJ's own layout); fewer decks leave room
	// in the script for more tags.
	static int deckCount() { return std::clamp(PluginMeta::instance().uiValue("wavelineVdjDecks").toInt(2), 2, VdjLink::kDecks); }

	// A tag counts as empty when no deck has anything in it.
	static bool tagEmpty(const QString &code)
	{
		VdjLink &link = VdjLink::instance();
		for (int d = 1; d <= deckCount(); d++)
			if (!link.value(d, code).isEmpty())
				return false;
		return true;
	}

	// Tags chosen to show, in TAG EDITOR order (Comment alone by default).
	static QStringList chosenTags()
	{
		QStringList saved;
		for (const QJsonValue &v : PluginMeta::instance().uiValue("wavelineVdjTags").toArray())
			saved << v.toString();
		QStringList list;
		for (const VdjField &f : VdjLink::fields())
			if (saved.contains(QLatin1String(f.code)))
				list << QString::fromLatin1(f.code);
		if (list.isEmpty() && !PluginMeta::instance().uiValue("wavelineVdjTags").isArray())
			list << QStringLiteral("cm");
		return list;
	}

	// The tag shown now; always one of the chosen ones.
	static QString tagField()
	{
		QStringList chosen = chosenTags();
		QString f = PluginMeta::instance().uiValue("wavelineVdjTagField").toString();
		return chosen.contains(f) ? f : chosen.value(0, QStringLiteral("cm"));
	}

	static QString tagFieldName(const QString &code) { return T(("Waveline.Vdj.Tag." + code).toUtf8().constData()); }

	// Click on the tag line: the next chosen tag, for all decks at once. With "skip empty" on, tags that no deck has
	// anything in are passed over; if every other one is empty the shown tag stays.
	void nextTag()
	{
		QStringList chosen = chosenTags();
		if (chosen.size() < 2)
			return;
		bool skipEmpty = PluginMeta::instance().uiValue("wavelineVdjSkipEmpty").toBool(true);
		int at = std::max(0, int(chosen.indexOf(tagField())));
		QString next = tagField();
		for (int step = 1; step < chosen.size(); step++) {
			const QString &f = chosen[(at + step) % chosen.size()];
			if (!skipEmpty || !tagEmpty(f)) {
				next = f;
				break;
			}
		}
		PluginMeta::instance().setUiValue("wavelineVdjTagField", next);
		updateBeat();
	}

	// "Set up VirtualDJ": everything in the order it is done, in one window. 1 VirtualDJ's settings, 2 decks and tags,
	// 3 the scripts to paste (rebuilt at once when 2 changes, so a stale copy is obvious), 4 how to use the buttons.
	void showVdjSetup()
	{
		if (setup) {
			setup->raise();
			return;
		}
		setup = new QDialog(this);
		setup->setAttribute(Qt::WA_DeleteOnClose);
		setup->setWindowTitle(T("Waveline.Vdj.Setup"));
		auto *outer = new QVBoxLayout(setup);
		auto *scroll = new QScrollArea(setup);
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		auto *page = new QWidget(scroll);
		auto *v = new QVBoxLayout(page);
		scroll->setWidget(page);
		outer->addWidget(scroll, 1);

		auto heading = [&](const QString &text) {
			auto *l = new QLabel(text, page);
			l->setStyleSheet("font-weight:bold;margin-top:8px;");
			l->setWordWrap(true);
			v->addWidget(l);
		};
		auto note = [&](const QString &text, QWidget *parent, QVBoxLayout *into) {
			auto *l = new QLabel(text, parent);
			l->setWordWrap(true);
			l->setTextInteractionFlags(Qt::TextSelectableByMouse);
			into->addWidget(l);
			return l;
		};
		// A read-only box with a copy button. `shown` differs from `code` when part of it must stay off screenshots.
		auto codeRow = [](const QString &code, const QString &shown, QWidget *parent, QVBoxLayout *into) {
			auto *box = new QPlainTextEdit(shown, parent);
			box->setReadOnly(true);
			box->setLineWrapMode(QPlainTextEdit::WidgetWidth);
			box->setFixedHeight(box->fontMetrics().lineSpacing() * (code.size() > 80 ? 5 : 1) + 12);
			auto *copy = new QPushButton(T("Waveline.Vdj.Setup.Copy"), parent);
			QObject::connect(copy, &QPushButton::clicked, copy, [code, copy] {
				QGuiApplication::clipboard()->setText(code);
				copy->setText(T("Waveline.Vdj.Setup.Copied"));
			});
			auto *row = new QHBoxLayout();
			row->addWidget(box, 1);
			row->addWidget(copy, 0, Qt::AlignTop);
			into->addLayout(row);
		};

		// 1. VirtualDJ's settings. The port is never drawn: dots on screen, the real address in the copy.
		heading(T("Waveline.Vdj.Setup.Head1"));
		note(T("Waveline.Vdj.Setup.Step1"), page, v);
		codeRow(QStringLiteral("127.0.0.1:%1").arg(VdjLink::instance().port()), QStringLiteral("127.0.0.1:••••"),
			page, v);

		// 2. Decks and tags.
		heading(T("Waveline.Vdj.Setup.Head2"));
		note(T("Waveline.Vdj.Setup.Step2"), page, v);
		auto *deckRow = new QHBoxLayout();
		deckRow->addWidget(new QLabel(T("Waveline.Vdj.Tags.Decks"), page));
		auto *decks = new QComboBox(page);
		for (int n = 2; n <= VdjLink::kDecks; n++)
			decks->addItem(QString::number(n), n);
		decks->setCurrentIndex(deckCount() - 2);
		deckRow->addWidget(decks);
		deckRow->addStretch(1);
		v->addLayout(deckRow);
		auto *grid = new QGridLayout();
		QStringList chosen = chosenTags();
		const auto &fields = VdjLink::fields();
		int rows = int((fields.size() + 3) / 4);
		auto boxes = std::make_shared<std::vector<std::pair<QString, QCheckBox *>>>();
		for (int i = 0; i < int(fields.size()); i++) {
			QString code = QString::fromLatin1(fields[i].code);
			auto *box = new QCheckBox(tagFieldName(code), page);
			box->setToolTip(QString::fromLatin1(fields[i].label)); // VirtualDJ's own name in its TAG EDITOR
			box->setChecked(chosen.contains(code));
			boxes->push_back({code, box});
			grid->addWidget(box, i % rows, i / rows); // down the columns, like reading the list
		}
		v->addLayout(grid);
		auto *room = new QLabel(page);
		room->setWordWrap(true);
		v->addWidget(room);
		auto *skip = new QCheckBox(T("Waveline.Vdj.Tags.SkipEmpty"), page);
		skip->setChecked(PluginMeta::instance().uiValue("wavelineVdjSkipEmpty").toBool(true));
		connect(skip, &QCheckBox::toggled, this,
			[](bool on) { PluginMeta::instance().setUiValue("wavelineVdjSkipEmpty", on); });
		v->addWidget(skip);

		// 3. The scripts, one per deck's button; rebuilt whenever 2 changes.
		heading(T("Waveline.Vdj.Setup.Head3"));
		note(T("Waveline.Vdj.Setup.Step3"), page, v);
		auto *scripts = new QWidget(page);
		auto *scriptsLayout = new QVBoxLayout(scripts);
		scriptsLayout->setContentsMargins(0, 0, 0, 0);
		v->addWidget(scripts);

		// 4. Using it.
		heading(T("Waveline.Vdj.Setup.Head4"));
		note(T("Waveline.Vdj.Setup.Step4"), page, v);
		v->addStretch(1);

		auto refresh = [boxes, room, scripts, scriptsLayout, codeRow] {
			QStringList list = chosenTags();
			int n = deckCount();
			// What still fits in a VirtualDJ button; a tag that would not fit cannot be ticked.
			int used = longestScript(list, n), more = 0;
			for (const auto &[code, box] : *boxes) {
				bool fits = longestScript(QStringList(list) << code, n) <= VdjLink::kMaxScript;
				box->setEnabled(box->isChecked() || fits);
				more += !box->isChecked() && fits;
			}
			bool over = used > VdjLink::kMaxScript;
			// How often each deck updates: the buttons slow down with more decks and tags (VdjLink::intervalMs).
			QString every = T("Waveline.Vdj.Tags.Every").arg(VdjLink::intervalMs(list, n) / 1000.0, 0, 'f', 1);
			room->setText(over ? T("Waveline.Vdj.Tags.Over").arg(used).arg(VdjLink::kMaxScript)
					   : T("Waveline.Vdj.Tags.Room").arg(used).arg(VdjLink::kMaxScript).arg(more) + "\n" + every);
			room->setStyleSheet(over ? "color:#ef5350;" : QString());
			// The scripts, new: copy buttons start over, so a script copied before the change is not mistaken.
			while (QLayoutItem *item = scriptsLayout->takeAt(0))
				delete item; // the rows' layouts; their widgets are children of `scripts`
			qDeleteAll(scripts->findChildren<QWidget *>(Qt::FindDirectChildrenOnly));
			for (int d = 1; d <= n; d++) {
				QString script = VdjLink::script(list, d, n);
				if (script.isEmpty())
					continue;
				QString role = d == 1 ? T("Waveline.Vdj.Setup.Button.Key")
					       : d == 2 ? T("Waveline.Vdj.Setup.Button.Master")
							: QString();
				auto *caption = new QLabel(T("Waveline.Vdj.Setup.Button").arg(d) + role, scripts);
				scriptsLayout->addWidget(caption);
				codeRow(script, script, scripts, scriptsLayout);
			}
		};
		for (const auto &[code, box] : *boxes) {
			connect(box, &QCheckBox::toggled, this, [code, refresh](bool on) {
				QStringList list = chosenTags();
				if (on && !list.contains(code))
					list << code;
				if (!on)
					list.removeAll(code);
				PluginMeta::instance().setUiValue("wavelineVdjTags", QJsonArray::fromStringList(list));
				refresh();
			});
		}
		connect(decks, &QComboBox::currentIndexChanged, this, [decks, refresh] {
			PluginMeta::instance().setUiValue("wavelineVdjDecks", decks->currentData().toInt());
			refresh();
		});
		refresh();

		auto *close = new QDialogButtonBox(QDialogButtonBox::Close, setup);
		connect(close, &QDialogButtonBox::rejected, setup, &QDialog::close);
		outer->addWidget(close);
		setup->resize(640, 760);
		setup->show();
	}

	// ---------- VirtualDJ (OS2L, see VdjLink) ----------

	void applyVdjMode()
	{
		bool on = VdjLink::instance().enabled();
		vdjRow->setVisible(on);
		vdjSide->setVisible(on);
		showVdjState();
	}

	void showVdjState()
	{
		VdjLink &link = VdjLink::instance();
		vdjButton->setToolTip(link.stateText() + "\n" + T("Waveline.Vdj.Tip"));
		vdjButton->setStyleSheet("QToolButton{padding:0;font-size:10px;color:" + link.stateColor() +
					 ";}QToolButton::menu-indicator{image:none;}");
	}

	void fillVdjMenu(QMenu *menu)
	{
		VdjLink &link = VdjLink::instance();
		menu->clear();
		for (const QString &line : vdjButton->toolTip().split('\n'))
			menu->addAction(line)->setEnabled(false);
		menu->addSeparator();
		connect(menu->addAction(T("Waveline.Vdj.Setup") + "..."), &QAction::triggered, this, [this] { showVdjSetup(); });
		connect(menu->addAction(T("Waveline.Vdj.Port")), &QAction::triggered, this, [this] {
			VdjLink &l = VdjLink::instance();
			bool ok = false;
			int port = QInputDialog::getInt(this, T("Waveline.Vdj.PortTitle"), T("Waveline.Vdj.PortLabel"), l.port(),
							1024, 65535, 1, &ok);
			if (ok)
				l.setPort(port);
		});
	}

	// ---------- waveform ----------

	void showZoom(int v)
	{
		zoomLabel->setText(QString::number(v) + "%");
		zoomLabel->setToolTip(T("Waveline.Span").arg(spanSeconds(v), 0, 'f', 1));
	}

	void readButtons()
	{
		mask = 0;
		for (int t = 0; t < 6; t++)
			if (trackButtons[t]->isChecked())
				mask |= 1u << t;
		followStream = false; // from now on the user's choice stays
		PluginMeta::instance().setUiValue("wavelineTracks", int(mask));
		sync();
	}

	void frame()
	{
		if (!isVisible())
			return;
		if (++frames >= kSyncFrames) {
			frames = 0;
			sync();
			showVdjState(); // playing / stopped changes without a message
		}
		updateBeat();
		view->update();
	}

	OBSSource resolve(const QString &id)
	{
		if (id == kMasterId)
			return masterbus::source();
		OBSSourceAutoRelease s = obs_get_source_by_uuid(id.toUtf8().constData());
		return s.Get();
	}

	void unhook(Track &track)
	{
		if (!track.hooked)
			return;
		track.hooked = false;
		OBSSourceAutoRelease s = obs_weak_source_get_source(track.weak);
		if (s)
			obs_source_remove_audio_capture_callback(s, onAudio, &track);
	}

	Track *trackFor(const QString &id)
	{
		for (auto &t : owned)
			if (t->id == id)
				return t.get();
		auto track = std::make_unique<Track>();
		track->id = id;
		QJsonObject style = styles[id].toObject();
		track->color = style["color"].toInt(freeColor());
		track->opacity = std::clamp(style["opacity"].toInt(100), kMinOpacity, 100);
		owned.push_back(std::move(track));
		saveStyle(*owned.back());
		return owned.back().get();
	}

	// The colour stays with the source when it changes layer.
	int freeColor() const
	{
		for (int c = 0; c < int(std::size(kPalette)); c++)
			if (std::none_of(owned.begin(), owned.end(), [&](const auto &t) { return t->color == c && wanted.contains(t->id); }))
				return c;
		return int(owned.size());
	}

	void saveStyle(const Track &track)
	{
		styles[track.id] = QJsonObject{{"color", track.color}, {"opacity", track.opacity}};
		PluginMeta::instance().setUiValue("wavelineStyles", styles);
	}

	void saveSources()
	{
		PluginMeta::instance().setUiValue("wavelineSources", QJsonArray::fromStringList(wanted));
		sync();
		fillLayers();
	}

	void showLayers()
	{
		if (!layers) {
			layers = new QDialog(this);
			layers->setWindowTitle(T("Waveline.Layers"));
			layers->resize(360, 10);
		}
		fillLayers();
		layers->show();
		layers->raise();
	}

	// Top layer first, the way it looks on screen.
	void fillLayers()
	{
		if (!layers)
			return;
		delete layers->layout();
		qDeleteAll(layers->findChildren<QWidget *>(Qt::FindDirectChildrenOnly));
		auto *grid = new QGridLayout(layers);
		grid->setColumnStretch(1, 1);
		grid->setColumnStretch(2, 2);
		if (tracks.empty())
			grid->addWidget(new QLabel(T("Waveline.Empty"), layers), 0, 0);
		for (int row = 0; row < int(tracks.size()); row++) {
			int index = int(tracks.size()) - 1 - row;
			Track *track = tracks[index];
			auto *swatch = new QLabel(layers);
			swatch->setFixedSize(12, 12);
			swatch->setStyleSheet("background:" + kPalette[track->color % std::size(kPalette)].name() + ";");
			auto *opacity = new QSlider(Qt::Horizontal, layers);
			opacity->setRange(kMinOpacity, 100);
			opacity->setValue(track->opacity);
			opacity->setToolTip(T("Waveline.Opacity"));
			auto *percent = new QLabel(QString::number(track->opacity) + "%", layers);
			percent->setFixedWidth(36);
			connect(opacity, &QSlider::valueChanged, this, [this, track, percent](int v) {
				track->opacity = v;
				percent->setText(QString::number(v) + "%");
				saveStyle(*track);
			});
			auto *up = new QToolButton(layers);
			up->setText(QStringLiteral("▲"));
			up->setEnabled(row > 0);
			auto *down = new QToolButton(layers);
			down->setText(QStringLiteral("▼"));
			down->setEnabled(index > 0);
			connect(up, &QToolButton::clicked, this, [this, index] { moveLayer(index, index + 1); });
			connect(down, &QToolButton::clicked, this, [this, index] { moveLayer(index, index - 1); });
			grid->addWidget(swatch, row, 0);
			grid->addWidget(new QLabel(track->name, layers), row, 1);
			grid->addWidget(opacity, row, 2);
			grid->addWidget(percent, row, 3);
			grid->addWidget(up, row, 4);
			grid->addWidget(down, row, 5);
		}
	}

	// Deferred: the clicked button is deleted when the rows are rebuilt.
	void moveLayer(int from, int to)
	{
		QTimer::singleShot(0, this, [this, from, to] {
			if (from < 0 || to < 0 || from >= int(tracks.size()) || to >= int(tracks.size()))
				return;
			wanted.swapItemsAt(wanted.indexOf(tracks[from]->id), wanted.indexOf(tracks[to]->id));
			saveSources();
		});
	}

	// Hooks what is wanted and exists. Track objects live as long as the dock: a capture callback may still be
	// running on an audio thread at the moment its source goes away.
	void sync()
	{
		if (followStream)
			mask = 1u << (masterbus::onAirTrack() - 1);
		for (int t = 0; t < 6; t++)
			trackButtons[t]->setChecked(mask & (1u << t));

		tracks.clear();
		for (auto &t : owned)
			if (!wanted.contains(t->id))
				unhook(*t);
		for (const QString &id : wanted) {
			OBSSource source = resolve(id);
			if (!source)
				continue;
			Track *track = trackFor(id);
			OBSSourceAutoRelease hookedTo = obs_weak_source_get_source(track->weak);
			if (track->hooked && hookedTo.Get() != source.Get())
				unhook(*track);
			if (!track->hooked) {
				track->weak = OBSGetWeakRef(source);
				obs_source_add_audio_capture_callback(source, onAudio, track);
				track->hooked = true;
			}
			track->name = QString::fromUtf8(obs_source_get_name(source));
			bool routed = obs_source_get_audio_mixers(source) & mask;
			if (id == kMasterId)
				track->onAir = masterbus::onStream() && !obs_source_muted(source);
			else
				track->onAir = routed && !obs_source_muted(source) && obs_source_active(source);
			tracks.push_back(track);
		}
	}

	void fillMenu(QMenu *menu)
	{
		menu->clear();
		auto add = [this, menu](const QString &id, const QString &name) {
			QAction *a = menu->addAction(name);
			a->setCheckable(true);
			a->setChecked(wanted.contains(id));
			connect(a, &QAction::toggled, this, [this, id](bool on) {
				if (on)
					wanted << id;
				else
					wanted.removeAll(id);
				saveSources();
			});
		};
		if (masterbus::source()) {
			add(kMasterId, QStringLiteral("VST Ex Master"));
			menu->addSeparator();
		}
		std::vector<OBSSource> sources;
		obs_enum_sources(
			[](void *p, obs_source_t *s) {
				if (obs_source_get_output_flags(s) & OBS_SOURCE_AUDIO)
					static_cast<std::vector<OBSSource> *>(p)->emplace_back(s);
				return true;
			},
			&sources);
		for (const OBSSource &s : sources)
			add(QString::fromUtf8(obs_source_get_uuid(s)), QString::fromUtf8(obs_source_get_name(s)));
	}

	WaveView *view;
	BeatRow *ownRow, *vdjRow;
	QDoubleSpinBox *bpmBox;
	QLabel *vdjBpm;
	QWidget *vdjSide;
	QToolButton *vdjButton = nullptr;
	obs_hotkey_id resetHotkey = OBS_INVALID_HOTKEY_ID;
	bool beatsOn = false, wasLive = false;
	int64_t anchorNs = 0;
	int64_t vdjOffset = 0; // VirtualDJ's beat number minus this = the lower row's beat
	QLabel *zoomLabel;
	QToolButton *trackButtons[6];
	QStringList wanted; // shown sources, by uuid; the order is the layer order, bottom first
	QJsonObject styles; // colour and opacity by source, kept when a source is hidden
	QPointer<QDialog> layers, setup;
	std::deque<std::unique_ptr<Track>> owned;
	std::vector<Track *> tracks; // those showing now
	uint32_t mask = 0;
	bool followStream = true; // Tr buttons follow the stream until the user clicks one
	int frames = 0;
	QTimer timer;
};

} // namespace

void install_waveline_dock()
{
	obs_frontend_add_dock_by_id(kDockId, "VST Ex WaveLine", new WavelineDock());
}

void remove_waveline_dock()
{
	obs_frontend_remove_dock(kDockId);
}
