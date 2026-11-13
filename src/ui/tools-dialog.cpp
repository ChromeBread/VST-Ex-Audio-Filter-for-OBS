#include "ui/tools-dialog.h"

#include "master-bus.h"
#include "plugin-meta.h"
#include "ui/backup.h"
#include "loader-api.h"
#include "log-file.h"
#include "ui/cb-menu.h"
#include "ui/chain-view.h"
#include "ui/master-dock.h"
#include "ui/ui-text.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

struct Language {
	const char *code;
	const char *label;
};
const Language kLanguages[] = {{"en-US", "EN  English"},          {"ja-JP", "JP  日本語"},
			       {"ko-KR", "KR  한국어"},             {"id-ID", "ID  Bahasa Indonesia"},
			       {"fr-FR", "FR  Français"},         {"zh-TW", "TW  繁體中文"}};

QPointer<ToolsDialog> openDialog;

// Saved choice, else OBS's language when it is one of ours, else English.
QString currentLanguage()
{
	QString saved = PluginMeta::instance().uiValue("language").toString();
	if (!saved.isEmpty())
		return saved;
	QString obs = QString::fromUtf8(obs_get_locale());
	for (const Language &l : kLanguages)
		if (obs == l.code)
			return obs;
	return "en-US";
}

QString native(const QString &path)
{
	return QDir::toNativeSeparators(QDir::cleanPath(path));
}

QLabel *selectable(const QString &text)
{
	auto *label = new QLabel(text);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	label->setWordWrap(true);
	return label;
}

QString binaryDir()
{
	return QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).absolutePath();
}

QString configDir()
{
	char *p = obs_module_config_path("");
	QString path = QString::fromUtf8(p);
	bfree(p);
	return path;
}

} // namespace

static void showDock(const char *id)
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (auto *dock = main->findChild<QDockWidget *>(id)) {
		dock->setVisible(true);
		dock->raise();
	}
}

static void showFilterHint(const char *nameKey)
{
	QMessageBox::information(static_cast<QWidget *>(obs_frontend_get_main_window()), T(nameKey),
				 T("CbMenu.FilterHint").arg(T(nameKey)));
}

// frontend-tools adds the action with its own translation of "Scripts", so ask that module for the text.
static void openScriptsWindow()
{
	const char *text = nullptr;
	obs_module_t *tools = obs_get_module("frontend-tools");
	if (!tools || !obs_module_get_locale_string(tools, "Scripts", &text))
		return;
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto *menu = main->findChild<QMenu *>("menuTools");
	if (!menu)
		return;
	for (QAction *action : menu->actions()) {
		if (action->text() == QString::fromUtf8(text)) {
			action->trigger();
			return;
		}
	}
}

// Returns the heading and its separator, so a heading whose items are all hidden can be hidden too.
static QList<QAction *> cbHeading(int order, const char *key)
{
	QMenu *menu = cb_menu();
	QList<QAction *> actions;
	if (order > 100) {
		auto *separator = new QAction(menu);
		separator->setSeparator(true);
		cb_menu_insert(separator, order - 1);
		actions << separator;
	}
	auto *heading = new QAction(T(key), menu);
	heading->setEnabled(false); // a label: QMenu::addSection draws no text in some themes
	cb_menu_insert(heading, order);
	actions << heading;
	return actions;
}

// #cbOBScripts Lua scripts. A new script gets one line here: menu order (1xx = Scripts, 3xx = Custom Docks),
// the name shown, and its .lua file name(s) - older names too, so an old install is still found.
struct LuaScript {
	int order;
	const char *name;
	QStringList files;
};
static const QList<LuaScript> &luaScripts()
{
	static const QList<LuaScript> list = {
		{110, "Audio Device Restorer", {"audio_device_restorer.lua"}},
		{120, "Font Bridge", {"obs_font_bridge.lua", "fontbridge_text.lua"}},
		{310, "Image Resize Reader", {"obs_image_resize_reader.lua"}},
		{320, "Source Resizer", {"obs_source_resizer.lua"}},
	};
	return list;
}

