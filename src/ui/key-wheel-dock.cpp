#include "ui/key-wheel-dock.h"

#include "dsp/key-wheel.h"
#include "plugin-meta.h"
#include "ui/ui-text.h"
#include "vdj-link.h"

#include <obs-frontend-api.h>
#include <util/platform.h>

#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QWidget>

#include <cmath>

namespace {

constexpr const char *kDockId = "vst-ex-key-wheel-dock";
constexpr int kFrameMs = 33;
constexpr double kEase = 0.18; // share of the remaining turn done per frame
bool installed = false;

// Camelot colours: a hue per number around the wheel, as on the usual printed wheels (1 green ... 7 red ... 10 blue).
QColor segmentColor(int number, int level) // level 0: other keys, 1: mixes well, 2: playing
{
	int hue = (150 - (number - 1) * 30 + 720) % 360;
	switch (level) {
	case 2:
		return QColor::fromHsv(hue, 210, 240);
	case 1:
		return QColor::fromHsv(hue, 180, 170);
	default:
		return QColor::fromHsv(hue, 110, 80);
	}
}

// Outer ring minor (A), inner ring major (B). The wheel turns so the playing key is at the top.
class KeyWheel : public QWidget {
public:
	KeyWheel() : QWidget(nullptr)
	{
		setMinimumSize(140, 140);
		connect(&timer, &QTimer::timeout, this, [this] { tick(); });
		timer.start(kFrameMs);
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.fillRect(rect(), palette().color(QPalette::Window));
		double side = std::min(width(), height()) - 6.0;
		QPointF c(width() / 2.0, height() / 2.0);
		double r = side / 2.0, rMid = r * 0.66, rHole = r * 0.36;
		auto ringRect = [&](double radius) { return QRectF(c.x() - radius, c.y() - radius, 2 * radius, 2 * radius); };
		const QColor gap = palette().color(QPalette::Window);

		for (bool minor : {true, false}) {
			double outer = minor ? r : rMid, inner = minor ? rMid : rHole;
			for (int n = 1; n <= 12; n++) {
				CamelotKey k{n, minor};
				int level = k == key ? 2 : keys_compatible(k, key) ? 1 : 0;
				double theta = (n - 1) * 30.0 - angle; // clockwise from the top
				double start = 90.0 - theta - 15.0;   // Qt: counter-clockwise from 3 o'clock
				QPainterPath path;
				path.arcMoveTo(ringRect(outer), start);
				path.arcTo(ringRect(outer), start, 30.0);
				path.arcTo(ringRect(inner), start + 30.0, -30.0);
				path.closeSubpath();
				p.setPen(QPen(gap, 1.5));
				p.setBrush(segmentColor(n, level));
				p.drawPath(path);
				// Labels stay upright: Camelot number, and the key name under it.
				double rad = theta * 3.14159265358979 / 180.0, mid = (outer + inner) / 2.0;
				QPointF at(c.x() + mid * std::sin(rad), c.y() - mid * std::cos(rad));
				QColor ink = level ? QColor(255, 255, 255) : QColor(205, 205, 205);
				QFont big = font();
				big.setPixelSize(std::max(8, int(r * 0.10)));
				big.setBold(true);
				QFont small = font();
				small.setPixelSize(std::max(7, int(r * 0.075)));
				double h = r * 0.09;
				p.setPen(ink);
				p.setFont(big);
				p.drawText(QRectF(at.x() - r * 0.2, at.y() - h, r * 0.4, h), Qt::AlignHCenter | Qt::AlignBottom,
					   QString::number(n) + (minor ? "A" : "B"));
				p.setFont(small);
				p.drawText(QRectF(at.x() - r * 0.2, at.y(), r * 0.4, h), Qt::AlignHCenter | Qt::AlignTop,
					   QString::fromUtf8(key_name(k)));
			}
		}
		// Outline the keys that mix well, then the playing one, so the outlines are never covered.
		for (int pass = 1; pass <= 2; pass++)
			for (bool minor : {true, false})
				for (int n = 1; n <= 12; n++) {
					CamelotKey k{n, minor};
					if ((pass == 2) != (k == key) || (pass == 1 && !keys_compatible(k, key)))
						continue;
					double outer = minor ? r : rMid, inner = minor ? rMid : rHole;
					double start = 90.0 - ((n - 1) * 30.0 - angle) - 15.0;
					QPainterPath path;
					path.arcMoveTo(ringRect(outer - 1.5), start);
					path.arcTo(ringRect(outer - 1.5), start, 30.0);
					path.arcTo(ringRect(inner + 1.5), start + 30.0, -30.0);
					path.closeSubpath();
					p.setBrush(Qt::NoBrush);
					p.setPen(QPen(QColor(255, 255, 255, pass == 2 ? 255 : 150), pass == 2 ? 3.0 : 1.5));
					p.drawPath(path);
				}

		// Centre: the playing key, big; or a dash while VirtualDJ sends none.
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(30, 30, 30));
		p.drawEllipse(c, rHole - 2, rHole - 2);
		QFont f = font();
		f.setBold(true);
		if (key.valid()) {
			f.setPixelSize(std::max(10, int(rHole * 0.55)));
			p.setFont(f);
			p.setPen(segmentColor(key.number, 2));
			p.drawText(QRectF(c.x() - rHole, c.y() - rHole * 0.75, 2 * rHole, rHole * 0.9), Qt::AlignCenter,
				   QString::fromUtf8(key_name(key)));
			f.setPixelSize(std::max(8, int(rHole * 0.28)));
			p.setFont(f);
			p.setPen(QColor(220, 220, 220));
			p.drawText(QRectF(c.x() - rHole, c.y() + rHole * 0.15, 2 * rHole, rHole * 0.5), Qt::AlignCenter,
				   QString::number(key.number) + (key.minor ? "A" : "B"));
		} else {
			f.setPixelSize(std::max(8, int(rHole * 0.2)));
			p.setFont(f);
			p.setPen(QColor(150, 150, 150));
			p.drawText(QRectF(c.x() - rHole * 0.9, c.y() - rHole * 0.6, 1.8 * rHole, 1.2 * rHole),
				   Qt::AlignCenter | Qt::TextWordWrap, T("KeyWheel.Waiting"));
		}
	}

private:
	void tick()
	{
		if (!isVisible())
			return;
		VdjLink &link = VdjLink::instance();
		CamelotKey now = link.hasInfo() ? parse_key(link.key().toStdString()) : CamelotKey{};
		bool repaint = !(now == key);
		key = now;
		if (key.valid()) {
			double target = (key.number - 1) * 30.0;
			double diff = std::remainder(target - angle, 360.0); // the short way round
			if (std::fabs(diff) > 0.05) {
				angle = std::fmod(angle + (std::fabs(diff) < 0.5 ? diff : diff * kEase) + 360.0, 360.0);
				repaint = true;
			}
		}
		if (repaint)
			update();
	}

	QTimer timer;
	CamelotKey key;
	double angle = 0.0; // degrees the wheel is turned: number 1 at the top at 0
};

} // namespace

bool key_wheel_enabled()
{
	auto &meta = PluginMeta::instance();
	return meta.uiValue("wavelineVdj").toBool(false) && meta.uiValue("keyWheel").toBool(false);
}

void apply_key_wheel_dock()
{
	bool want = key_wheel_enabled();
	if (want && !installed) {
		installed = obs_frontend_add_dock_by_id(kDockId, "VST Ex Key Wheel", new KeyWheel());
	} else if (!want && installed) {
		obs_frontend_remove_dock(kDockId);
		installed = false;
	}
}

void remove_key_wheel_dock()
{
	if (installed)
		obs_frontend_remove_dock(kDockId);
	installed = false;
}
