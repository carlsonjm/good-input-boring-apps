#include "Signature.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <cmath>

namespace pdf {

bool SignatureInk::isEmpty() const
{
    for (const auto &stroke : strokes)
        if (stroke.size() > 1)
            return false;
    return true;
}

SignatureInk SignatureInk::simplified() const
{
    constexpr double minStep = 0.002;
    SignatureInk out;
    out.aspect = aspect;
    for (const auto &stroke : strokes) {
        QList<QPointF> kept;
        for (const QPointF &pt : stroke) {
            const QPointF clamped(std::clamp(pt.x(), 0.0, 1.0), std::clamp(pt.y(), 0.0, 1.0));
            if (kept.isEmpty() || std::hypot(clamped.x() - kept.last().x(),
                                             (clamped.y() - kept.last().y()) / aspect) >= minStep)
                kept.append(clamped);
        }
        if (kept.size() > 1)
            out.strokes.append(kept);
    }
    return out;
}

SignatureStore::SignatureStore(const QString &directory)
    : m_directory(directory.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      : directory)
{
}

QString SignatureStore::filePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("signature.json"));
}

bool SignatureStore::has() const { return !load().isEmpty(); }

SignatureInk SignatureStore::load() const
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    SignatureInk ink;
    const double aspect = root.value(QStringLiteral("aspect")).toDouble();
    if (aspect > 0.1 && aspect < 20)
        ink.aspect = aspect;
    for (const QJsonValue &strokeValue : root.value(QStringLiteral("strokes")).toArray()) {
        QList<QPointF> stroke;
        for (const QJsonValue &pointValue : strokeValue.toArray()) {
            const QJsonArray xy = pointValue.toArray();
            if (xy.size() == 2)
                stroke.append(QPointF(xy.at(0).toDouble(), xy.at(1).toDouble()));
        }
        ink.strokes.append(stroke);
    }
    return ink.simplified();
}

bool SignatureStore::save(const SignatureInk &ink) const
{
    const SignatureInk clean = ink.simplified();
    if (clean.isEmpty())
        return false;
    if (!QDir().mkpath(m_directory))
        return false;
    QJsonArray strokes;
    for (const auto &stroke : clean.strokes) {
        QJsonArray points;
        for (const QPointF &pt : stroke)
            points.append(QJsonArray{std::round(pt.x() * 10000) / 10000,
                                     std::round(pt.y() * 10000) / 10000});
        strokes.append(points);
    }
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("aspect"), clean.aspect},
                                         {QStringLiteral("strokes"), strokes}})
                   .toJson(QJsonDocument::Compact));
    return file.commit();
}

bool SignatureStore::forget() const
{
    return !QFile::exists(filePath()) || QFile::remove(filePath());
}

} // namespace pdf
