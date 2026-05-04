#pragma once
#include <QString>
#include <QSettings>

class Lang {
public:
    static Lang& get() {
        static Lang instance;
        return instance;
    }

    QString currentLang;

    Lang() {
        QSettings settings("Simanta", "Preferences");
        currentLang = settings.value("Language", "en").toString();
    }

    void setLanguage(const QString& lang) {
        currentLang = lang;
        QSettings settings("Simanta", "Preferences");
        settings.setValue("Language", lang);
    }

    QString t(const QString& en, const QString& id) {
        return (currentLang == "id") ? id : en;
    }
};
