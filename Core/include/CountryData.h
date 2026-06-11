#ifndef COUNTRYDATA_H
#define COUNTRYDATA_H

#include <QString>
#include <QVector>

struct CountryInfo {
    QString name;
    QString code;
    QString phoneCode;
    int     phoneDigits;
    QString currency;
    int     currencyDecimals;
    int     postalDigits;  // 0 = alphanumeric/skip strict validation
};

namespace CountryData {
    const QVector<CountryInfo>& countries();
    CountryInfo findByCode(const QString& code);
    CountryInfo defaultCountry();
}

#endif // COUNTRYDATA_H
