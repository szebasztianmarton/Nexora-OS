#pragma once

#include "nexora/update/Version.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>

namespace nexora::update {

// The pinned root filesystem image for one slot.
struct ImageInfo
{
    QString file; // plain file name, relative to the bundle directory
    qint64 size = 0; // bytes; 1..64 GiB
    QByteArray sha256Hex; // 64 lowercase hex characters
};

// A signed update bundle's manifest.json; see docs/update-system.md for the bundle layout
// and the trust model the signature over this document is part of.
struct UpdateManifest
{
    int format = 1;
    QString product;
    QString board;
    QString buildId;
    QString notes;
    Version version;
    Version minimumVersion;
    QDateTime releasedAt;
    ImageInfo image;

    // Strict: this is the thing a signature is pinned to, so anything unrecognised is
    // rejected rather than ignored. Rejects: input over 64 KiB; a non-object root; an
    // unknown top-level key; "format" != 1; a non-string/empty product, board, buildId,
    // version, minimumVersion or releasedAt; an unparsable version/minimumVersion; an
    // unparsable releasedAt; a missing or malformed "image" (unknown key; file containing
    // '/' or "..", empty, over 128 characters, or outside ^[A-Za-z0-9._-]+$; size outside
    // 1..64 GiB; sha256 not exactly 64 lowercase hex characters); "notes" over 4000
    // characters. On rejection |error| (if given) holds a short reason.
    static std::optional<UpdateManifest> parse(const QByteArray &json, QString *error);
};

} // namespace nexora::update
