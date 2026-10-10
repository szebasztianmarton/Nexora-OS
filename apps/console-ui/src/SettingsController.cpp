#include "SettingsController.h"

#include "input/ButtonMapping.h"
#include "input/InputController.h"

#include "nexora/shared/Logging.h"
#include "nexora/shared/Settings.h"
#include "nexora/system/SystemIntegration.h"

namespace nexora::ui {

using shared::Settings;

namespace {

const QStringList kResolutions{QStringLiteral("auto"), QStringLiteral("1280x720"), QStringLiteral("1920x1080"),
                               QStringLiteral("2560x1440"), QStringLiteral("3840x2160")};

const QHash<QString, QStringList> kChoices{
    {Settings::DisplayMode, {QStringLiteral("fullscreen"), QStringLiteral("windowed")}},
    {Settings::DisplayResolution, kResolutions},
    {Settings::LibrarySortOrder,
     {QStringLiteral("title"), QStringLiteral("recent"), QStringLiteral("emulator"), QStringLiteral("added")}},
    {Settings::InputBackend,
     {QStringLiteral("auto"), QStringLiteral("evdev"), QStringLiteral("sdl3"), QStringLiteral("none")}},
    {Settings::UiLanguage, {QStringLiteral("en")}},
};

QString actionLabel(const QString &action)
{
    static const QHash<QString, QString> labels{
        {QStringLiteral("accept"), QStringLiteral("Confirm")},     {QStringLiteral("back"), QStringLiteral("Back")},
        {QStringLiteral("menu"), QStringLiteral("Menu")},          {QStringLiteral("favorite"), QStringLiteral("Favorite")},
        {QStringLiteral("search"), QStringLiteral("Search")},      {QStringLiteral("prevTab"), QStringLiteral("Previous tab")},
        {QStringLiteral("nextTab"), QStringLiteral("Next tab")},
    };
    return labels.value(action, action);
}

} // namespace

SettingsController::SettingsController(shared::Settings *settings, input::InputController *input,
                                       system::SystemIntegration *system, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_input(input)
    , m_system(system)
{
    m_captureTimeout.setSingleShot(true);
    m_captureTimeout.setInterval(5000);
    connect(&m_captureTimeout, &QTimer::timeout, this, &SettingsController::cancelRemap);
    if (m_input)
        connect(m_input, &input::InputController::buttonCaptured, this, &SettingsController::onCaptured);
    connect(m_settings, &Settings::valueChanged, this, [this](const QString &key, const QVariant &value) {
        if (key == Settings::InputMapping)
            applyMapping();
        else if (key == Settings::AudioVolume && volumeSupported())
            m_system->applyVolume(value.toInt(), nullptr);
        else if (key == Settings::DisplayMode)
            emit displayModeChanged(value.toString());
        emit changed();
    });
    connect(m_settings, &Settings::reloaded, this, &SettingsController::changed);
}

QString SettingsController::language() const { return m_settings->stringValue(Settings::UiLanguage); }
QString SettingsController::displayMode() const { return m_settings->stringValue(Settings::DisplayMode); }
QString SettingsController::resolution() const { return m_settings->stringValue(Settings::DisplayResolution); }
QStringList SettingsController::resolutions() const { return kResolutions; }
int SettingsController::volume() const { return m_settings->intValue(Settings::AudioVolume); }
bool SettingsController::showHints() const { return m_settings->boolValue(Settings::UiShowHints); }
QString SettingsController::sortOrder() const { return m_settings->stringValue(Settings::LibrarySortOrder); }
QString SettingsController::inputBackend() const { return m_settings->stringValue(Settings::InputBackend); }
QString SettingsController::userFile() const { return m_settings->paths().userFile; }

bool SettingsController::volumeSupported() const
{
    return m_system && m_system->supports(system::Capability::Volume);
}

QStringList SettingsController::issues() const
{
    QStringList list = m_settings->issues();
    QStringList mappingErrors;
    if (!input::ButtonMapping::fromVariantMap(m_settings->mapValue(Settings::InputMapping), &mappingErrors))
        list.append(QStringLiteral("Controller mapping ignored (%1); using defaults").arg(mappingErrors.join(QStringLiteral("; "))));
    return list;
}

QVariantList SettingsController::mappingRows() const
{
    const QVariantMap map = m_settings->mapValue(Settings::InputMapping);
    QVariantList rows;
    for (const QString &action : input::ButtonMapping::actionNames()) {
        rows.append(QVariantMap{{QStringLiteral("action"), action},
                                {QStringLiteral("label"), actionLabel(action)},
                                {QStringLiteral("button"), map.value(action).toString()}});
    }
    return rows;
}

bool SettingsController::set(const QString &key, const QVariant &value)
{
    QString error;
    if (!m_settings->setValue(key, value, &error)) {
        m_lastError = error;
        qCWarning(lcConfig).noquote() << "Rejected setting:" << error;
        emit changed();
        return false;
    }
    m_lastError.clear();
    if (!m_settings->save()) {
        m_lastError = QStringLiteral("Could not save %1").arg(m_settings->paths().userFile);
        emit changed();
        return false;
    }
    return true;
}

void SettingsController::cycle(const QString &key, int step)
{
    const QStringList choices = kChoices.value(key);
    if (choices.isEmpty())
        return;
    const qsizetype current = qMax<qsizetype>(0, choices.indexOf(m_settings->stringValue(key)));
    const qsizetype next = ((current + step) % choices.size() + choices.size()) % choices.size();
    set(key, choices.at(next));
}

bool SettingsController::beginRemap(const QString &action)
{
    if (!input::ButtonMapping::actionFromName(action) || !m_input)
        return false;
    m_capturing = action;
    m_input->setCapturing(true);
    m_captureTimeout.start();
    emit capturingChanged();
    return true;
}

void SettingsController::cancelRemap()
{
    if (m_capturing.isEmpty())
        return;
    m_capturing.clear();
    m_captureTimeout.stop();
    if (m_input)
        m_input->setCapturing(false);
    emit capturingChanged();
}

void SettingsController::onCaptured(const QString &button)
{
    if (m_capturing.isEmpty())
        return;
    const QString action = m_capturing;
    cancelRemap();
    remap(action, button);
}

bool SettingsController::remap(const QString &action, const QString &button)
{
    const auto a = input::ButtonMapping::actionFromName(action);
    const auto b = input::ButtonMapping::buttonFromName(button);
    if (!a || !b) {
        m_lastError = QStringLiteral("Unknown action or button");
        emit changed();
        return false;
    }
    auto mapping = input::ButtonMapping::fromVariantMap(m_settings->mapValue(Settings::InputMapping), nullptr)
                       .value_or(input::ButtonMapping::defaults());
    mapping.bind(*a, *b);
    return set(Settings::InputMapping, mapping.toVariantMap());
}

bool SettingsController::resetToDefaults()
{
    cancelRemap();
    const bool ok = m_settings->resetToDefaults();
    applyAll();
    emit changed();
    return ok;
}

void SettingsController::applyMapping()
{
    if (!m_input)
        return;
    QStringList errors;
    const auto mapping = input::ButtonMapping::fromVariantMap(m_settings->mapValue(Settings::InputMapping), &errors);
    if (!mapping)
        qCWarning(lcInput).noquote() << "Invalid controller mapping, using defaults:" << errors.join(QStringLiteral("; "));
    m_input->setMapping(mapping.value_or(input::ButtonMapping::defaults()));
}

void SettingsController::applyAll()
{
    applyMapping();
    if (volumeSupported())
        m_system->applyVolume(volume(), nullptr);
}

} // namespace nexora::ui
