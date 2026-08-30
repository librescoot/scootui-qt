#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class AddressDatabaseService;

// The address-entry state machine behind AddressSelectionScreen.qml: eight
// phases from city letters to confirm, with auto-advance through single-option
// states and a back path that mirrors the forward path. The screen renders
// this controller's state and maps brake gestures onto scroll()/back()/
// select(); it holds no logic of its own.
//
// Outcomes are signals: dismissed() when the rider backs out, and
// destinationConfirmed() when an address is chosen. Application wires them to
// Navigator / MenuController / NavigationService.
class AddressEntryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString cityPrefix READ cityPrefix NOTIFY stateChanged)
    Q_PROPERTY(QString streetPrefix READ streetPrefix NOTIFY stateChanged)
    Q_PROPERTY(QString housePrefix READ housePrefix NOTIFY stateChanged)
    Q_PROPERTY(QStringList validChars READ validChars NOTIFY stateChanged)
    Q_PROPERTY(int charIndex READ charIndex NOTIFY stateChanged)
    Q_PROPERTY(QVariantList itemList READ itemList NOTIFY stateChanged)
    Q_PROPERTY(int listIndex READ listIndex NOTIFY stateChanged)
    Q_PROPERTY(QString selectedCity READ selectedCity NOTIFY stateChanged)
    Q_PROPERTY(QString selectedStreet READ selectedStreet NOTIFY stateChanged)
    Q_PROPERTY(QString selectedPostcode READ selectedPostcode NOTIFY stateChanged)
    Q_PROPERTY(QString selectedHouse READ selectedHouse NOTIFY stateChanged)
    Q_PROPERTY(bool loadingHouseNumbers READ loadingHouseNumbers NOTIFY stateChanged)
    // How many cities/streets/houses the current prefix still matches, for
    // the breadcrumb. The label word is the view's business.
    Q_PROPERTY(int matchCount READ matchCount NOTIFY stateChanged)

public:
    enum Phase {
        Loading = 0,
        CityLetters,
        CityList,
        StreetLetters,
        StreetList,
        HouseNumbers,
        Confirm,
        HouseDigits,
    };
    Q_ENUM(Phase)

    explicit AddressEntryController(AddressDatabaseService *db, QObject *parent = nullptr);

    int phase() const { return m_phase; }
    QString cityPrefix() const { return m_cityPrefix; }
    QString streetPrefix() const { return m_streetPrefix; }
    QString housePrefix() const { return m_housePrefix; }
    QStringList validChars() const { return m_validChars; }
    int charIndex() const { return m_charIndex; }
    QVariantList itemList() const { return m_itemList; }
    int listIndex() const { return m_listIndex; }
    QString selectedCity() const { return m_selectedCity; }
    QString selectedStreet() const { return m_selectedStreet; }
    QString selectedPostcode() const { return m_selectedPostcode; }
    QString selectedHouse() const { return m_selectedHouse; }
    bool loadingHouseNumbers() const { return m_loadingHouseNumbers; }
    int matchCount() const;

    // Called when the screen comes up: resets to a fresh entry session, then
    // starts city input if the database is ready, or kicks off its build.
    Q_INVOKABLE void activate();

    // The screen's three inputs. scroll() cycles the letter carousel or the
    // list, back() steps one logical step back (leaving the screen from the
    // first one), select() confirms whatever the phase offers. All three
    // ignore input while house numbers are loading; select() additionally
    // cancels a running database build and dismisses when the database is
    // not ready.
    Q_INVOKABLE void scroll();
    Q_INVOKABLE void back();
    Q_INVOKABLE void select();

signals:
    void stateChanged();
    // Rider backed out past the first character (or closed while the
    // database was unavailable).
    void dismissed();
    void destinationConfirmed(double lat, double lng, const QString &label);

private:
    void onDbStatusChanged();
    void onHouseNumbersReady(const QVariantList &houses);

    void enterCityLetters(const QString &prefix);
    void enterCityList(bool autoSelect = true);
    void enterStreetLetters(const QString &prefix);
    void enterStreetList(bool autoSelect = true);
    void enterHouseNumbers();
    void enterHouseDigits(const QString &prefix);
    void enterConfirm();

    void refreshValidChars();
    // When a letter-input state has only one valid next character, pick it
    // automatically so the rider doesn't have to confirm a one-option
    // carousel. Recurses via selectCurrentChar, so a stretch of
    // deterministic characters (e.g. "Berli…n") is typed out in one go.
    void autoPickIfSingle();
    void selectCurrentChar();

    void stepBack();
    void backFromCityLetters();
    void backFromCityList();
    void backFromStreetLetters();
    void backFromStreetList();
    void backFromHouseDigits();
    void backFromHouseOrConfirm();

    void selectCity(int index);
    void selectStreet(int index);
    void selectHouseNumber();
    void confirmAndNavigate();

    QVariantList filteredHouses(const QString &prefix) const;
    QStringList validHouseDigits(const QString &prefix) const;

    AddressDatabaseService *m_db;

    // When a digit-narrowed house set drops to this many, switch from digit
    // entry to a scrollable list. Slightly larger than kMaxListItems because
    // the rider is already mid-narrowing and a small extra scroll beats
    // forcing another digit.
    static constexpr int kMaxListItems = 8;
    static constexpr int kHouseListThreshold = 10;

    int m_phase = Loading;
    QString m_cityPrefix;
    QString m_streetPrefix;
    QString m_housePrefix;
    QStringList m_validChars;
    int m_charIndex = 0;
    QVariantList m_itemList;
    int m_listIndex = 0;
    QVariantList m_allHouses;
    QString m_selectedCity;
    QString m_selectedStreet;
    QString m_selectedPostcode;
    QString m_selectedHouse;
    double m_destLat = 0;
    double m_destLng = 0;
    bool m_loadingHouseNumbers = false;
};
