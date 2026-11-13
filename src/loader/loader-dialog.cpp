#include "loader/loader-dialog.h"

#include "loader/handover.h"
#include "plugin-meta.h"
#include "plugin-scanner.h"
#include "ui/ui-text.h"

#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QCheckBox>
#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QPointer<LoaderDialog> openDialog;

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

QStringList vstFolders()
{
	QStringList found;
	for (const auto &d : PluginCatalog::instance().plugins()) {
		QString dir = native(QFileInfo(QString::fromStdString(d.path)).absolutePath());
		if (!found.contains(dir))
			found << dir;
	}
	found.sort(Qt::CaseInsensitive);
	return found;
}

} // namespace

void LoaderDialog::open()
{
	if (openDialog) {
		openDialog->raise();
		openDialog->activateWindow();
		return;
	}
	openDialog = new LoaderDialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
	openDialog->show();
}

void LoaderDialog::reopen()
{
	close();
	QTimer::singleShot(0, [] { LoaderDialog::open(); });
}

LoaderDialog::LoaderDialog(QWidget *parent) : QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(QStringLiteral("VST Enigmax Loader"));
	resize(720, 440);
	setMaximumSize(1280, 720);
	auto &meta = PluginMeta::instance();

	auto *dataDir = selectable(native(meta.dataDir()));
	auto *changeDir = new QPushButton(T("Loader.DataDir.Change"), this);
	changeDir->setToolTip(T("Loader.DataDir.Tip"));
	connect(changeDir, &QPushButton::clicked, this, [this] {
		QString dir = QFileDialog::getExistingDirectory(this, T("Loader.DataDir"), PluginMeta::instance().dataDir());
		if (dir.isEmpty())
			return;
		if (!PluginMeta::instance().setDataDir(dir))
			QMessageBox::warning(this, T("Loader.DataDir"), T("Loader.DataDir.Failed").arg(native(dir)));
		reopen();
	});
	auto *dataRow = new QHBoxLayout();
	dataRow->addWidget(dataDir, 1);
	dataRow->addWidget(changeDir);

	// The DLL's own date: /Brepro (reproducible builds) turns __DATE__ into "1". Shown with the month named (or
	// year / month / day marked), so no order is misread.
	QDate built = QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).lastModified().date();
	auto *form = new QFormLayout();
	form->addRow(T("Loader.Version"), selectable(QString::fromUtf8(PLUGIN_VERSION)));
	form->addRow(T("Loader.BuildDate"), selectable(QLocale(PluginMeta::instance().uiValue("language").toString(
								  QString::fromUtf8(obs_get_locale())))
							  .toString(built, T("Loader.BuildDate.Format"))));
	form->addRow(T("Loader.Author"), selectable(QStringLiteral("ChromeBread feat. BlackDream")));
	auto *debug = new QCheckBox(T("Loader.Debug"), this);
	debug->setToolTip(T("Loader.Debug.Tip"));
	debug->setChecked(meta.uiValue("debugMode").toBool(false));
	form->addRow(debug);

	// Paths, file names and the names of sources and filters stay out of sight unless debug mode is on.
	auto *debugSection = new QWidget(this);
	auto *debugForm = new QFormLayout();
	debugForm->setContentsMargins(0, 0, 0, 0);
	debugForm->addRow(T("Loader.DataDir"), dataRow);
	debugForm->addRow(T("Loader.Installed"),
			  selectable(native(QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module())))
						    .absolutePath() +
					    "/../..")));
	auto *tabs = new QTabWidget(debugSection);
	tabs->addTab(buildHandoverPage(), T("Handover"));
	tabs->addTab(buildFilesPage(), T("Loader.VstFiles"));
	tabs->addTab(buildFoldersPage(), T("Loader.VstFolders"));
	tabs->addTab(buildShotsPage(meta.screenshotDir(), "Loader.Thumbnails.Text", false), T("Loader.Thumbnails"));
	tabs->addTab(buildShotsPage(meta.highResDir(), "Loader.HighRes.Text", true), T("Loader.HighRes"));
	auto *debugLayout = new QVBoxLayout(debugSection);
	debugLayout->setContentsMargins(0, 0, 0, 0);
	debugLayout->addLayout(debugForm);
	debugLayout->addWidget(tabs, 1);
	debugSection->setVisible(debug->isChecked());

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
	auto *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(debugSection, 1);
	layout->addWidget(buttons);

	connect(debug, &QCheckBox::toggled, this, [this, debugSection](bool on) {
		PluginMeta::instance().setUiValue("debugMode", on);
		debugSection->setVisible(on);
		resize(width(), on ? 440 : sizeHint().height());
	});
	if (!debug->isChecked())
		resize(width(), sizeHint().height());
}

