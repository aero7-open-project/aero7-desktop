#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <cstring>

// All requests terminate here. No socket, proxy or external service is used.
class FixtureReply final : public QNetworkReply
{
public:
    FixtureReply(const QNetworkRequest &request, QObject *parent) : QNetworkReply(parent)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        open(QIODevice::ReadOnly);
    }
    void finish(const QJsonObject &payload, NetworkError error = NoError)
    {
        finishBytes(QJsonDocument(payload).toJson(QJsonDocument::Compact), error);
    }
    void finishBytes(const QByteArray &payload, NetworkError error = NoError)
    {
        m_bytes = payload;
        if (error != NoError) setError(error, QStringLiteral("Controlled request failure"));
        setFinished(true);
        emit readyRead();
        if (error != NoError) emit errorOccurred(error);
        emit finished();
    }
    void abort() override { setError(OperationCanceledError, QStringLiteral("Aborted")); }
    qint64 bytesAvailable() const override { return m_bytes.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const qint64 count = qMin(maximum, m_bytes.size() - m_offset);
        if (!count) return -1;
        std::memcpy(data, m_bytes.constData() + m_offset, count);
        m_offset += count;
        return count;
    }
private:
    QByteArray m_bytes;
    qint64 m_offset = 0;
};

class FixtureNetwork final : public QNetworkAccessManager
{
public:
    QList<FixtureReply *> replies;
protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *) override
    {
        Q_ASSERT(operation == GetOperation);
        auto *reply = new FixtureReply(request, this);
        replies.append(reply);
        return reply;
    }
};