// File names (lower case) of the scripts added in Tools > Scripts of any scene collection whose file still exists.
// Lua scripts have no menu API, so the saved scene collections are the only place that says which are installed.
// shortcut: a script added in this session shows up once OBS has saved the scene collection (scene change,
// exit, ...), which is good enough for a menu; upgrade to a signal from each script if that ever matters.
static QSet<QString> registeredLuaScripts()
{
	struct Cached {
		QDateTime modified;
		QSet<QString> files;
	};
	static QHash<QString, Cached> cache; // parsed once per file version: collections can be several MB
	QDir scenes(QDir(configDir()).absoluteFilePath("../../basic/scenes")); // works in portable mode too
	QSet<QString> found;
	for (const QFileInfo &info : scenes.entryInfoList({"*.json"}, QDir::Files)) {
		Cached &entry = cache[info.absoluteFilePath()];
		if (entry.modified != info.lastModified()) {
			entry.modified = info.lastModified();
			entry.files.clear();
			QFile f(info.absoluteFilePath());
			if (f.open(QIODevice::ReadOnly)) {
				const QJsonArray scripts =
					QJsonDocument::fromJson(f.readAll()).object()["modules"].toObject()["scripts-tool"].toArray();
				for (const QJsonValue &s : scripts) {
					QString path = s.toObject()["path"].toString();
					if (!path.isEmpty() && QFileInfo::exists(path))
						entry.files.insert(QFileInfo(path).fileName().toLower());
				}
			}
		}
		found.unite(entry.files);
	}
	return found;
}

void install_tools_menu()
{
	PluginMeta &meta = PluginMeta::instance();
	bool chainView = meta.uiValue("featureChainView").toBool(true);
	bool mixer = meta.uiValue("featureMixer").toBool(true);
	bool switcher = meta.uiValue("featureSwitcher").toBool(true);
	bool waveline = meta.uiValue("featureWaveline").toBool(true);

	// Lua scripts have no API for menu items, so these lead to the Scripts window where each one is managed.
	// Only the installed ones are shown, checked again each time the menu opens.
	QList<QAction *> scriptsHeading = cbHeading(100, "CbMenu.Scripts");
	QList<QAction *> customDocksHeading = cbHeading(300, "CbMenu.CustomDocks");
	QList<QPair<QAction *, QStringList>> luaActions;
	for (const LuaScript &s : luaScripts()) {
		auto *action = new QAction(QString::fromUtf8(s.name), cb_menu());
		QObject::connect(action, &QAction::triggered, action, [] { openScriptsWindow(); });
		cb_menu_insert(action, s.order);
		luaActions.append({action, s.files});
	}
	QObject::connect(cb_menu(), &QMenu::aboutToShow, cb_menu(), [=] {
		QSet<QString> installed = registeredLuaScripts();
		bool anyScripts = false, anyCustomDocks = false;
		for (const auto &[action, files] : luaActions) {
			bool on = std::any_of(files.begin(), files.end(), [&](const QString &f) { return installed.contains(f); });
			action->setVisible(on);
			bool customDock = action->property("cbOrder").toInt() >= 300;
			(customDock ? anyCustomDocks : anyScripts) |= on;
		}
		for (QAction *a : scriptsHeading)
			a->setVisible(anyScripts);
		for (QAction *a : customDocksHeading)
			a->setVisible(anyCustomDocks);
	});

	if (mixer || switcher || waveline || masterbus::unlocked())
		cbHeading(200, "CbMenu.Docks");
	if (mixer)
		cb_menu_add(210, "VST Ex Mixer", [] { showDock("vst-ex-fx-dock"); });
	if (switcher)
		cb_menu_add(220, "VST Ex Switcher", [] { showDock("vst-ex-switch-dock"); });
	if (waveline)
		cb_menu_add(230, "VST Ex WaveLine", [] { showDock("vst-ex-waveline-dock"); });
	if (masterbus::unlocked())
		cb_menu_add(240, "VST Ex Master", [] { showDock("vst-ex-master-dock"); });

	cbHeading(400, "CbMenu.Filters");
	cb_menu_add(410, "VST Ex Audio Filter", [] { ToolsDialog::open(); });
	if (chainView)
		cb_menu_add(430, "VST Ex Chain View", [] { open_chain_view(); });
	cb_menu_add(440, T("Pitch.Name"), [] { showFilterHint("Pitch.Name"); });
	cb_menu_add(450, T("Stereo.Name"), [] { showFilterHint("Stereo.Name"); });

	// First run: ask for the data folder once OBS is up, so it is not forgotten.
	obs_frontend_add_event_callback(
		[](enum obs_frontend_event event, void *) {
			if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && !PluginMeta::instance().dataDirChosen())
				QTimer::singleShot(0, [] {
					choose_data_folder(static_cast<QWidget *>(obs_frontend_get_main_window()), true);
				});
		},
		nullptr);
}

