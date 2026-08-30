#include "AddressEntryController.h"

#include "services/AddressDatabaseService.h"

AddressEntryController::AddressEntryController(AddressDatabaseService *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
    connect(m_db, &AddressDatabaseService::statusChanged,
            this, &AddressEntryController::onDbStatusChanged);
    connect(m_db, &AddressDatabaseService::houseNumbersReady,
            this, &AddressEntryController::onHouseNumbersReady);
}

int AddressEntryController::matchCount() const
{
    if (m_phase == CityLetters)
        return m_db->getCityCount(m_cityPrefix);
    if (m_phase == StreetLetters)
        return m_db->getStreetCount(m_selectedCity, m_streetPrefix);
    if (m_phase == HouseDigits)
        return filteredHouses(m_housePrefix).size();
    return 0;
}

void AddressEntryController::activate()
{
    m_phase = Loading;
    m_cityPrefix.clear();
    m_streetPrefix.clear();
    m_housePrefix.clear();
    m_validChars.clear();
    m_charIndex = 0;
    m_itemList.clear();
    m_listIndex = 0;
    m_allHouses.clear();
    m_selectedCity.clear();
    m_selectedStreet.clear();
    m_selectedPostcode.clear();
    m_selectedHouse.clear();
    m_destLat = 0;
    m_destLng = 0;
    m_loadingHouseNumbers = false;

    const int status = m_db->status();
    if (status == AddressDatabaseService::Ready) {
        enterCityLetters(QString());
    } else {
        if (status == AddressDatabaseService::Idle
            || status == AddressDatabaseService::Error) {
            m_db->initialize();
        }
        emit stateChanged();
    }
}

void AddressEntryController::onDbStatusChanged()
{
    // The build finishing mid-session starts city input; a session already
    // past Loading is left alone.
    if (m_db->status() == AddressDatabaseService::Ready && m_phase == Loading)
        enterCityLetters(QString());
}

// --- Phase transitions ---

void AddressEntryController::enterCityLetters(const QString &prefix)
{
    m_phase = CityLetters;
    m_cityPrefix = prefix;
    m_charIndex = 0;
    refreshValidChars();
    autoPickIfSingle();
    emit stateChanged();
}

void AddressEntryController::enterCityList(bool autoSelect)
{
    m_phase = CityList;
    m_listIndex = 0;
    m_itemList.clear();
    const QStringList cities = m_db->getMatchingCities(m_cityPrefix);
    for (const QString &c : cities)
        m_itemList.append(c);
    if (autoSelect && m_itemList.size() == 1) {
        selectCity(0);
        return;
    }
    emit stateChanged();
}

void AddressEntryController::enterStreetLetters(const QString &prefix)
{
    m_phase = StreetLetters;
    m_streetPrefix = prefix;
    m_charIndex = 0;
    refreshValidChars();
    autoPickIfSingle();
    emit stateChanged();
}

void AddressEntryController::enterStreetList(bool autoSelect)
{
    m_phase = StreetList;
    m_listIndex = 0;
    m_itemList = m_db->getMatchingStreets(m_selectedCity, m_streetPrefix);
    if (autoSelect && m_itemList.size() == 1) {
        selectStreet(0);
        return;
    }
    emit stateChanged();
}

void AddressEntryController::enterHouseNumbers()
{
    m_loadingHouseNumbers = true;
    emit stateChanged();
    m_db->queryHouseNumbers(m_selectedCity, m_selectedStreet, m_selectedPostcode);
}

