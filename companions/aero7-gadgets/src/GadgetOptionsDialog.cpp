#include "GadgetOptionsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QUrl>
#include <QUrlQuery>

#include <utility>

namespace {
struct FeedEntry { QString name; QString url; };

QString feedsPath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/aero7/gadgets");
    QDir().mkpath(directory);
    return directory + QStringLiteral("/feeds.json");
}

QList<FeedEntry> loadFeeds()
{
    QList<FeedEntry> feeds;
    QFile file(feedsPath());
    if (file.open(QIODevice::ReadOnly)) {
        for (const QJsonValue value : QJsonDocument::fromJson(file.readAll()).array()) {
            const QJsonObject item = value.toObject();
            const QString name = item.value(QStringLiteral("name")).toString();
            const QString url = item.value(QStringLiteral("url")).toString();
            if (!name.isEmpty() && !url.isEmpty()) feeds << FeedEntry{name, url};
        }
    }
    if (feeds.isEmpty()) {
        feeds = {
            {QStringLiteral("Aero7 Releases"), QStringLiteral("https://github.com/memegeko/aero7-repo/releases.atom")},
            {QStringLiteral("Arch Linux News"), QStringLiteral("https://archlinux.org/feeds/news/")},
            {QStringLiteral("KDE News"), QStringLiteral("https://kde.org/announcements/index.xml")},
        };
    }
    return feeds;
}

void saveFeeds(const QList<FeedEntry> &feeds)
{
    QJsonArray array;
    for (const FeedEntry &feed : feeds) array.append(QJsonObject{{QStringLiteral("name"), feed.name}, {QStringLiteral("url"), feed.url}});
    QSaveFile file(feedsPath());
    if (file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument(array).toJson(QJsonDocument::Indented)); file.commit(); }
}

class FeedManagerDialog final : public QDialog
{
public:
    explicit FeedManagerDialog(QWidget *parent = nullptr) : QDialog(parent), m_feeds(loadFeeds())
    {
        setWindowTitle(QStringLiteral("Manage Feeds")); setMinimumSize(440, 290);
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(QStringLiteral("Feeds available to the Feed Headlines gadget:"), this));
        m_list = new QListWidget(this); layout->addWidget(m_list, 1); rebuild();
        auto *row = new QHBoxLayout;
        auto *add = new QPushButton(QStringLiteral("Add..."), this);
        auto *remove = new QPushButton(QStringLiteral("Remove"), this);
        row->addWidget(add); row->addWidget(remove); row->addStretch(); layout->addLayout(row);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this); layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
        connect(add, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            const QString name = QInputDialog::getText(this, QStringLiteral("Add Feed"), QStringLiteral("Name:"), QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok || name.isEmpty()) return;
            const QString url = QInputDialog::getText(this, QStringLiteral("Add Feed"), QStringLiteral("RSS or Atom URL:"), QLineEdit::Normal, QStringLiteral("https://"), &ok).trimmed();
            const QUrl parsed = QUrl::fromUserInput(url);
            if (!ok || (parsed.scheme() != QStringLiteral("http") && parsed.scheme() != QStringLiteral("https"))) return;
            m_feeds << FeedEntry{name, parsed.toString()}; saveFeeds(m_feeds); rebuild();
        });
        connect(remove, &QPushButton::clicked, this, [this]() {
            const int row = m_list->currentRow(); if (row < 0 || row >= m_feeds.size()) return;
            m_feeds.removeAt(row); saveFeeds(m_feeds); rebuild();
        });
    }
private:
    void rebuild() { m_list->clear(); for (const FeedEntry &feed : std::as_const(m_feeds)) { auto *item = new QListWidgetItem(feed.name + QStringLiteral("\n") + feed.url, m_list); item->setToolTip(feed.url); } if (m_list->count()) m_list->setCurrentRow(0); }
    QList<FeedEntry> m_feeds;
    QListWidget *m_list = nullptr;
};
}