bool choose_data_folder(QWidget *parent, bool firstRun)
{
	auto &meta = PluginMeta::instance();
	QDialog dlg(parent);
	dlg.setWindowTitle(T("Tools.DataDir"));
	auto *help = new QLabel(firstRun ? T("Tools.DataDir.FirstRun") : T("Tools.DataDir.Help"), &dlg);
	help->setWordWrap(true);
	auto *path = new QLineEdit(native(meta.dataDir()), &dlg);
	auto *browse = new QPushButton(T("Browse"), &dlg);
	QObject::connect(browse, &QPushButton::clicked, &dlg, [&dlg, path] {
		QString dir = QFileDialog::getExistingDirectory(&dlg, T("Tools.DataDir"), path->text());
		if (!dir.isEmpty())
			path->setText(native(dir));
	});
	auto *row = new QHBoxLayout();
	row->addWidget(path, 1);
	row->addWidget(browse);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	auto *layout = new QVBoxLayout(&dlg);
	layout->addWidget(help);
	layout->addLayout(row);
	layout->addWidget(buttons);
	dlg.resize(560, dlg.sizeHint().height());

	if (dlg.exec() != QDialog::Accepted) {
		if (firstRun) { // keep the default and don't ask again; it can be changed in Tools
			meta.setDataDir(meta.dataDir());
			loader_set_data_dir(meta.dataDir()); // one data folder for both modules
		}
		return false;
	}
	QString dir = QDir::fromNativeSeparators(path->text().trimmed());
	if (dir.isEmpty() || !QDir().mkpath(dir)) {
		QMessageBox::warning(parent, T("Tools.DataDir"), T("Tools.DataDir.Failed").arg(path->text()));
		return false;
	}
	if (!meta.setDataDir(dir)) {
		QMessageBox::warning(parent, T("Tools.DataDir"), T("Tools.DataDir.Failed").arg(path->text()));
		return false;
	}
	loader_set_data_dir(dir);
	return true;
}

void apply_saved_language()
{
	QString lang = PluginMeta::instance().uiValue("language").toString();
	if (!lang.isEmpty())
		obs_module_set_locale(lang.toUtf8().constData());
}

void ToolsDialog::open()
{
	if (openDialog) {
		openDialog->raise();
		openDialog->activateWindow();
		return;
	}
	openDialog = new ToolsDialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
	openDialog->show();
}

ToolsDialog::ToolsDialog(QWidget *parent) : QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(T("Tools.Title"));
	resize(720, 420);
	setMaximumSize(1280, 720); // debug mode included, it must fit a 1280x720 screen
	auto &meta = PluginMeta::instance();

	auto *language = new QComboBox(this);
	for (const Language &l : kLanguages)
		language->addItem(QString::fromUtf8(l.label), QString::fromLatin1(l.code));
	language->setCurrentIndex(qMax(0, language->findData(currentLanguage())));
	connect(language, &QComboBox::activated, this, [this, language] {
		QString code = language->currentData().toString();
		PluginMeta::instance().setUiValue("language", code);
		loader_set_ui("language", code);
		obs_module_set_locale(code.toUtf8().constData());
		// Reopen so this window is in the new language too.
		close();
		QTimer::singleShot(0, [] { ToolsDialog::open(); });
	});
	auto *languageNote = new QLabel(T("Tools.Language.Note"), this);
	languageNote->setEnabled(false);

	auto *hiddenBoth = new QCheckBox(T("Tools.HiddenBoth"), this);
	hiddenBoth->setToolTip(T("Tools.HiddenBoth.Tip"));
	hiddenBoth->setChecked(meta.uiValue("hidePluginRows").toBool(false) && meta.uiValue("pickerHidden").toBool(true));
	connect(hiddenBoth, &QCheckBox::toggled, this, [](bool on) {
		PluginMeta::instance().setUiValue("hidePluginRows", on);
		PluginMeta::instance().setUiValue("pickerHidden", on);
		loader_set_ui("pickerHidden", on);
	});

	auto *features = new QGroupBox(T("Tools.Features"), this);
	auto *featureLayout = new QVBoxLayout(features);
	for (auto [key, text] : {std::pair{"featureChainView", "Tools.Feature.ChainView"},
				 {"featureMixer", "Tools.Feature.Mixer"},
				 {"featureSwitcher", "Tools.Feature.Switcher"},
				 {"featureWaveline", "Tools.Feature.Waveline"}}) {
		auto *box = new QCheckBox(T(text), features);
		box->setChecked(meta.uiValue(key).toBool(true));
		connect(box, &QCheckBox::toggled, this, [key](bool on) { PluginMeta::instance().setUiValue(key, on); });
		featureLayout->addWidget(box);
	}
	auto *featureNote = new QLabel(T("Tools.Features.Note"), features);
	featureNote->setEnabled(false);
	featureLayout->addWidget(featureNote);

	auto *debug = new QCheckBox(T("Tools.Debug"), this);
	debug->setChecked(meta.uiValue("debugMode").toBool(false));

	auto *form = new QFormLayout();
	form->addRow(T("Tools.Language"), language);
	form->addRow(QString(), languageNote);
	form->addRow(hiddenBoth);
	form->addRow(T("Tools.Version"), selectable(QString::fromUtf8(PLUGIN_VERSION)));
	// The DLL's own date: /Brepro (reproducible builds) turns __DATE__ and __TIME__ into "1".
	QDateTime built = QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).lastModified();
	// Per-language pattern that cannot be misread: month names ("MMMM d, yyyy") or marked units ("yyyy'年'M'月'd'日'").
	QString builtText = QLocale(currentLanguage()).toString(built.date(), T("Tools.BuildDate.Format"));
	form->addRow(T("Tools.BuildDate"), selectable(builtText + "  " + built.toString("HH:mm")));
	form->addRow(T("Tools.Author"), selectable(QStringLiteral("ChromeBread feat. BlackDream")));
	form->addRow(debug);

	QWidget *debugSection = buildDebugSection();
	debugSection->setVisible(debug->isChecked());
	connect(debug, &QCheckBox::toggled, this, [this, debugSection](bool on) {
		PluginMeta::instance().setUiValue("debugMode", on);
		debugSection->setVisible(on);
		resize(width(), qMin(720, sizeHint().height()));
	});

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
	auto *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(features);
	layout->addWidget(debugSection, 1);
	layout->addWidget(buttons);
}