void AddressEntryController::onHouseNumbersReady(const QVariantList &houses)
{
    if (!m_loadingHouseNumbers)
        return;
    m_loadingHouseNumbers = false;
    m_housePrefix.clear();
    m_allHouses = houses;
    if (houses.size() <= 1) {
        if (houses.size() == 1) {
            const QVariantMap house = houses.first().toMap();
            m_selectedHouse = house.value(QStringLiteral("housenumber")).toString();
            m_destLat = house.value(QStringLiteral("latitude")).toDouble();
            m_destLng = house.value(QStringLiteral("longitude")).toDouble();
        } else {
            m_selectedHouse.clear();
            const QVariantMap coords =
                m_db->getStreetCoordinates(m_selectedCity, m_selectedStreet);
            m_destLat = coords.value(QStringLiteral("latitude")).toDouble();
            m_destLng = coords.value(QStringLiteral("longitude")).toDouble();
        }
        m_phase = Confirm;
        emit stateChanged();
        return;
    }
    if (houses.size() <= kHouseListThreshold) {
        m_phase = HouseNumbers;
        m_listIndex = 0;
        m_itemList = houses;
        emit stateChanged();
        return;
    }
    enterHouseDigits(QString());
}

void AddressEntryController::enterHouseDigits(const QString &prefix)
{
    m_phase = HouseDigits;
    m_housePrefix = prefix;
    m_charIndex = 0;
    refreshValidChars();
    autoPickIfSingle();
    emit stateChanged();
}

void AddressEntryController::enterConfirm()
{
    m_phase = Confirm;
    emit stateChanged();
}

// Houses whose number begins with the given prefix.
QVariantList AddressEntryController::filteredHouses(const QString &prefix) const
{
    QVariantList out;
    for (const QVariant &houseVar : m_allHouses) {
        const QString hn = houseVar.toMap().value(QStringLiteral("housenumber")).toString();
        if (hn.startsWith(prefix))
            out.append(houseVar);
    }
    return out;
}

// Digits that would lead to at least one matching house if appended to the
// current prefix. Alpha suffixes (e.g. "12a") are ignored here — once the
// filtered list is short enough we hand off to the scrollable list.
QStringList AddressEntryController::validHouseDigits(const QString &prefix) const
{
    QStringList out;
    for (const QVariant &houseVar : m_allHouses) {
        const QString hn = houseVar.toMap().value(QStringLiteral("housenumber")).toString();
        if (hn.length() <= prefix.length())
            continue;
        if (!hn.startsWith(prefix))
            continue;
        const QChar next = hn.at(prefix.length());
        if (next < QLatin1Char('0') || next > QLatin1Char('9'))
            continue;
        const QString digit(next);
        if (!out.contains(digit))
            out.append(digit);
    }
    out.sort();
    return out;
}

// --- Char carousel ---

void AddressEntryController::refreshValidChars()
{
    if (m_phase == CityLetters)
        m_validChars = m_db->getValidCityChars(m_cityPrefix);
    else if (m_phase == StreetLetters)
        m_validChars = m_db->getValidStreetChars(m_selectedCity, m_streetPrefix);
    else if (m_phase == HouseDigits)
        m_validChars = validHouseDigits(m_housePrefix);
    m_charIndex = 0;
}

void AddressEntryController::autoPickIfSingle()
{
    if ((m_phase == CityLetters || m_phase == StreetLetters || m_phase == HouseDigits)
        && m_validChars.size() == 1) {
        selectCurrentChar();
    }
}

void AddressEntryController::selectCurrentChar()
{
    if (m_validChars.isEmpty())
        return;

    const QString ch = m_validChars.at(m_charIndex);
    if (m_phase == CityLetters) {
        m_cityPrefix += ch;
        const int cityCount = m_db->getCityCount(m_cityPrefix);
        if (cityCount <= kMaxListItems && cityCount > 0) {
            enterCityList();
        } else if (cityCount == 0) {
            m_cityPrefix.chop(1);
        } else {
            refreshValidChars();
            autoPickIfSingle();
        }
    } else if (m_phase == StreetLetters) {
        m_streetPrefix += ch;
        const int streetCount = m_db->getStreetCount(m_selectedCity, m_streetPrefix);
        if (streetCount <= kMaxListItems && streetCount > 0) {
            enterStreetList();
        } else if (streetCount == 0) {
            m_streetPrefix.chop(1);
        } else {
            refreshValidChars();
            autoPickIfSingle();
        }
    } else if (m_phase == HouseDigits) {
        const QString newPrefix = m_housePrefix + ch;
        const QVariantList filtered = filteredHouses(newPrefix);
        if (filtered.isEmpty())
            return;
        m_housePrefix = newPrefix;
        if (filtered.size() == 1) {
            const QVariantMap house = filtered.first().toMap();
            m_selectedHouse = house.value(QStringLiteral("housenumber")).toString();
            m_destLat = house.value(QStringLiteral("latitude")).toDouble();
            m_destLng = house.value(QStringLiteral("longitude")).toDouble();
            enterConfirm();
        } else if (filtered.size() <= kHouseListThreshold) {
            m_phase = HouseNumbers;
            m_listIndex = 0;
            m_itemList = filtered;
        } else {
            refreshValidChars();
            if (m_validChars.isEmpty()) {
                // Can't narrow further by digit (e.g. only alpha suffixes
                // remain). Drop into the list even though it's longer than
                // the usual threshold.
                m_phase = HouseNumbers;
                m_listIndex = 0;
                m_itemList = filtered;
            } else {
                autoPickIfSingle();
            }
        }
    }
}

