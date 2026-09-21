#include "GadgetOptionsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
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
#include <cmath>

namespace {
struct FeedEntry { QString name; QString url; };
constexpr qint64 maximumFeedCatalogBytes = 1024 * 1024;

void showLookupStatus(QLabel *label, const QString &message)
{
    label->setText(message);
    label->show();
    auto *dialog = label->window();
    // A newly shown wrapped label otherwise gets squeezed into the old dialog
    // height. Lay out its actual width before reserving enough text height.
    if (dialog->layout()) dialog->layout()->activate();
    label->setMinimumHeight(qMax(0, label->heightForWidth(label->width())));
    dialog->adjustSize();
}

QString feedsPath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/aero7/gadgets");
    QDir().mkpath(directory);
    return directory + QStringLiteral("/feeds.json");
}

bool validFeedAddress(const QString &url)
{
    const QUrl parsed(url, QUrl::StrictMode);
    return parsed.isValid() && !parsed.host().isEmpty()
        && (parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https"));
}

QList<FeedEntry> loadFeeds(QString *loadError = nullptr)
{
    if (loadError) loadError->clear();
    QList<FeedEntry> feeds;
    QFile file(feedsPath());
    const QFileInfo info(file);
    // Defaults belong only to first use. A damaged existing catalog must never
    // become an editable fallback that overwrites the user's original bytes.
    if (!info.exists() && !info.isSymLink()) {
        return {
            {QStringLiteral("Aero7 Releases"), QStringLiteral("https://github.com/memegeko/aero7-repo/releases.atom")},
            {QStringLiteral("Arch Linux News"), QStringLiteral("https://archlinux.org/feeds/news/")},
            {QStringLiteral("KDE News"), QStringLiteral("https://kde.org/announcements/index.xml")},
        };
    }
    const auto fail = [loadError](const QString &reason) -> QList<FeedEntry> {
        if (loadError) *loadError = QStringLiteral("The saved feed list could not be loaded. %1 The original file has not been changed. Restore or repair feeds.json, then select Try Again.").arg(reason);
        return {};
    };
    if (!info.isFile() || !file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Check that the configuration file is readable."));
    const auto bytes = file.read(maximumFeedCatalogBytes + 1);
    if (file.error() != QFileDevice::NoError)
        return fail(QStringLiteral("The configuration file could not be read completely."));
    if (bytes.size() > maximumFeedCatalogBytes)
        return fail(QStringLiteral("The configuration file exceeds the 1 MiB limit."));
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return fail(QStringLiteral("The configuration is not a valid feed-list document."));
    for (const QJsonValue value : document.array()) {
        const QJsonObject item = value.toObject();
        const QString name = item.value(QStringLiteral("name")).toString();
        const QString url = item.value(QStringLiteral("url")).toString();
        if (!value.isObject() || name.trimmed().isEmpty() || !validFeedAddress(url))
            return fail(QStringLiteral("The configuration contains an invalid feed entry."));
        feeds << FeedEntry{name, url};
    }
    // A saved empty catalog is an explicit choice, not first-run setup.
    return feeds;
}

bool saveFeeds(const QList<FeedEntry> &feeds)
{
    QJsonArray array;
    for (const FeedEntry &feed : feeds) array.append(QJsonObject{{QStringLiteral("name"), feed.name}, {QStringLiteral("url"), feed.url}});
    QSaveFile file(feedsPath());
    const auto bytes = QJsonDocument(array).toJson(QJsonDocument::Indented);
    return bytes.size() <= maximumFeedCatalogBytes && file.open(QIODevice::WriteOnly)
        && file.write(bytes) == bytes.size() && file.commit();
}

class FeedManagerDialog final : public QDialog
{
public:
    explicit FeedManagerDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        m_feeds = loadFeeds(&m_loadError);
        setWindowTitle(QStringLiteral("Manage Feeds")); setMinimumSize(440, 290);
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(QStringLiteral("Feeds available to the Feed Headlines gadget:"), this));
        m_list = new QListWidget(this); layout->addWidget(m_list, 1); rebuild();
        m_saveStatus = new QLabel(this);
        m_saveStatus->setObjectName(QStringLiteral("feedSaveStatus"));
        m_saveStatus->setWordWrap(true);
        m_saveStatus->hide();
        layout->addWidget(m_saveStatus);
        auto *row = new QHBoxLayout;
        auto *add = m_add = new QPushButton(QStringLiteral("Add..."), this);
        auto *remove = m_remove = new QPushButton(QStringLiteral("Remove"), this);
        m_retry = new QPushButton(QStringLiteral("Try Again"), this);
        row->addWidget(add); row->addWidget(remove); row->addStretch(); layout->addLayout(row);
        row->addWidget(m_retry);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this); layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
        connect(add, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            const QString name = QInputDialog::getText(this, QStringLiteral("Add Feed"), QStringLiteral("Name:"), QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok || name.isEmpty()) return;
            const QString url = QInputDialog::getText(this, QStringLiteral("Add Feed"), QStringLiteral("RSS or Atom URL:"), QLineEdit::Normal, QStringLiteral("https://"), &ok).trimmed();
            if (!ok) return;
            // Do not reinterpret malformed explicit URLs as another host/path.
            const QUrl parsed(url, QUrl::StrictMode);
            if (!validFeedAddress(url)) {
                showLookupStatus(m_saveStatus, QStringLiteral("Could not add feed. Enter a valid HTTP or HTTPS feed address with a host name. No changes were made."));
                return;
            }
            auto updated = m_feeds;
            updated << FeedEntry{name, parsed.toString()};
            persist(updated);
        });
        connect(remove, &QPushButton::clicked, this, [this]() {
            const int row = m_list->currentRow(); if (row < 0 || row >= m_feeds.size()) return;
            auto updated = m_feeds;
            updated.removeAt(row);
            persist(updated);
        });
        connect(m_retry, &QPushButton::clicked, this, [this]() {
            m_feeds = loadFeeds(&m_loadError);
            rebuild();
            showLoadState();
        });
        showLoadState();
    }