QWidget *ToolsDialog::buildDebugSection()
{
	// Tabs keep the dialog compact; stacked group boxes did not fit a 1280x720 screen.
	auto *tabs = new QTabWidget(this);
	QWidget *box = tabs;

	// Data folder (paths stay out of sight unless debug mode is on)
	auto &meta = PluginMeta::instance();
	auto *dataPage = new QWidget(box);
	auto *dataLayout = new QVBoxLayout(dataPage);
	auto *dataDir = selectable(native(meta.dataDir()));
	auto *changeDir = new QPushButton(T("Tools.DataDir.Change"), dataPage);
	connect(changeDir, &QPushButton::clicked, this, [this] {
		if (choose_data_folder(this, false)) {
			close(); // reopen with the new paths
			QTimer::singleShot(0, [] { ToolsDialog::open(); });
		}
	});
	auto *dataRow = new QHBoxLayout();
	dataRow->addWidget(dataDir, 1);
	dataRow->addWidget(changeDir);

	dataLayout->addLayout(dataRow);
	dataLayout->addStretch(1);
	tabs->addTab(dataPage, T("Tools.DataDir"));

	// Installed files
	QString bin = binaryDir();
	QString pluginRoot = QDir(bin + "/../..").absolutePath();
	auto *installed = new QWidget(box);
	auto *installedLayout = new QVBoxLayout(installed);
	installedLayout->addWidget(selectable(
		QStringList{native(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))),
			    native(QString::fromUtf8(obs_get_module_data_path(obs_current_module())))}
			.join('\n')));
	tabs->addTab(installed, T("Tools.Installed"));

	// Uninstall
	auto *uninstall = new QWidget(box);
	auto *uninstallLayout = new QVBoxLayout(uninstall);
	uninstallLayout->addWidget(selectable(T("Tools.Uninstall.Text") + "\n" +
					      T("Tools.Uninstall.Plugin").arg(native(pluginRoot)) + "\n" +
					      T("Tools.Uninstall.Config").arg(native(configDir())) + "\n" +
					      T("Tools.Uninstall.Data").arg(native(PluginMeta::instance().dataDir()))));
	tabs->addTab(uninstall, T("Tools.Uninstall"));

	// Backup: everything both modules keep, to a folder and back
	auto *backup = new QWidget(box);
	auto *backupLayout = new QVBoxLayout(backup);
	backupLayout->addWidget(selectable(T("Backup.Text")));
	auto *exportAll = new QPushButton(T("Backup.Export"), backup);
	auto *importAll = new QPushButton(T("Backup.Import"), backup);
	connect(exportAll, &QPushButton::clicked, this, [this] { export_all_settings(this); });
	connect(importAll, &QPushButton::clicked, this, [this] {
		if (import_all_settings(this)) {
			close(); // reopen with the imported settings
			QTimer::singleShot(0, [] { ToolsDialog::open(); });
		}
	});
	auto *backupButtons = new QHBoxLayout();
	backupButtons->addWidget(exportAll);
	backupButtons->addWidget(importAll);
	backupButtons->addStretch(1);
	backupLayout->addLayout(backupButtons);
	backupLayout->addStretch(1);
	tabs->addTab(backup, T("Backup"));

	// Log: off by default; written only on this PC, for when the user asks the developer for help
	auto *logPage = new QWidget(box);
	auto *logLayout = new QVBoxLayout(logPage);
	auto *logOn = new QCheckBox(T("Log.Enable"), logPage);
	logOn->setChecked(log_file_enabled());
	auto *logExport = new QPushButton(T("Log.Export"), logPage);
	auto refreshLog = [logExport] {
		logExport->setEnabled(QFile::exists(log_file_path()) || QFile::exists(log_file_path() + ".old"));
	};
	connect(logOn, &QCheckBox::toggled, this, [refreshLog](bool on) {
		PluginMeta::instance().setUiValue("logFile", on);
		log_file_set_enabled(on);
		obs_log(LOG_INFO, on ? "log file on (version %s)" : "log file off (version %s)", PLUGIN_VERSION);
		refreshLog();
	});
	connect(logExport, &QPushButton::clicked, this, [this] {
		QString start = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/vst-ex-log.txt";
		QString path = QFileDialog::getSaveFileName(this, T("Log.Export"), start, "Text (*.txt)");
		if (path.isEmpty())
			return; // the file dialog itself asks before overwriting
		QByteArray text;
		for (const QString &part : {log_file_path() + ".old", log_file_path()}) {
			QFile f(part);
			if (f.open(QIODevice::ReadOnly))
				text += f.readAll();
		}
		QSaveFile out(path);
		if (out.open(QIODevice::WriteOnly) && out.write(text) == text.size() && out.commit())
			QMessageBox::information(this, T("Log.Export"), T("Log.Exported").arg(native(path)));
		else
			QMessageBox::warning(this, T("Log.Export"), T("Log.ExportFailed").arg(native(path)));
	});
	auto *logButtons = new QHBoxLayout();
	logButtons->addWidget(logExport);
	logButtons->addStretch(1);
	logLayout->addWidget(selectable(T("Log.Text")));
	logLayout->addWidget(logOn);
	logLayout->addLayout(logButtons);
	logLayout->addStretch(1);
	refreshLog();
	tabs->addTab(logPage, T("Log"));

	// VST Ex Master: experimental, so nothing of it exists in OBS until the user accepts what is written here
	auto *master = new QWidget(box);
	auto *masterLayout = new QVBoxLayout(master);
	auto *masterText = new QLabel(T("Master.HowTo.Text") + "<br><br>" + T("Master.Unlock.Cautions"), master);
	masterText->setTextFormat(Qt::RichText);
	masterText->setWordWrap(true);
	masterText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	auto *masterScroll = new QScrollArea(master);
	masterScroll->setWidgetResizable(true);
	masterScroll->setFrameShape(QFrame::NoFrame);
	masterScroll->setWidget(masterText);
	auto *agree = new QCheckBox(T("Master.Unlock.Agree"), master);
	auto *unlocked = new QLabel(T("Master.Unlock.Done"), master);
	auto *unlock = new QPushButton(master);
	auto refresh = [agree, unlocked, unlock] {
		bool on = masterbus::unlocked();
		agree->setVisible(!on);
		unlocked->setVisible(on);
		unlock->setText(on ? T("Master.Unlock.Lock") : T("Master.Unlock.Button"));
		unlock->setEnabled(on ? !masterbus::onStream() : agree->isChecked()); // locking mid-stream would cut its sound
	};
	connect(agree, &QCheckBox::toggled, this, refresh);
	connect(unlock, &QPushButton::clicked, this, [agree, refresh] {
		bool on = !masterbus::unlocked();
		masterbus::setUnlocked(on);
		if (on)
			install_master_dock();
		else
			remove_master_dock();
		agree->setChecked(false);
		refresh();
	});
	masterLayout->addWidget(masterScroll, 1);
	masterLayout->addWidget(agree);
	masterLayout->addWidget(unlocked);
	masterLayout->addWidget(unlock);
	refresh();
	tabs->addTab(master, QStringLiteral("VST Ex Master"));

	return tabs;
}