// --- Back path ---
// One logical step back from the current phase. When the state we would land
// in is itself a single-option state (a list with one item or a carousel with
// one valid char — i.e. one the forward path would auto-advance past), skip
// through it so the back path mirrors the forward path.

void AddressEntryController::stepBack()
{
    switch (m_phase) {
    case CityLetters:   backFromCityLetters(); break;
    case CityList:      backFromCityList(); break;
    case StreetLetters: backFromStreetLetters(); break;
    case StreetList:    backFromStreetList(); break;
    case HouseDigits:   backFromHouseDigits(); break;
    case HouseNumbers:
    case Confirm:       backFromHouseOrConfirm(); break;
    default: break;
    }
    emit stateChanged();
}

void AddressEntryController::backFromCityLetters()
{
    if (m_cityPrefix.isEmpty()) {
        emit dismissed();
        return;
    }
    m_cityPrefix.chop(1);
    refreshValidChars();
    if (!m_cityPrefix.isEmpty() && m_validChars.size() == 1)
        backFromCityLetters();
}

void AddressEntryController::backFromCityList()
{
    if (m_itemList.size() == 1 && !m_cityPrefix.isEmpty())
        m_cityPrefix.chop(1);
    m_phase = CityLetters;
    refreshValidChars();
    if (!m_cityPrefix.isEmpty() && m_validChars.size() == 1)
        backFromCityLetters();
}

void AddressEntryController::backFromStreetLetters()
{
    if (!m_streetPrefix.isEmpty()) {
        m_streetPrefix.chop(1);
        refreshValidChars();
        if (!m_streetPrefix.isEmpty() && m_validChars.size() == 1)
            backFromStreetLetters();
        return;
    }
    const QStringList cities = m_db->getMatchingCities(m_cityPrefix);
    if (cities.size() == 1 && !m_cityPrefix.isEmpty()) {
        m_cityPrefix.chop(1);
        m_phase = CityLetters;
        refreshValidChars();
        if (!m_cityPrefix.isEmpty() && m_validChars.size() == 1)
            backFromCityLetters();
    } else {
        m_itemList.clear();
        for (const QString &c : cities)
            m_itemList.append(c);
        m_phase = CityList;
        m_listIndex = 0;
    }
}

void AddressEntryController::backFromStreetList()
{
    if (m_itemList.size() == 1 && !m_streetPrefix.isEmpty())
        m_streetPrefix.chop(1);
    m_phase = StreetLetters;
    refreshValidChars();
    if (!m_streetPrefix.isEmpty() && m_validChars.size() == 1)
        backFromStreetLetters();
}

void AddressEntryController::backFromHouseOrConfirm()
{
    // If we got here via digit narrowing, pop one digit and return to the
    // digit carousel rather than jumping all the way back to street.
    if (!m_housePrefix.isEmpty()) {
        m_housePrefix.chop(1);
        m_phase = HouseDigits;
        refreshValidChars();
        return;
    }
    const QVariantList streets = m_db->getMatchingStreets(m_selectedCity, m_streetPrefix);
    if (streets.size() == 1 && !m_streetPrefix.isEmpty()) {
        m_streetPrefix.chop(1);
        m_phase = StreetLetters;
        refreshValidChars();
        if (!m_streetPrefix.isEmpty() && m_validChars.size() == 1)
            backFromStreetLetters();
    } else {
        m_itemList = streets;
        m_phase = StreetList;
        m_listIndex = 0;
    }
}

