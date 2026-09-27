#include <vlc/vlc.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tvariant.h>

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QSaveFile>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <cstdint>

namespace {

QString iconPath(const QString &name)
{
    return QStringLiteral(":/media/assets/media-player/icons/") + name + QStringLiteral(".png");
}

QIcon icon(const QString &name)
{
    return QIcon(iconPath(name));
}

QString clockTime(qint64 milliseconds)
{
    if (milliseconds < 0) {
        return QStringLiteral("00:00");
    }
    const qint64 seconds = milliseconds / 1000;
    if (seconds >= 3600) {
        return QStringLiteral("%1:%2:%3")
            .arg(seconds / 3600)
            .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

bool isVideo(const QString &path)
{
    static const QSet<QString> suffixes = {
        QStringLiteral("mp4"), QStringLiteral("m4v"), QStringLiteral("mkv"),
        QStringLiteral("avi"), QStringLiteral("mov"), QStringLiteral("wmv"),
        QStringLiteral("webm"), QStringLiteral("mpg"), QStringLiteral("mpeg")};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

bool isMedia(const QString &path)
{
    static const QSet<QString> suffixes = {
        QStringLiteral("mp3"), QStringLiteral("m4a"), QStringLiteral("mp4"),
        QStringLiteral("m4v"), QStringLiteral("wav"), QStringLiteral("flac"),
        QStringLiteral("ogg"), QStringLiteral("wma"), QStringLiteral("mkv"),
        QStringLiteral("avi"), QStringLiteral("mov"), QStringLiteral("wmv"),
        QStringLiteral("webm"), QStringLiteral("mpg"), QStringLiteral("mpeg")};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

struct MediaEntry {
    QString path;
    QString title;
    QString artist;
    QString album;
    QString genre;
    qint64 durationMs = -1;
    QString kind;
};

QList<MediaEntry> scanFolders(const QStringList &folders)
{
    QList<MediaEntry> entries;
    QSet<QString> seen;
    for (const QString &folder : folders) {
        if (!QDir(folder).exists()) {
            continue;
        }
        QDirIterator iterator(folder, QDir::Files | QDir::Readable | QDir::NoSymLinks,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext() && entries.size() < 5000) {
            const QString path = iterator.next();
            if (!isMedia(path) || seen.contains(path)) {
                continue;
            }
            seen.insert(path);
            const QFileInfo info(path);
            MediaEntry entry{path, info.completeBaseName(), QString(), info.dir().dirName(),
                             QString(), -1,
                             isVideo(path) ? QStringLiteral("Video") : QStringLiteral("Music")};
            if (!isVideo(path)) {
                const QByteArray name = path.toUtf8();
                const TagLib::FileRef tags(name.constData(), true, TagLib::AudioProperties::Fast);
                if (!tags.isNull()) {
                    if (const auto *tag = tags.tag()) {
                        const auto value = [](const TagLib::String &text) {
                            return QString::fromStdString(text.to8Bit(true));
                        };
                        if (!tag->title().isEmpty()) entry.title = value(tag->title());
                        entry.artist = value(tag->artist());
                        if (!tag->album().isEmpty()) entry.album = value(tag->album());
                        entry.genre = value(tag->genre());
                    }
                    if (const auto *properties = tags.audioProperties()) {
                        entry.durationMs = properties->lengthInMilliseconds();
                    }
                }
            }
            entries.append(entry);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const MediaEntry &a, const MediaEntry &b) {
        return a.title.localeAwareCompare(b.title) < 0;
    });
    return entries;
}

QToolButton *toolButton(const QString &resource, const QString &tip, QWidget *parent,
                        int iconSize = 22)
{
    auto *button = new QToolButton(parent);
    button->setIcon(icon(resource));
    button->setIconSize(QSize(iconSize, iconSize));
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

class MediaWindow final : public QMainWindow {
public:
    explicit MediaWindow(const QStringList &startupFiles)
    {
        setWindowTitle(QStringLiteral("Media Player"));
        setWindowIcon(icon(QStringLiteral("app/64")));
        setMinimumSize(780, 520);
        resize(1040, 680);
        setAcceptDrops(true);

        const char *arguments[] = {"--no-video-title-show", "--quiet"};
        vlc_ = libvlc_new(2, arguments);
        if (vlc_) {
            player_ = libvlc_media_player_new(vlc_);
        }

        buildUi();
        connect(&clock_, &QTimer::timeout, this, [this] { refreshPlayback(); });
        clock_.start(250);
        scanLibrary();

        if (!player_) {
            statusBar()->showMessage(QStringLiteral("VLC playback engine is unavailable."));
            play_->setEnabled(false);
        } else if (!startupFiles.isEmpty()) {
            queue_ = startupFiles;
            rebuildQueue();
            playIndex(0);
        } else {
            showLibrary();
        }
    }

    ~MediaWindow() override
    {
        if (player_) {
            libvlc_media_player_stop(player_);
            libvlc_media_player_release(player_);
        }
        if (vlc_) {
            libvlc_release(vlc_);
        }
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        QStringList files;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && isMedia(url.toLocalFile())) {
                files.append(url.toLocalFile());
            }
        }
        if (!files.isEmpty()) {
            queue_ = files;
            rebuildQueue();
            playIndex(0);
            event->acceptProposedAction();
        }
    }

    void closeEvent(QCloseEvent *event) override
    {
        if (player_) {
            libvlc_media_player_stop(player_);
        }
        QMainWindow::closeEvent(event);
    }

private:
    enum Category { All = 0, Music = 1, Artists = 2, Albums = 3, Genres = 4,
                    Videos = 5, Queue = 6 };

    void buildUi()
    {
        setStyleSheet(QStringLiteral(R"CSS(
            QMainWindow, QWidget#root { background: #f5f9fd; color: #193751; }
            QMenuBar { background: #e8f1f9; border-bottom: 1px solid #c3d7e8; padding: 2px; }
            QMenuBar::item:selected, QMenu::item:selected { background: #c6e1f6; }
            QMenu { background: #f8fbff; border: 1px solid #9bbbd3; }
            QWidget#navigation { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #d5e5f3,stop:1 #f6fafe); border-bottom: 1px solid #aac6dc; }
            QWidget#tabs { background: #edf5fb; border-bottom: 1px solid #b4ccdf; }
            QFrame#sidebar { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #e1edf8,stop:1 #f4f9fd); border-right: 1px solid #bdd1e1; }
            QListWidget#categories { background: transparent; border: 0; outline: 0; color: #214767; }
            QListWidget#categories::item { padding: 8px 9px; margin: 1px 6px; }
            QListWidget#categories::item:selected { background: #c9e3f8; border: 1px solid #86b7dc; border-radius: 3px; color: #143c5b; }
            QTableWidget { background: white; alternate-background-color: #f3f8fc; border: 0; gridline-color: #e0eaf2; selection-background-color: #c6e4fa; selection-color: #123d5e; }
            QHeaderView::section { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #fbfdff,stop:1 #e2eff9); color: #385a73; border: 0; border-right: 1px solid #c8dce9; border-bottom: 1px solid #bdd4e5; padding: 5px 8px; }
            QLineEdit { background: white; border: 1px solid #8cb7d3; border-radius: 3px; padding: 5px; selection-background-color: #98c8ee; }
            QPushButton#tab { background: transparent; border: 0; border-left: 1px solid #ccdce9; padding: 9px 23px; color: #264862; }
            QPushButton#tab:checked { background: white; border-bottom: 2px solid #4a94d5; color: #124f8a; }
            QPushButton#tab:disabled { color: #9aabba; }
            QFrame#transport { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f8fcff,stop:1 #d4e5f3); border-top: 1px solid #9dbed6; }
            QToolButton { background: transparent; border: 0; border-radius: 4px; padding: 4px; }
            QToolButton:hover { background: #cee8fb; border: 1px solid #7cb2db; }
            QToolButton#play { background: qradialgradient(cx:0.5,cy:0.3,radius:0.8,fx:0.5,fy:0.2,stop:0 #e8f6ff,stop:0.65 #76b5e9,stop:1 #1e66af); border: 1px solid #2a6da4; border-radius: 24px; padding: 5px; }
            QToolButton#play:hover { background: #a9d9fa; }
            QSlider::groove:horizontal { height: 5px; background: #9ab0c1; border: 1px solid #6e8ca6; border-radius: 3px; }
            QSlider::sub-page:horizontal { background: #3694e2; border-radius: 3px; }
            QSlider::handle:horizontal { width: 13px; margin: -5px 0; border: 1px solid #3b76ae; border-radius: 7px; background: #d2edff; }
            QWidget#nowPage { background: qradialgradient(cx:0.5,cy:0.45,radius:0.8,stop:0 #116489,stop:0.63 #092a45,stop:1 #010a13); }
            QLabel#nowTitle { color: white; font-size: 17px; font-weight: 600; }
            QLabel#nowSubtitle { color: #bed4e4; font-size: 11px; }
            QFrame#artFrame { background: rgba(224,241,255,35); border: 1px solid rgba(186,219,244,90); }
            QFrame#videoHost { background: black; border: 0; }
            QStatusBar { background: #e8f1f9; border-top: 1px solid #c2d8e8; color: #3b5d74; }
        )CSS"));

        auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
        fileMenu->addAction(icon(QStringLiteral("document-open")), QStringLiteral("Open file..."),
                            QKeySequence::Open, this, [this] { openFiles(); });
        fileMenu->addAction(icon(QStringLiteral("list-add")), QStringLiteral("Add folder to library..."),
                            this, [this] { addFolder(); });
        fileMenu->addSeparator();
        fileMenu->addAction(QStringLiteral("Exit"), this, &QWidget::close);

        auto *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
        viewMenu->addAction(QStringLiteral("Library"), this, [this] { showLibrary(); });
        viewMenu->addAction(QStringLiteral("Now Playing"), this, [this] { showNowPlaying(); });
        viewMenu->addAction(QStringLiteral("Full screen"), QKeySequence(Qt::Key_F11),
                            this, [this] { toggleFullScreen(); });

        auto *playMenu = menuBar()->addMenu(QStringLiteral("&Play"));
        playMenu->addAction(QStringLiteral("Play / Pause"), this, [this] { togglePlay(); });
        playMenu->addAction(QStringLiteral("Stop"), this, [this] { stop(); });
        playMenu->addAction(QStringLiteral("Previous"), this, [this] { previous(); });
        playMenu->addAction(QStringLiteral("Next"), this, [this] { next(); });

        auto *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
        helpMenu->addAction(QStringLiteral("About Media Player"), this, [this] {
            QMessageBox::about(this, QStringLiteral("About Media Player"),
                               QStringLiteral("Aero7 Media Player uses VLC for real audio and video playback.\n"
                                              "The library browses your Music and Videos folders; disc burning and "
                                              "device synchronization are not available."));
        });

        auto *root = new QWidget(this);
        root->setObjectName(QStringLiteral("root"));
        auto *vertical = new QVBoxLayout(root);
        vertical->setContentsMargins(0, 0, 0, 0);
        vertical->setSpacing(0);
        setCentralWidget(root);

        auto *navigation = new QWidget(root);
        navigation->setObjectName(QStringLiteral("navigation"));
        auto *navLayout = new QHBoxLayout(navigation);
        navLayout->setContentsMargins(12, 5, 12, 5);
        navLayout->setSpacing(7);
        auto *back = toolButton(QStringLiteral("library/back"), QStringLiteral("Library"), navigation);
        auto *forward = toolButton(QStringLiteral("library/forward"), QStringLiteral("Now Playing"), navigation);
        navLayout->addWidget(back);
        navLayout->addWidget(forward);
        connect(back, &QToolButton::clicked, this, [this] { showLibrary(); });
        connect(forward, &QToolButton::clicked, this, [this] { showNowPlaying(); });
        breadcrumb_ = new QLabel(QStringLiteral("Library  >  Music"), navigation);
        breadcrumb_->setStyleSheet(QStringLiteral("font-size: 11px; color: #245b83; padding-left: 10px;"));
        navLayout->addWidget(breadcrumb_, 1);
        search_ = new QLineEdit(navigation);
        search_->setPlaceholderText(QStringLiteral("Search library"));
        search_->setFixedWidth(215);
        search_->addAction(icon(QStringLiteral("library/search")), QLineEdit::LeadingPosition);
        navLayout->addWidget(search_);
        connect(search_, &QLineEdit::textChanged, this, [this] { updateLibrary(); });
        vertical->addWidget(navigation);

        auto *tabs = new QWidget(root);
        tabs->setObjectName(QStringLiteral("tabs"));
        auto *tabsLayout = new QHBoxLayout(tabs);
        tabsLayout->setContentsMargins(14, 0, 12, 0);
        tabsLayout->setSpacing(0);
        auto *organize = new QToolButton(tabs);
        organize->setText(QStringLiteral("Organize  ▾"));
        organize->setPopupMode(QToolButton::InstantPopup);
        auto *organizeMenu = new QMenu(organize);
        organizeMenu->addAction(icon(QStringLiteral("document-open")), QStringLiteral("Open file..."),
                                this, [this] { openFiles(); });
        organizeMenu->addAction(icon(QStringLiteral("list-add")), QStringLiteral("Add folder..."),
                                this, [this] { addFolder(); });
        organizeMenu->addAction(QStringLiteral("Refresh library"), this, [this] { scanLibrary(); });
        organize->setMenu(organizeMenu);
        tabsLayout->addWidget(organize);
        auto *stream = new QToolButton(tabs);
        stream->setText(QStringLiteral("Stream  ▾"));
        stream->setPopupMode(QToolButton::InstantPopup);
        auto *streamMenu = new QMenu(stream);
        streamMenu->addAction(QStringLiteral("Open network stream..."), this, [this] { openStream(); });
        stream->setMenu(streamMenu);
        tabsLayout->addWidget(stream);
        auto *playlist = new QToolButton(tabs);
        playlist->setText(QStringLiteral("Create playlist  ▾"));
        playlist->setPopupMode(QToolButton::InstantPopup);
        auto *playlistMenu = new QMenu(playlist);
        playlistMenu->addAction(QStringLiteral("Save current list..."), this, [this] { savePlaylist(); });
        playlistMenu->addAction(QStringLiteral("Open playlist..."), this, [this] { openPlaylist(); });
        playlist->setMenu(playlistMenu);
        tabsLayout->addWidget(playlist);
        tabsLayout->addStretch(1);
        auto *playTab = new QPushButton(QStringLiteral("Play"), tabs);
        playTab->setObjectName(QStringLiteral("tab"));
        playTab->setCheckable(true);
        playTab->setChecked(true);
        tabsLayout->addWidget(playTab);
        connect(playTab, &QPushButton::clicked, this, [this] { showLibrary(); });
        for (const QString &label : {QStringLiteral("Burn"), QStringLiteral("Sync")}) {
            auto *unavailable = new QPushButton(label, tabs);
            unavailable->setObjectName(QStringLiteral("tab"));
            unavailable->setToolTip(QStringLiteral("Not available in Aero7 Media Player"));
            unavailable->setEnabled(false);
            tabsLayout->addWidget(unavailable);
        }
        vertical->addWidget(tabs);

        pages_ = new QStackedWidget(root);
        vertical->addWidget(pages_, 1);
        buildLibraryPage();
        buildNowPage();
        buildTransport(root, vertical);
        statusBar()->showMessage(QStringLiteral("Ready"));
    }

    void buildLibraryPage()
    {
        auto *library = new QWidget(pages_);
        auto *layout = new QHBoxLayout(library);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        auto *sidebar = new QFrame(library);
        sidebar->setObjectName(QStringLiteral("sidebar"));
        sidebar->setFixedWidth(185);
        auto *sideLayout = new QVBoxLayout(sidebar);
        sideLayout->setContentsMargins(5, 11, 5, 7);
        sideLayout->setSpacing(2);
        auto *libraryLabel = new QLabel(QStringLiteral("  Library"), sidebar);
        libraryLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #2c5574; padding: 5px;"));
        sideLayout->addWidget(libraryLabel);
        categories_ = new QListWidget(sidebar);
        categories_->setObjectName(QStringLiteral("categories"));
        categories_->setIconSize(QSize(22, 22));
        const auto addCategory = [this](const QString &title, const QString &resource, Category category) {
            auto *item = new QListWidgetItem(icon(resource), title, categories_);
            item->setData(Qt::UserRole, category);
        };
        addCategory(QStringLiteral("All media"), QStringLiteral("library/folder"), All);
        auto *playlistLabel = new QListWidgetItem(QStringLiteral("Playlists"), categories_);
        playlistLabel->setFlags(Qt::NoItemFlags);
        addCategory(QStringLiteral("Now Playing"), QStringLiteral("view-list-details"), Queue);
        auto *musicLabel = new QListWidgetItem(QStringLiteral("Music"), categories_);
        musicLabel->setFlags(Qt::NoItemFlags);
        addCategory(QStringLiteral("All music"), QStringLiteral("library/music"), Music);
        addCategory(QStringLiteral("    Artist"), QStringLiteral("library/music"), Artists);
        addCategory(QStringLiteral("    Album"), QStringLiteral("library/music"), Albums);
        addCategory(QStringLiteral("    Genre"), QStringLiteral("library/music"), Genres);
        addCategory(QStringLiteral("Videos"), QStringLiteral("library/videos"), Videos);
        categories_->setCurrentRow(4);
        connect(categories_, &QListWidget::currentRowChanged, this, [this] { updateLibrary(); });
        sideLayout->addWidget(categories_, 1);
        auto *addFolderButton = new QPushButton(icon(QStringLiteral("list-add")),
                                                QStringLiteral("Add folder..."), sidebar);
        connect(addFolderButton, &QPushButton::clicked, this, [this] { addFolder(); });
        sideLayout->addWidget(addFolderButton);
        layout->addWidget(sidebar);

        auto *content = new QWidget(library);
        auto *contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(17, 15, 17, 12);
        contentLayout->setSpacing(8);
        libraryHeading_ = new QLabel(QStringLiteral("Music"), content);
        libraryHeading_->setStyleSheet(QStringLiteral("font-size: 19px; color: #185a91;"));
        contentLayout->addWidget(libraryHeading_);
        libraryDescription_ = new QLabel(QStringLiteral("Browse and play your music."), content);
        libraryDescription_->setStyleSheet(QStringLiteral("color: #5c7b91;"));
        contentLayout->addWidget(libraryDescription_);
        libraryContent_ = new QStackedWidget(content);
        table_ = new QTableWidget(libraryContent_);
        table_->setColumnCount(6);
        table_->setHorizontalHeaderLabels({QStringLiteral("Title"), QStringLiteral("Length"),
                                            QStringLiteral("Artist"), QStringLiteral("Album"),
                                            QStringLiteral("Genre"), QStringLiteral("Type")});
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setAlternatingRowColors(true);
        table_->setShowGrid(false);
        table_->verticalHeader()->hide();
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        table_->setColumnWidth(1, 70);
        table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
        table_->setColumnWidth(2, 135);
        table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
        table_->setColumnWidth(3, 135);
        table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
        table_->setColumnWidth(4, 90);
        table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
        table_->setColumnWidth(5, 60);
        table_->setSortingEnabled(true);
        connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            QStringList visible;
            for (int index = 0; index < table_->rowCount(); ++index) {
                visible.append(table_->item(index, 0)->data(Qt::UserRole).toString());
            }
            queue_ = visible;
            rebuildQueue();
            playIndex(row);
        });
        libraryContent_->addWidget(table_);

        auto *empty = new QWidget(libraryContent_);
        auto *emptyLayout = new QVBoxLayout(empty);
        emptyLayout->setAlignment(Qt::AlignCenter);
        auto *emptyIcon = new QLabel(empty);
        emptyIcon->setPixmap(icon(QStringLiteral("library/music")).pixmap(72, 72));
        emptyIcon->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyIcon);
        emptyText_ = new QLabel(QStringLiteral("Looking for music..."), empty);
        emptyText_->setAlignment(Qt::AlignCenter);
        emptyText_->setStyleSheet(QStringLiteral("font-size: 16px; color: #325a76;"));
        emptyLayout->addWidget(emptyText_);
        auto *emptyHint = new QLabel(QStringLiteral("Music and Videos folders are included automatically."), empty);
        emptyHint->setAlignment(Qt::AlignCenter);
        emptyHint->setStyleSheet(QStringLiteral("color: #6f8799;"));
        emptyLayout->addWidget(emptyHint);
        auto *emptyButtons = new QHBoxLayout();
        emptyButtons->setAlignment(Qt::AlignCenter);
        auto *open = new QPushButton(icon(QStringLiteral("document-open")), QStringLiteral("Open a file"), empty);
        auto *add = new QPushButton(icon(QStringLiteral("list-add")), QStringLiteral("Add a folder"), empty);
        connect(open, &QPushButton::clicked, this, [this] { openFiles(); });
        connect(add, &QPushButton::clicked, this, [this] { addFolder(); });
        emptyButtons->addWidget(open);
        emptyButtons->addWidget(add);
        emptyLayout->addLayout(emptyButtons);
        libraryContent_->addWidget(empty);
        libraryContent_->setCurrentIndex(1);
        contentLayout->addWidget(libraryContent_, 1);
        layout->addWidget(content, 1);
        pages_->addWidget(library);
    }

    void buildNowPage()
    {
        auto *now = new QWidget(pages_);
        now->setObjectName(QStringLiteral("nowPage"));
        auto *layout = new QVBoxLayout(now);
        layout->setContentsMargins(18, 13, 18, 13);
        layout->setSpacing(7);
        auto *heading = new QHBoxLayout();
        auto *titles = new QVBoxLayout();
        nowTitle_ = new QLabel(QStringLiteral("Now Playing"), now);
        nowTitle_->setObjectName(QStringLiteral("nowTitle"));
        nowSubtitle_ = new QLabel(QStringLiteral("Choose a song or video from your library."), now);
        nowSubtitle_->setObjectName(QStringLiteral("nowSubtitle"));
        titles->addWidget(nowTitle_);
        titles->addWidget(nowSubtitle_);
        heading->addLayout(titles, 1);
        auto *showList = new QPushButton(icon(QStringLiteral("view-list-details")),
                                         QStringLiteral("Show List"), now);
        connect(showList, &QPushButton::clicked, this, [this] {
            queuePanel_->setVisible(!queuePanel_->isVisible());
        });
        heading->addWidget(showList);
        layout->addLayout(heading);

        auto *middle = new QHBoxLayout();
        artStack_ = new QStackedWidget(now);
        auto *artFrame = new QFrame(artStack_);
        artFrame->setObjectName(QStringLiteral("artFrame"));
        auto *artLayout = new QVBoxLayout(artFrame);
        artLayout->setContentsMargins(20, 20, 20, 20);
        art_ = new QLabel(artFrame);
        art_->setAlignment(Qt::AlignCenter);
        art_->setPixmap(QPixmap(iconPath(QStringLiteral("library/audio-art"))).scaled(
            250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        artLayout->addWidget(art_);
        artStack_->addWidget(artFrame);
        videoHost_ = new QFrame(artStack_);
        videoHost_->setObjectName(QStringLiteral("videoHost"));
        videoHost_->setAttribute(Qt::WA_NativeWindow);
        artStack_->addWidget(videoHost_);
        artStack_->setCurrentIndex(0);
        middle->addWidget(artStack_, 1);

        queuePanel_ = new QFrame(now);
        queuePanel_->setFixedWidth(250);
        queuePanel_->setStyleSheet(QStringLiteral("QFrame { background: #eaf3fa; border: 1px solid #a5c3d9; }"));
        auto *queueLayout = new QVBoxLayout(queuePanel_);
        queueLayout->setContentsMargins(8, 7, 8, 7);
        auto *queueTitle = new QLabel(QStringLiteral("Now Playing List"), queuePanel_);
        queueTitle->setStyleSheet(QStringLiteral("font-weight: 600; color: #255477; border: 0;"));
        queueLayout->addWidget(queueTitle);
        queueList_ = new QListWidget(queuePanel_);
        queueList_->setStyleSheet(QStringLiteral("QListWidget { background: white; border: 1px solid #bfd5e5; }"));
        queueLayout->addWidget(queueList_, 1);
        connect(queueList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
            playIndex(queueList_->row(item));
        });
        queuePanel_->hide();
        middle->addWidget(queuePanel_);
        layout->addLayout(middle, 1);
        pages_->addWidget(now);
    }

    void buildTransport(QWidget *parent, QVBoxLayout *vertical)
    {
        auto *transport = new QFrame(parent);
        transport->setObjectName(QStringLiteral("transport"));
        auto *layout = new QVBoxLayout(transport);
        layout->setContentsMargins(13, 5, 13, 5);
        layout->setSpacing(4);
        progress_ = new QSlider(Qt::Horizontal, transport);
        progress_->setRange(0, 1000);
        connect(progress_, &QSlider::sliderReleased, this, [this] {
            if (player_) {
                libvlc_media_player_set_position(player_, progress_->value() / 1000.0f);
            }
        });
        layout->addWidget(progress_);
        auto *controls = new QHBoxLayout();
        controls->setSpacing(5);
        elapsed_ = new QLabel(QStringLiteral("00:00 / 00:00"), transport);
        elapsed_->setFixedWidth(125);
        controls->addWidget(elapsed_);
        controls->addStretch(1);
        auto *shuffle = toolButton(QStringLiteral("view-list-details"), QStringLiteral("Library"), transport);
        controls->addWidget(shuffle);
        connect(shuffle, &QToolButton::clicked, this, [this] { showLibrary(); });
        auto *previousButton = toolButton(QStringLiteral("media-skip-backward"), QStringLiteral("Previous"), transport);
        controls->addWidget(previousButton);
        connect(previousButton, &QToolButton::clicked, this, [this] { previous(); });
        play_ = toolButton(QStringLiteral("media-playback-start"), QStringLiteral("Play / Pause"), transport, 30);
        play_->setObjectName(QStringLiteral("play"));
        play_->setFixedSize(48, 48);
        controls->addWidget(play_);
        connect(play_, &QToolButton::clicked, this, [this] { togglePlay(); });
        auto *stopButton = toolButton(QStringLiteral("media-playback-stop"), QStringLiteral("Stop"), transport);
        controls->addWidget(stopButton);
        connect(stopButton, &QToolButton::clicked, this, [this] { stop(); });
        auto *nextButton = toolButton(QStringLiteral("media-skip-forward"), QStringLiteral("Next"), transport);
        controls->addWidget(nextButton);
        connect(nextButton, &QToolButton::clicked, this, [this] { next(); });
        controls->addStretch(1);
        auto *mute = toolButton(QStringLiteral("stock_volume-max"), QStringLiteral("Mute"), transport, 19);
        controls->addWidget(mute);
        connect(mute, &QToolButton::clicked, this, [this, mute] {
            if (!player_) {
                return;
            }
            const bool muted = libvlc_audio_get_mute(player_) == 1;
            libvlc_audio_set_mute(player_, !muted);
            mute->setIcon(icon(muted ? QStringLiteral("stock_volume-max")
                                     : QStringLiteral("stock_volume-mute")));
        });
        auto *volume = new QSlider(Qt::Horizontal, transport);
        volume->setFixedWidth(85);
        volume->setRange(0, 100);
        volume->setValue(80);
        if (player_) {
            libvlc_audio_set_volume(player_, 80);
        }
        connect(volume, &QSlider::valueChanged, this, [this](int value) {
            if (player_) {
                libvlc_audio_set_volume(player_, value);
            }
        });
        controls->addWidget(volume);
        auto *toggle = toolButton(QStringLiteral("view-fullscreen"),
                                  QStringLiteral("Full screen / restore"), transport, 21);
        connect(toggle, &QToolButton::clicked, this, [this] { toggleFullScreen(); });
        controls->addWidget(toggle);
        layout->addLayout(controls);
        vertical->addWidget(transport);
    }

    void scanLibrary()
    {
        QStringList folders = {
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)};
        QSettings settings(QStringLiteral("Aero7"), QStringLiteral("MediaPlayer"));
        folders.append(settings.value(QStringLiteral("libraryFolders")).toStringList());
        folders.append(qEnvironmentVariable("AERO7_MEDIA_LIBRARY_FOLDERS")
                           .split(QDir::listSeparator(), Qt::SkipEmptyParts));
        folders.removeDuplicates();
        statusBar()->showMessage(QStringLiteral("Scanning music and videos..."));
        auto *watcher = new QFutureWatcher<QList<MediaEntry>>(this);
        connect(watcher, &QFutureWatcher<QList<MediaEntry>>::finished, this, [this, watcher] {
            media_ = watcher->result();
            watcher->deleteLater();
            updateLibrary();
            statusBar()->showMessage(QStringLiteral("%1 items in library").arg(media_.size()), 7000);
        });
        watcher->setFuture(QtConcurrent::run([folders] { return scanFolders(folders); }));
    }

    void updateLibrary()
    {
        if (!categories_ || !table_) {
            return;
        }
        const auto category = static_cast<Category>(categories_->currentItem()->data(Qt::UserRole).toInt());
        QString heading;
        switch (category) {
        case Music: heading = QStringLiteral("Music"); break;
        case Artists: heading = QStringLiteral("Artists"); break;
        case Albums: heading = QStringLiteral("Albums"); break;
        case Genres: heading = QStringLiteral("Genres"); break;
        case Videos: heading = QStringLiteral("Videos"); break;
        case Queue: heading = QStringLiteral("Now Playing List"); break;
        default: heading = QStringLiteral("All media"); break;
        }
        libraryHeading_->setText(heading);
        breadcrumb_->setText(QStringLiteral("Library  >  ") + heading);
        libraryDescription_->setText(category == Queue
            ? QStringLiteral("Your current playback queue.")
            : QStringLiteral("Double-click an item to play it."));
        table_->setSortingEnabled(false);
        table_->setRowCount(0);
        const QString query = search_->text().trimmed();
        const auto addRow = [this](const MediaEntry &entry) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            auto *title = new QTableWidgetItem(icon(entry.kind == QStringLiteral("Video")
                                                  ? QStringLiteral("library/videos")
                                                  : QStringLiteral("library/music")), entry.title);
            title->setData(Qt::UserRole, entry.path);
            title->setToolTip(entry.path);
            table_->setItem(row, 0, title);
            table_->setItem(row, 1, new QTableWidgetItem(entry.durationMs < 0
                ? QString() : clockTime(entry.durationMs)));
            table_->setItem(row, 2, new QTableWidgetItem(entry.artist));
            table_->setItem(row, 3, new QTableWidgetItem(entry.album));
            table_->setItem(row, 4, new QTableWidgetItem(entry.genre));
            table_->setItem(row, 5, new QTableWidgetItem(entry.kind));
            table_->setRowHeight(row, 33);
        };
        if (category == Queue) {
            for (const QString &path : queue_) {
                const QFileInfo info(path);
                const MediaEntry entry{path, info.completeBaseName(), QString(), info.dir().dirName(),
                                       QString(), -1,
                                       isVideo(path) ? QStringLiteral("Video") : QStringLiteral("Music")};
                if (query.isEmpty() || entry.title.contains(query, Qt::CaseInsensitive)) {
                    addRow(entry);
                }
            }
        } else {
            for (const MediaEntry &entry : media_) {
                if (((category == Music || category == Artists || category == Albums || category == Genres)
                       && entry.kind != QStringLiteral("Music")) ||
                    (category == Videos && entry.kind != QStringLiteral("Video")) ||
                    (!query.isEmpty() && !entry.title.contains(query, Qt::CaseInsensitive) &&
                     !entry.album.contains(query, Qt::CaseInsensitive) &&
                     !entry.artist.contains(query, Qt::CaseInsensitive) &&
                     !entry.genre.contains(query, Qt::CaseInsensitive))) {
                    continue;
                }
                addRow(entry);
            }
        }
        table_->setSortingEnabled(true);
        if (category == Artists) table_->sortItems(2);
        if (category == Albums) table_->sortItems(3);
        if (category == Genres) table_->sortItems(4);
        const bool empty = table_->rowCount() == 0;
        libraryContent_->setCurrentIndex(empty ? 1 : 0);
        if (empty) {
            emptyText_->setText(!query.isEmpty() ? QStringLiteral("No matching media found")
                : category == Queue ? QStringLiteral("Your play list is empty")
                : QStringLiteral("No %1 found yet").arg(heading.toLower()));
        }
    }

    void rebuildQueue()
    {
        queueList_->clear();
        for (const QString &path : queue_) {
            queueList_->addItem(QFileInfo(path).completeBaseName());
        }
        if (categories_->currentItem()->data(Qt::UserRole).toInt() == Queue) {
            updateLibrary();
        }
    }

    void playIndex(int index)
    {
        if (!player_ || index < 0 || index >= queue_.size()) {
            return;
        }
        const QString source = queue_.at(index);
        libvlc_media_player_stop(player_);
        currentIndex_ = index;
        const QUrl url(source);
        const bool local = url.scheme().isEmpty() || url.isLocalFile();
        const QString path = url.isLocalFile() ? url.toLocalFile() : source;
        const QByteArray bytes = (local ? path : source).toUtf8();
        libvlc_media_t *media = local ? libvlc_media_new_path(vlc_, bytes.constData())
                                      : libvlc_media_new_location(vlc_, bytes.constData());
        if (!media) {
            statusBar()->showMessage(QStringLiteral("Could not open %1").arg(source), 10000);
            return;
        }
        libvlc_media_player_set_media(player_, media);
        libvlc_media_release(media);
        const bool video = !local || isVideo(path);
        artStack_->setCurrentIndex(video ? 1 : 0);
        if (video && QGuiApplication::platformName() == QStringLiteral("xcb")) {
            libvlc_media_player_set_xwindow(player_, static_cast<uint32_t>(videoHost_->winId()));
        }
        QString title = QFileInfo(path).completeBaseName();
        QString subtitle = QFileInfo(path).dir().dirName();
        if (local && !video) {
            const QByteArray name = path.toUtf8();
            const TagLib::FileRef tags(name.constData(), false);
            if (!tags.isNull() && tags.tag()) {
                const auto value = [](const TagLib::String &text) {
                    return QString::fromStdString(text.to8Bit(true));
                };
                if (!tags.tag()->title().isEmpty()) title = value(tags.tag()->title());
                const QString artist = value(tags.tag()->artist());
                const QString album = value(tags.tag()->album());
                if (!artist.isEmpty() && !album.isEmpty()) subtitle = artist + QStringLiteral("  •  ") + album;
                else if (!artist.isEmpty()) subtitle = artist;
                else if (!album.isEmpty()) subtitle = album;
            }
        }
        nowTitle_->setText(title);
        nowSubtitle_->setText(subtitle);
        if (!video) {
            QPixmap art;
            for (const QString &name : {QStringLiteral("cover.jpg"), QStringLiteral("folder.jpg"),
                                        QStringLiteral("cover.png"), QStringLiteral("folder.png")}) {
                const QString candidate = QFileInfo(path).dir().filePath(name);
                if (QFileInfo::exists(candidate)) {
                    art.load(candidate);
                    break;
                }
            }
            if (art.isNull()) {
                const QByteArray name = path.toUtf8();
                const TagLib::FileRef tags(name.constData(), false);
                if (!tags.isNull()) {
                    for (const auto &picture : tags.complexProperties("PICTURE")) {
                        const auto found = picture.find("data");
                        if (found == picture.end()) continue;
                        const auto bytes = found->second.toByteVector();
                        if (art.loadFromData(reinterpret_cast<const uchar *>(bytes.data()),
                                             static_cast<int>(bytes.size()))) break;
                    }
                }
            }
            if (art.isNull()) {
                art.load(iconPath(QStringLiteral("library/audio-art")));
            }
            art_->setPixmap(art.scaled(280, 280, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        queueList_->setCurrentRow(index);
        if (libvlc_media_player_play(player_) != 0) {
            statusBar()->showMessage(QStringLiteral("VLC could not play %1").arg(path), 10000);
            return;
        }
        showNowPlaying();
        statusBar()->showMessage(QStringLiteral("Playing %1").arg(QFileInfo(path).fileName()), 5000);
    }

    void openFiles()
    {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, QStringLiteral("Open media"),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            QStringLiteral("Media files (*.mp3 *.m4a *.mp4 *.m4v *.wav *.flac *.ogg *.wma *.mkv *.avi *.mov *.wmv *.webm *.mpg *.mpeg);;All files (*)"));
        if (files.isEmpty()) {
            return;
        }
        queue_ = files;
        rebuildQueue();
        playIndex(0);
    }

    void addFolder()
    {
        const QString folder = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Add a media folder"), QDir::homePath());
        if (folder.isEmpty()) {
            return;
        }
        QSettings settings(QStringLiteral("Aero7"), QStringLiteral("MediaPlayer"));
        QStringList folders = settings.value(QStringLiteral("libraryFolders")).toStringList();
        if (!folders.contains(folder)) {
            folders.append(folder);
            settings.setValue(QStringLiteral("libraryFolders"), folders);
        }
        scanLibrary();
    }

    void openStream()
    {
        bool accepted = false;
        const QString location = QInputDialog::getText(
            this, QStringLiteral("Open network stream"), QStringLiteral("Network URL:"),
            QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted || !QUrl(location).isValid() || QUrl(location).scheme().isEmpty()) {
            return;
        }
        queue_ = {location};
        rebuildQueue();
        playIndex(0);
    }

    void savePlaylist()
    {
        if (queue_.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Add media before saving a playlist."), 5000);
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save playlist"),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
                + QStringLiteral("/Now Playing.m3u"),
            QStringLiteral("M3U playlist (*.m3u)"));
        if (path.isEmpty()) return;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            statusBar()->showMessage(QStringLiteral("Could not save the playlist."), 10000);
            return;
        }
        QTextStream stream(&file);
        stream << "#EXTM3U\n";
        for (const QString &item : queue_) {
            stream << item << '\n';
        }
        stream.flush();
        if (!file.commit()) {
            statusBar()->showMessage(QStringLiteral("Could not finish saving the playlist."), 10000);
        }
    }

    void openPlaylist()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Open playlist"),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            QStringLiteral("M3U playlist (*.m3u *.m3u8)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            statusBar()->showMessage(QStringLiteral("Could not open the playlist."), 10000);
            return;
        }
        QStringList files;
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
            const QUrl url(line);
            const QString candidate = url.isLocalFile() ? url.toLocalFile()
                : url.scheme().isEmpty() ? QFileInfo(QFileInfo(path).dir(), line).absoluteFilePath()
                                         : line;
            if ((!url.scheme().isEmpty() && !url.isLocalFile()) || QFileInfo::exists(candidate)) {
                files.append(candidate);
            }
        }
        if (files.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("No available media in this playlist."), 10000);
            return;
        }
        queue_ = files;
        rebuildQueue();
        playIndex(0);
    }

    void togglePlay()
    {
        if (!player_) {
            return;
        }
        if (currentIndex_ < 0) {
            openFiles();
        } else if (libvlc_media_player_is_playing(player_)) {
            libvlc_media_player_set_pause(player_, 1);
        } else {
            libvlc_media_player_play(player_);
        }
    }

    void stop()
    {
        if (player_) {
            libvlc_media_player_stop(player_);
        }
    }

    void next()
    {
        if (currentIndex_ + 1 < queue_.size()) {
            playIndex(currentIndex_ + 1);
        }
    }

    void previous()
    {
        if (currentIndex_ > 0) {
            playIndex(currentIndex_ - 1);
        } else if (player_) {
            libvlc_media_player_set_position(player_, 0.0f);
        }
    }

    void refreshPlayback()
    {
        if (!player_) {
            return;
        }
        const libvlc_state_t state = libvlc_media_player_get_state(player_);
        play_->setIcon(icon(state == libvlc_Playing ? QStringLiteral("media-playback-pause")
                                              : QStringLiteral("media-playback-start")));
        if (state == libvlc_Ended && currentIndex_ + 1 < queue_.size()) {
            next();
            return;
        }
        elapsed_->setText(clockTime(libvlc_media_player_get_time(player_)) + QStringLiteral(" / ")
                          + clockTime(libvlc_media_player_get_length(player_)));
        if (!progress_->isSliderDown()) {
            const float position = libvlc_media_player_get_position(player_);
            progress_->setValue(position < 0 ? 0 : static_cast<int>(position * 1000));
        }
    }

    void showLibrary()
    {
        pages_->setCurrentIndex(0);
        search_->show();
        const QString heading = libraryHeading_ ? libraryHeading_->text() : QStringLiteral("Music");
        breadcrumb_->setText(QStringLiteral("Library  >  ") + heading);
    }

    void showNowPlaying()
    {
        pages_->setCurrentIndex(1);
        search_->hide();
        breadcrumb_->setText(QStringLiteral("Now Playing"));
    }

    void toggleFullScreen()
    {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    }

    libvlc_instance_t *vlc_ = nullptr;
    libvlc_media_player_t *player_ = nullptr;
    QTimer clock_;
    QStackedWidget *pages_ = nullptr;
    QStackedWidget *libraryContent_ = nullptr;
    QStackedWidget *artStack_ = nullptr;
    QTableWidget *table_ = nullptr;
    QListWidget *categories_ = nullptr;
    QListWidget *queueList_ = nullptr;
    QFrame *videoHost_ = nullptr;
    QFrame *queuePanel_ = nullptr;
    QLabel *breadcrumb_ = nullptr;
    QLabel *libraryHeading_ = nullptr;
    QLabel *libraryDescription_ = nullptr;
    QLabel *emptyText_ = nullptr;
    QLabel *nowTitle_ = nullptr;
    QLabel *nowSubtitle_ = nullptr;
    QLabel *art_ = nullptr;
    QLabel *elapsed_ = nullptr;
    QLineEdit *search_ = nullptr;
    QSlider *progress_ = nullptr;
    QToolButton *play_ = nullptr;
    QList<MediaEntry> media_;
    QStringList queue_;
    int currentIndex_ = -1;
};

int main(int argc, char **argv)
{
    // LibVLC 3 embeds video in an X11 window. XWayland lets KWin give our
    // ordinary Qt window the configured Aero7 decoration on Wayland sessions.
    if (!qEnvironmentVariableIsEmpty("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Aero7 Media Player"));
    application.setOrganizationName(QStringLiteral("Aero7"));
    application.setWindowIcon(icon(QStringLiteral("app/64")));

    QStringList files;
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        const QUrl url(argument);
        files.append(url.isLocalFile() ? url.toLocalFile() : argument);
    }
    MediaWindow window(files);
    window.show();
    return application.exec();
}