private:
    void showLoadState()
    {
        const bool loaded = m_loadError.isEmpty();
        m_add->setEnabled(loaded);
        m_remove->setEnabled(loaded);
        m_retry->setVisible(!loaded);
        if (!loaded) {
            showLookupStatus(m_saveStatus, m_loadError);
        } else {
            m_saveStatus->clear();
            m_saveStatus->hide();
            m_saveStatus->setMinimumHeight(0);
        }
    }
    void persist(const QList<FeedEntry> &updated)
    {
        if (!m_loadError.isEmpty()) return;
        if (!saveFeeds(updated)) {
            showLookupStatus(m_saveStatus, QStringLiteral("Could not save feeds. Check write permissions and free space, and keep the feed list below 1 MiB. No changes were made."));
            return;
        }
        m_feeds = updated;
        m_saveStatus->clear();
        m_saveStatus->hide();
        m_saveStatus->setMinimumHeight(0);
        rebuild();
    }
    void rebuild() { m_list->clear(); for (const FeedEntry &feed : std::as_const(m_feeds)) { auto *item = new QListWidgetItem(feed.name + QStringLiteral("\n") + feed.url, m_list); item->setToolTip(feed.url); } if (m_list->count()) m_list->setCurrentRow(0); }
    QList<FeedEntry> m_feeds;
    QString m_loadError;
    QListWidget *m_list = nullptr;
    QLabel *m_saveStatus = nullptr;
    QPushButton *m_add = nullptr;
    QPushButton *m_remove = nullptr;
    QPushButton *m_retry = nullptr;
};
}

