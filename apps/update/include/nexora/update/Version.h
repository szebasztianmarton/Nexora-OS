#pragma once

#include <QString>

#include <compare>
#include <optional>

namespace nexora::update {

// A semantic-ish version used to compare an update bundle against the running build:
// three integers, each 1-4 decimal digits ("0.4.1", "12.0.3"). No pre-release or build
// metadata, no leading "v". Distinct from shared::build::Version (Task 1), which is a
// simple build-identity string, not a comparable type.
struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;

    // Strict ^\d{1,4}\.\d{1,4}\.\d{1,4}$; nullopt for anything else (too few/many
    // components, a leading "v", a pre-release/build suffix, non-digits, empty). Leading
    // zeros in a component are accepted ("01.02.03" == "1.2.3").
    static std::optional<Version> parse(const QString &text);

    auto operator<=>(const Version &) const = default;

    QString toString() const;
};

} // namespace nexora::update