void AddressEntryController::backFromHouseDigits()
{
    if (!m_housePrefix.isEmpty()) {
        m_housePrefix.chop(1);
        refreshValidChars();
        return;
    }
    const QVariantList streets = m_db->getMatchingStreets(m_selectedCity, m_streetPrefix);
    if (streets.size() == 1 && !m_streetPrefix.isEmpty()) {
        m_streetPrefix.chop(1);
        m_phase = StreetLetters;
        refreshValidChars();
        if (!m_streetPrefix.isEmpty() && m_validChars.size() == 1)
            backFromStreetLetters();
    } else {
        m_itemList = streets;
        m_phase = StreetList;
        m_listIndex = 0;
    }
}

// --- List selection ---

void AddressEntryController::selectCity(int index)
{
    if (index < 0 || index >= m_itemList.size())
        return;
    m_selectedCity = m_itemList.at(index).toString();
    m_streetPrefix.clear();
    enterStreetLetters(QString());
}

void AddressEntryController::selectStreet(int index)
{
    if (index < 0 || index >= m_itemList.size())
        return;
    const QVariantMap entry = m_itemList.at(index).toMap();
    m_selectedStreet = entry.value(QStringLiteral("street")).toString();
    m_selectedPostcode = entry.value(QStringLiteral("postcode")).toString();
    enterHouseNumbers();
}

void AddressEntryController::selectHouseNumber()
{
    if (m_listIndex < 0 || m_listIndex >= m_itemList.size())
        return;
    const QVariantMap entry = m_itemList.at(m_listIndex).toMap();
    m_selectedHouse = entry.value(QStringLiteral("housenumber")).toString();
    m_destLat = entry.value(QStringLiteral("latitude")).toDouble();
    m_destLng = entry.value(QStringLiteral("longitude")).toDouble();
    enterConfirm();
}

void AddressEntryController::confirmAndNavigate()
{
    QString label = m_selectedStreet;
    if (!m_selectedHouse.isEmpty())
        label += QLatin1Char(' ') + m_selectedHouse;
    label += QStringLiteral(", ") + m_selectedCity;
    emit destinationConfirmed(m_destLat, m_destLng, label);
}

// --- Inputs ---

void AddressEntryController::scroll()
{
    if (m_loadingHouseNumbers)
        return;
    if (m_phase == CityLetters || m_phase == StreetLetters || m_phase == HouseDigits) {
        if (m_validChars.isEmpty())
            return;
        m_charIndex = (m_charIndex + 1) % m_validChars.size();
    } else if (m_phase == CityList || m_phase == StreetList || m_phase == HouseNumbers) {
        if (m_itemList.isEmpty())
            return;
        m_listIndex = (m_listIndex + 1) % m_itemList.size();
    } else {
        return;
    }
    emit stateChanged();
}

void AddressEntryController::back()
{
    if (m_loadingHouseNumbers)
        return;
    stepBack();
}

void AddressEntryController::select()
{
    if (m_loadingHouseNumbers)
        return;
    const int status = m_db->status();
    if (status == AddressDatabaseService::Building) {
        m_db->cancelBuild();
        return;
    }
    if (status != AddressDatabaseService::Ready) {
        emit dismissed();
        return;
    }

    switch (m_phase) {
    case CityLetters:
    case StreetLetters:
    case HouseDigits:
        selectCurrentChar();
        break;
    case CityList:
        selectCity(m_listIndex);
        break;
    case StreetList:
        selectStreet(m_listIndex);
        break;
    case HouseNumbers:
        selectHouseNumber();
        break;
    case Confirm:
        confirmAndNavigate();
        return;
    default:
        return;
    }
    emit stateChanged();
}