// Where the pictures are, how many, how big; open the folder or delete them all.
QWidget *LoaderDialog::buildShotsPage(const QString &dir, const char *countKey, bool highRes)
{
	int files = 0;
	qint64 bytes = 0;
	for (QDirIterator it(dir, {"*.png"}, QDir::Files); it.hasNext();) {
		bytes += QFileInfo(it.next()).size();
		files++;
	}
	QString size = QLocale().formattedDataSize(bytes);
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);
	layout->addWidget(selectable(native(dir)));
	layout->addWidget(selectable(T(countKey).arg(files).arg(size)));
	auto *openFolder = new QPushButton(T("Loader.OpenFolder"), page);
	auto *remove = new QPushButton(T("Loader.Delete").arg(size), page);
	remove->setEnabled(bytes > 0);
	connect(openFolder, &QPushButton::clicked, this, [dir] {
		QDir().mkpath(dir);
		QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
	});
	connect(remove, &QPushButton::clicked, this, [this, size, highRes] {
		if (QMessageBox::question(this, windowTitle(), T("Loader.Delete.Confirm").arg(size),
					  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
		if (highRes)
			PluginMeta::instance().deleteHighRes();
		else
			PluginMeta::instance().deleteThumbnails();
		reopen(); // with the new numbers
	});
	auto *row = new QHBoxLayout();
	row->addWidget(openFolder);
	row->addWidget(remove);
	row->addStretch(1);
	layout->addLayout(row);
	layout->addStretch(1);
	return page;
}

// One line per VST filter that can move between VST Enigmax Loader and OBS VST 2.x / atkAudio.
QWidget *LoaderDialog::buildHandoverPage()
{
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);
	layout->addWidget(selectable(T("Handover.Help")));
	auto *list = new QListWidget(page);
	QVector<handover::Item> items = handover::collect();
	for (int i = 0; i < items.size(); i++) {
		const auto &it = items[i];
		bool outward = it.kind == handover::Kind::LoaderVst2 || it.kind == handover::Kind::LoaderVst3;
		QString from = outward ? QStringLiteral("VST Enigmax Loader")
				       : it.kind == handover::Kind::ObsVst2 ? QStringLiteral("OBS VST 2.x")
									     : QStringLiteral("atkAudio PluginHost");
		QString text = QString("%1 > %2   [%3]   %4 -> %5")
				       .arg(it.sourceName, it.filterName, it.plugin, from, handover::targetName(it.kind));
		if (!it.problem.isEmpty())
			text += "   (" + it.problem + ")";
		auto *row = new QListWidgetItem(text, list);
		row->setData(Qt::UserRole, i);
		row->setFlags(it.problem.isEmpty() ? (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable) : Qt::NoItemFlags);
		row->setCheckState(Qt::Unchecked);
	}
	if (items.isEmpty())
		new QListWidgetItem(T("Handover.None"), list);
	auto *convert = new QPushButton(T("Handover.Convert"), page);
	connect(convert, &QPushButton::clicked, this, [this, list, items] {
		QVector<handover::Item> checked;
		for (int r = 0; r < list->count(); r++)
			if (list->item(r)->checkState() == Qt::Checked)
				checked << items[list->item(r)->data(Qt::UserRole).toInt()];
		if (checked.isEmpty() ||
		    QMessageBox::question(this, T("Handover"), T("Handover.Confirm").arg(checked.size())) != QMessageBox::Yes)
			return;
		QStringList failed;
		for (const auto &it : checked) {
			QString error;
			if (!handover::convert(it, error))
				failed << QString("%1 > %2: %3").arg(it.sourceName, it.filterName, error);
		}
		if (!failed.isEmpty())
			QMessageBox::warning(this, T("Handover"), T("Handover.Failed") + '\n' + failed.join('\n'));
		reopen(); // with the new list
	});
	layout->addWidget(list, 1);
	layout->addWidget(convert, 0, Qt::AlignLeft);
	return page;
}

QWidget *LoaderDialog::buildFoldersPage()
{
	QStringList search;
	for (const QString &p : PluginCatalog::instance().searchPaths())
		search << native(p);
	QString text = "[" + T("Loader.VstFolders.Search") + "]\n" + search.join('\n') + "\n\n[" +
		       T("Loader.VstFolders.Found") + "]\n" + vstFolders().join('\n');
	return buildTextPage(text, "vst-folders.txt");
}

// Every scanned plugin with its full file path, one per line.
QWidget *LoaderDialog::buildFilesPage()
{
	QStringList lines;
	for (const auto &d : PluginCatalog::instance().plugins())
		lines << QString("[%1] %2    %3")
				 .arg(QString::fromLatin1(vstex::formatName(d.format)), QString::fromStdString(d.name),
				      native(QString::fromStdString(d.path)));
	lines.sort(Qt::CaseInsensitive);
	QString text = T("Loader.VstFiles.Count").arg(lines.size()) + "\n\n" + lines.join('\n');
	return buildTextPage(text, "vst-files.txt");
}

// Read-only text with an Export... button that saves it as a .txt file.
QWidget *LoaderDialog::buildTextPage(const QString &text, const QString &fileName)
{
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);
	auto *list = new QPlainTextEdit(text, page);
	list->setReadOnly(true);
	list->setLineWrapMode(QPlainTextEdit::NoWrap);
	auto *exportButton = new QPushButton(T("Loader.Export"), page);
	connect(exportButton, &QPushButton::clicked, this, [this, text, fileName] {
		QString path = QFileDialog::getSaveFileName(this, T("Loader.Export"), fileName, "Text (*.txt)");
		if (path.isEmpty())
			return;
		QSaveFile f(path);
		if (!f.open(QIODevice::WriteOnly) || f.write(text.toUtf8()) < 0 || !f.commit())
			QMessageBox::warning(this, T("Loader.Export"), f.errorString());
	});
	layout->addWidget(list, 1);
	layout->addWidget(exportButton, 0, Qt::AlignLeft);
	return page;
}