GadgetOptionsDialog::GadgetOptionsDialog(const GadgetDefinition &definition, const QJsonObject &settings, QWidget *parent)
    : QDialog(parent)
    , m_definition(definition)
    , m_original(settings)
{
    setWindowTitle(m_definition.name + QStringLiteral(" Options"));
    setModal(true);
    setMinimumWidth(390);

    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(QStringLiteral("<b>") + m_definition.name + QStringLiteral(" Options</b>"), this);
    layout->addWidget(heading);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layout->addLayout(form);

    auto addLine = [&](const QString &key, const QString &label, const QString &fallback = {}) {
        auto *line = new QLineEdit(settings.value(key).toString(fallback), this);
        form->addRow(label, line); m_lines.insert(key, line); return line;
    };
    auto addCombo = [&](const QString &key, const QString &label, const QStringList &items, const QString &fallback = {}) {
        auto *combo = new QComboBox(this); combo->addItems(items);
        const QString value = settings.value(key).toString(fallback);
        int index = combo->findText(value, Qt::MatchFixedString); if (index >= 0) combo->setCurrentIndex(index);
        form->addRow(label, combo); m_combos.insert(key, combo); return combo;
    };
    auto addSpin = [&](const QString &key, const QString &label, int minimum, int maximum, int fallback) {
        auto *spin = new QSpinBox(this); spin->setRange(minimum, maximum); spin->setValue(settings.value(key).toInt(fallback));
        form->addRow(label, spin); m_spins.insert(key, spin); return spin;
    };
    auto addDouble = [&](const QString &key, const QString &label, double minimum, double maximum, double fallback) {
        auto *spin = new QDoubleSpinBox(this); spin->setRange(minimum, maximum); spin->setDecimals(4); spin->setValue(settings.value(key).toDouble(fallback));
        form->addRow(label, spin); m_doubles.insert(key, spin); return spin;
    };
    auto addCheck = [&](const QString &key, const QString &label, bool fallback) {
        auto *check = new QCheckBox(label, this); check->setChecked(settings.value(key).toBool(fallback));
        form->addRow(QString(), check); m_checks.insert(key, check); return check;
    };

    const QString id = definition.id;
    if (id.endsWith(QStringLiteral("clock"))) {
        auto *face = new QComboBox(this); face->addItems({QStringLiteral("Classic"), QStringLiteral("Dark"), QStringLiteral("Aero")});
        face->setCurrentIndex(qBound(0, settings.value(QStringLiteral("face")).toInt(0), 2)); form->addRow(QStringLiteral("Clock:"), face); m_combos.insert(QStringLiteral("face"), face);
        addLine(QStringLiteral("label"), QStringLiteral("Clock name:"));
        QStringList zones; for (const QByteArray &zone : QTimeZone::availableTimeZoneIds()) zones << QString::fromUtf8(zone);
        auto *timezone = addCombo(QStringLiteral("timezone"), QStringLiteral("Time zone:"), zones, QStringLiteral("Europe/Amsterdam"));
        timezone->setEditable(true); timezone->setInsertPolicy(QComboBox::NoInsert);
        addCheck(QStringLiteral("seconds"), QStringLiteral("Show second hand"), true);
    } else if (id.endsWith(QStringLiteral("calendar"))) {
        addCombo(QStringLiteral("firstDay"), QStringLiteral("First day of week:"), {QStringLiteral("Monday"), QStringLiteral("Sunday")}, QStringLiteral("Monday"));
        addCheck(QStringLiteral("highlightToday"), QStringLiteral("Highlight today"), true);
        addCheck(QStringLiteral("weekNumbers"), QStringLiteral("Show week numbers"), false);
    } else if (id.endsWith(QStringLiteral("currency"))) {
        const QStringList currencies{QStringLiteral("EUR"), QStringLiteral("USD"), QStringLiteral("GBP"), QStringLiteral("JPY"), QStringLiteral("CHF"), QStringLiteral("CAD"), QStringLiteral("AUD"), QStringLiteral("CNY")};
        addCombo(QStringLiteral("base"), QStringLiteral("From:"), currencies, QStringLiteral("EUR"));
        addCombo(QStringLiteral("target"), QStringLiteral("To:"), currencies, QStringLiteral("USD"));
        addDouble(QStringLiteral("amount"), QStringLiteral("Amount:"), 0.01, 100000000.0, 1.0)->setDecimals(2);
    } else if (id.endsWith(QStringLiteral("feeds"))) {
        auto *feedRow = new QWidget(this); auto *feedLayout = new QHBoxLayout(feedRow); feedLayout->setContentsMargins(0, 0, 0, 0);
        auto *feed = new QComboBox(feedRow); feed->setEditable(true);
        const QString selectedFeed = settings.value(QStringLiteral("feed")).toString(QStringLiteral("https://github.com/memegeko/aero7-repo/releases.atom"));
        auto rebuildFeeds = [feed, selectedFeed]() {
            const QString current = feed->currentData().toString().isEmpty() ? selectedFeed : feed->currentData().toString();
            feed->clear(); int selected = -1;
            for (const FeedEntry &entry : loadFeeds()) { feed->addItem(entry.name, entry.url); if (entry.url == current) selected = feed->count() - 1; }
            if (selected < 0) { feed->addItem(QStringLiteral("Custom Feed"), current); selected = feed->count() - 1; }
            feed->setCurrentIndex(selected);
        };
        rebuildFeeds(); m_combos.insert(QStringLiteral("feed"), feed);
        auto *manage = new QPushButton(QStringLiteral("Manage feeds..."), feedRow); feedLayout->addWidget(feed, 1); feedLayout->addWidget(manage);
        form->addRow(QStringLiteral("Feed:"), feedRow);
        connect(manage, &QPushButton::clicked, this, [this, rebuildFeeds]() { FeedManagerDialog dialog(this); dialog.exec(); rebuildFeeds(); });
        addSpin(QStringLiteral("refreshMinutes"), QStringLiteral("Refresh (minutes):"), 5, 1440, 30);
        addSpin(QStringLiteral("count"), QStringLiteral("Number of headlines:"), 1, 20, 5);
        addCheck(QStringLiteral("openLinks"), QStringLiteral("Open links in default browser"), true);
    } else if (id.endsWith(QStringLiteral("picturepuzzle"))) {
        addCombo(QStringLiteral("image"), QStringLiteral("Image:"), {QStringLiteral("aero7-flower"), QStringLiteral("aero7-aurora"), QStringLiteral("aero7-landscape"), QStringLiteral("custom")}, QStringLiteral("aero7-flower"));
        addCombo(QStringLiteral("difficulty"), QStringLiteral("Difficulty:"), {QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5")}, QStringLiteral("4"));
        auto *imageRow = new QWidget(this); auto *imageLayout = new QHBoxLayout(imageRow); imageLayout->setContentsMargins(0, 0, 0, 0);
        auto *customImage = new QLineEdit(settings.value(QStringLiteral("customImage")).toString(), imageRow);
        auto *browse = new QPushButton(QStringLiteral("Browse..."), imageRow); imageLayout->addWidget(customImage, 1); imageLayout->addWidget(browse);
        form->addRow(QStringLiteral("Custom image:"), imageRow); m_lines.insert(QStringLiteral("customImage"), customImage);
        connect(browse, &QPushButton::clicked, this, [this, customImage]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select puzzle image"), customImage->text(),
                                                                   QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
            if (!selected.isEmpty()) customImage->setText(selected);
        });
        auto *newPuzzle = new QPushButton(QStringLiteral("New puzzle"), this); form->addRow(QString(), newPuzzle);
        connect(newPuzzle, &QPushButton::clicked, this, &QDialog::accept);
    } else if (id.endsWith(QStringLiteral("slideshow"))) {
        auto *row = new QWidget(this); auto *rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *folder = new QLineEdit(settings.value(QStringLiteral("folder")).toString(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)), row);
        auto *browse = new QPushButton(QStringLiteral("Browse..."), row); rowLayout->addWidget(folder, 1); rowLayout->addWidget(browse); form->addRow(QStringLiteral("Folder:"), row); m_lines.insert(QStringLiteral("folder"), folder);
        connect(browse, &QPushButton::clicked, this, [this, folder]() { const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Select picture folder"), folder->text()); if (!selected.isEmpty()) folder->setText(selected); });
        addSpin(QStringLiteral("delaySeconds"), QStringLiteral("Show each picture (seconds):"), 2, 3600, 10);
        addCombo(QStringLiteral("transition"), QStringLiteral("Transition:"), {QStringLiteral("fade"), QStringLiteral("none")}, QStringLiteral("fade"));
        addCheck(QStringLiteral("shuffle"), QStringLiteral("Shuffle pictures"), false);
    } else if (id.endsWith(QStringLiteral("weather"))) {
        auto *locationRow = new QWidget(this); auto *locationLayout = new QHBoxLayout(locationRow); locationLayout->setContentsMargins(0, 0, 0, 0);
        auto *location = new QLineEdit(settings.value(QStringLiteral("location")).toString(QStringLiteral("Doetinchem")), locationRow);
        auto *findLocation = new QPushButton(QStringLiteral("Find"), locationRow); locationLayout->addWidget(location, 1); locationLayout->addWidget(findLocation);
        form->addRow(QStringLiteral("Location:"), locationRow); m_lines.insert(QStringLiteral("location"), location);
        auto *latitude = addDouble(QStringLiteral("latitude"), QStringLiteral("Latitude:"), -90.0, 90.0, 51.965);
        auto *longitude = addDouble(QStringLiteral("longitude"), QStringLiteral("Longitude:"), -180.0, 180.0, 6.288);
        connect(findLocation, &QPushButton::clicked, this, [this, location, latitude, longitude, findLocation]() {
            const QString queryText = location->text().trimmed(); if (queryText.isEmpty()) return;
            QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search")); QUrlQuery query;
            query.addQueryItem(QStringLiteral("name"), queryText); query.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
            query.addQueryItem(QStringLiteral("language"), QStringLiteral("en")); url.setQuery(query);
            auto *network = new QNetworkAccessManager(this);
            network->setTransferTimeout(15000);
            network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
            findLocation->setEnabled(false); findLocation->setText(QStringLiteral("Finding..."));
            QNetworkReply *reply = network->get(QNetworkRequest(url));
            connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
                constexpr qint64 maximumBytes = 1024 * 1024;
                if (received > maximumBytes || total > maximumBytes) reply->abort();
            });
            connect(reply, &QNetworkReply::finished, this, [reply, network, latitude, longitude, location, findLocation]() {
                const QJsonArray results = QJsonDocument::fromJson(reply->read(1024 * 1024)).object().value(QStringLiteral("results")).toArray();
                reply->deleteLater(); network->deleteLater(); findLocation->setEnabled(true); findLocation->setText(QStringLiteral("Find"));
                if (results.isEmpty()) return;
                const QJsonObject result = results.first().toObject(); latitude->setValue(result.value(QStringLiteral("latitude")).toDouble()); longitude->setValue(result.value(QStringLiteral("longitude")).toDouble());
                const QString country = result.value(QStringLiteral("country")).toString();
                location->setText(result.value(QStringLiteral("name")).toString() + (country.isEmpty() ? QString() : QStringLiteral(", ") + country));
            });
        });
        addCombo(QStringLiteral("unit"), QStringLiteral("Temperature:"), {QStringLiteral("celsius"), QStringLiteral("fahrenheit")}, QStringLiteral("celsius"));
        addSpin(QStringLiteral("refreshMinutes"), QStringLiteral("Refresh (minutes):"), 15, 360, 30);
    } else if (id.endsWith(QStringLiteral("mediacenter"))) {
        addCombo(QStringLiteral("player"), QStringLiteral("Media player:"), {QStringLiteral("auto")}, QStringLiteral("auto"));
        auto *info = new QLabel(QStringLiteral("Aero7 automatically follows the currently playing MPRIS2 application."), this);
        info->setWordWrap(true); form->addRow(QString(), info);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QJsonObject GadgetOptionsDialog::settings() const
{
    QJsonObject result = m_original;
    for (auto it = m_lines.cbegin(); it != m_lines.cend(); ++it) result.insert(it.key(), it.value()->text().trimmed());
    for (auto it = m_combos.cbegin(); it != m_combos.cend(); ++it) {
        if (it.key() == QStringLiteral("difficulty")) result.insert(it.key(), it.value()->currentText().toInt());
        else if (it.key() == QStringLiteral("face")) result.insert(it.key(), it.value()->currentIndex());
        else if (it.key() == QStringLiteral("firstDay")) result.insert(it.key(), it.value()->currentText() == QStringLiteral("Monday") ? 1 : 7);
        else if (it.key() == QStringLiteral("feed")) {
            const QString text = it.value()->currentText().trimmed();
            result.insert(it.key(), text.startsWith(QStringLiteral("http://")) || text.startsWith(QStringLiteral("https://"))
                                      ? text : it.value()->currentData().toString());
        }
        else result.insert(it.key(), it.value()->currentText());
    }
    for (auto it = m_spins.cbegin(); it != m_spins.cend(); ++it) result.insert(it.key(), it.value()->value());
    for (auto it = m_doubles.cbegin(); it != m_doubles.cend(); ++it) result.insert(it.key(), it.value()->value());
    for (auto it = m_checks.cbegin(); it != m_checks.cend(); ++it) result.insert(it.key(), it.value()->isChecked());
    return result;
}