GadgetOptionsDialog::GadgetOptionsDialog(const GadgetDefinition &definition, const QJsonObject &settings, QWidget *parent,
                                       QNetworkAccessManager *lookupNetwork)
    : QDialog(parent)
    , m_definition(definition)
    , m_original(settings)
    , m_lookupNetwork(lookupNetwork)
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
        auto *firstDay = addCombo(QStringLiteral("firstDay"), QStringLiteral("First day of week:"),
                                  {QStringLiteral("Monday"), QStringLiteral("Sunday")}, QStringLiteral("Monday"));
        // The saved setting is an ISO weekday number, not the displayed label.
        firstDay->setCurrentIndex(settings.value(QStringLiteral("firstDay")).toInt(1) == 7 ? 1 : 0);
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
            const QString entered = feed->currentText().trimmed();
            const QString current = entered.startsWith(QStringLiteral("http://")) || entered.startsWith(QStringLiteral("https://"))
                ? entered : (feed->currentData().toString().isEmpty() ? selectedFeed : feed->currentData().toString());
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
        auto *difficulty = addCombo(QStringLiteral("difficulty"), QStringLiteral("Difficulty:"),
                                     {QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5")}, QStringLiteral("4"));
        difficulty->setCurrentIndex(qBound(3, settings.value(QStringLiteral("difficulty")).toInt(4), 5) - 3);
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
        connect(newPuzzle, &QPushButton::clicked, this, [this]() {
            m_newPuzzleRequested = true;
            accept();
        });
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
        auto *lookupStatus = new QLabel(this);
        lookupStatus->setObjectName(QStringLiteral("locationLookupStatus"));
        lookupStatus->setTextFormat(Qt::PlainText);
        lookupStatus->setWordWrap(true);
        lookupStatus->hide();
        form->addRow(lookupStatus);
        const auto locationChanged = [this]() { ++m_locationRevision; };
        connect(location, &QLineEdit::textChanged, this, locationChanged);
        connect(latitude, &QDoubleSpinBox::valueChanged, this, locationChanged);
        connect(longitude, &QDoubleSpinBox::valueChanged, this, locationChanged);
        connect(findLocation, &QPushButton::clicked, this, [this, location, latitude, longitude, findLocation, lookupStatus]() {
            const QString queryText = location->text().trimmed();
            if (queryText.isEmpty()) {
                showLookupStatus(lookupStatus, QStringLiteral("Enter a city or place name to search."));
                return;
            }
            const quint64 revision = m_locationRevision;
            lookupStatus->clear();
            lookupStatus->hide();
            lookupStatus->setMinimumHeight(0);
            QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search")); QUrlQuery query;
            query.addQueryItem(QStringLiteral("name"), queryText); query.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
            query.addQueryItem(QStringLiteral("language"), QStringLiteral("en")); url.setQuery(query);
            const bool ownsNetwork = !m_lookupNetwork;
            auto *network = m_lookupNetwork ? m_lookupNetwork : new QNetworkAccessManager(this);
            network->setTransferTimeout(15000);
            network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
            findLocation->setEnabled(false); findLocation->setText(QStringLiteral("Finding..."));
            QNetworkReply *reply = network->get(QNetworkRequest(url));
            connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
                constexpr qint64 maximumBytes = 1024 * 1024;
                if (received > maximumBytes || total > maximumBytes) reply->abort();
            });
            connect(reply, &QNetworkReply::finished, this, [this, reply, network, ownsNetwork, latitude, longitude, location, findLocation, lookupStatus, revision]() {
                constexpr qint64 maximumBytes = 1024 * 1024;
                const bool failed = reply->error() != QNetworkReply::NoError;
                const bool oversized = reply->bytesAvailable() > maximumBytes;
                const QByteArray payload = reply->read(maximumBytes);
                reply->deleteLater(); if (ownsNetwork) network->deleteLater(); findLocation->setEnabled(true); findLocation->setText(QStringLiteral("Find"));
                const auto showError = [lookupStatus](const QString &message) {
                    showLookupStatus(lookupStatus, message);
                };
                // Revision identity also rejects edits changed away and back.
                if (revision != m_locationRevision) {
                    showError(QStringLiteral("Location changed during the search. Click Find to search again."));
                    return;
                }
                if (failed) {
                    showError(QStringLiteral("Could not look up the location. Check your connection and try again, or enter coordinates manually."));
                    return;
                }
                QJsonParseError error;
                const auto document = QJsonDocument::fromJson(payload, &error);
                const auto resultsValue = document.object().value(QStringLiteral("results"));
                const auto results = resultsValue.toArray();
                if (oversized || error.error != QJsonParseError::NoError || !document.isObject() ||
                    (!resultsValue.isUndefined() && !resultsValue.isArray()) || document.object().value(QStringLiteral("error")).toBool()) {
                    showError(QStringLiteral("The location service returned an invalid response. Your settings were not changed."));
                    return;
                }
                if (results.isEmpty()) {
                    showError(QStringLiteral("No matching location was found. Try another name or enter coordinates manually."));
                    return;
                }
                const auto result = results.first().toObject();
                const auto lat = result.value(QStringLiteral("latitude"));
                const auto lon = result.value(QStringLiteral("longitude"));
                const QString name = result.value(QStringLiteral("name")).toString().trimmed();
                if (name.isEmpty() || !lat.isDouble() || !lon.isDouble() ||
                    !std::isfinite(lat.toDouble()) || !std::isfinite(lon.toDouble()) ||
                    lat.toDouble() < -90 || lat.toDouble() > 90 || lon.toDouble() < -180 || lon.toDouble() > 180) {
                    showError(QStringLiteral("The location service returned an invalid location. Your settings were not changed."));
                    return;
                }
                latitude->setValue(lat.toDouble()); longitude->setValue(lon.toDouble());
                const QString country = result.value(QStringLiteral("country")).toString().trimmed();
                location->setText(name + (country.isEmpty() ? QString() : QStringLiteral(", ") + country));
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
