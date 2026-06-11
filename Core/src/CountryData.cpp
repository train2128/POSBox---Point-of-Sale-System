#include "CountryData.h"

// postalDigits: 0 = alphanumeric/skip strict validation

static const QVector<CountryInfo> s_countries = {
    // Africa
    {"Algeria",        "DZ", "+213", 9,  "DZD", 2, 5},
    {"Angola",         "AO", "+244", 9,  "AOA", 2, 5},
    {"Benin",          "BJ", "+229", 8,  "XOF", 0, 4},
    {"Botswana",       "BW", "+267", 8,  "BWP", 2, 0},
    {"Burkina Faso",   "BF", "+226", 8,  "XOF", 0, 4},
    {"Cameroon",       "CM", "+237", 9,  "XAF", 0, 5},
    {"Chad",           "TD", "+235", 8,  "XAF", 0, 5},
    {"Côte d'Ivoire",  "CI", "+225", 10, "XOF", 0, 5},
    {"Congo (DRC)",    "CD", "+243", 9,  "CDF", 2, 0},
    {"Egypt",          "EG", "+20",  10, "EGP", 2, 5},
    {"Ethiopia",       "ET", "+251", 9,  "ETB", 2, 4},
    {"Ghana",          "GH", "+233", 9,  "GHS", 2, 5},
    {"Guinea",         "GN", "+224", 9,  "GNF", 0, 0},
    {"Kenya",          "KE", "+254", 9,  "KES", 2, 5},
    {"Libya",          "LY", "+218", 9,  "LYD", 3, 5},
    {"Madagascar",     "MG", "+261", 9,  "MGA", 0, 3},
    {"Mali",           "ML", "+223", 8,  "XOF", 0, 4},
    {"Mauritania",     "MR", "+222", 8,  "MRU", 2, 5},
    {"Morocco",        "MA", "+212", 9,  "MAD", 2, 5},
    {"Mozambique",     "MZ", "+258", 9,  "MZN", 2, 4},
    {"Niger",          "NE", "+227", 8,  "XOF", 0, 4},
    {"Nigeria",        "NG", "+234", 10, "NGN", 2, 6},
    {"Senegal",        "SN", "+221", 9,  "XOF", 0, 5},
    {"South Africa",   "ZA", "+27",  9,  "ZAR", 2, 4},
    {"Tanzania",       "TZ", "+255", 9,  "TZS", 2, 5},
    {"Tunisia",        "TN", "+216", 8,  "TND", 3, 4},
    {"Uganda",         "UG", "+256", 9,  "UGX", 0, 5},
    {"Zambia",         "ZM", "+260", 9,  "ZMW", 2, 5},
    {"Zimbabwe",       "ZW", "+263", 9,  "USD", 2, 0},

    // Middle East
    {"Bahrain",        "BH", "+973", 8,  "BHD", 3, 0},
    {"Iraq",           "IQ", "+964", 10, "IQD", 0, 5},
    {"Jordan",         "JO", "+962", 9,  "JOD", 3, 5},
    {"Kuwait",         "KW", "+965", 8,  "KWD", 3, 5},
    {"Lebanon",        "LB", "+961", 7,  "LBP", 0, 0},
    {"Oman",           "OM", "+968", 8,  "OMR", 3, 0},
    {"Palestine",      "PS", "+970", 9,  "ILS", 2, 0},
    {"Qatar",          "QA", "+974", 8,  "QAR", 2, 0},
    {"Saudi Arabia",   "SA", "+966", 9,  "SAR", 2, 5},
    {"Syria",          "SY", "+963", 9,  "SYP", 0, 5},
    {"UAE",            "AE", "+971", 9,  "AED", 2, 0},
    {"Yemen",          "YE", "+967", 9,  "YER", 0, 5},

    // Europe
    {"Albania",        "AL", "+355", 9,  "ALL", 0, 4},
    {"Austria",        "AT", "+43",  10, "EUR", 2, 4},
    {"Belgium",        "BE", "+32",  9,  "EUR", 2, 4},
    {"Bosnia",         "BA", "+387", 8,  "BAM", 2, 5},
    {"Bulgaria",       "BG", "+359", 9,  "BGN", 2, 4},
    {"Croatia",        "HR", "+385", 9,  "EUR", 2, 5},
    {"Czech Republic", "CZ", "+420", 9,  "CZK", 2, 5},
    {"Denmark",        "DK", "+45",  8,  "DKK", 2, 4},
    {"Finland",        "FI", "+358", 10, "EUR", 2, 5},
    {"France",         "FR", "+33",  9,  "EUR", 2, 5},
    {"Germany",        "DE", "+49",  10, "EUR", 2, 5},
    {"Greece",         "GR", "+30",  10, "EUR", 2, 5},
    {"Hungary",        "HU", "+36",  9,  "HUF", 0, 4},
    {"Ireland",        "IE", "+353", 9,  "EUR", 2, 0},
    {"Italy",          "IT", "+39",  10, "EUR", 2, 5},
    {"Netherlands",    "NL", "+31",  9,  "EUR", 2, 0},
    {"Norway",         "NO", "+47",  8,  "NOK", 2, 4},
    {"Poland",         "PL", "+48",  9,  "PLN", 2, 6},
    {"Portugal",       "PT", "+351", 9,  "EUR", 2, 0},
    {"Romania",        "RO", "+40",  9,  "RON", 2, 6},
    {"Russia",         "RU", "+7",   10, "RUB", 2, 6},
    {"Serbia",         "RS", "+381", 9,  "RSD", 0, 5},
    {"Slovakia",       "SK", "+421", 9,  "EUR", 2, 5},
    {"Slovenia",       "SI", "+386", 9,  "EUR", 2, 4},
    {"Spain",          "ES", "+34",  9,  "EUR", 2, 5},
    {"Sweden",         "SE", "+46",  9,  "SEK", 2, 5},
    {"Switzerland",    "CH", "+41",  9,  "CHF", 2, 4},
    {"Turkey",         "TR", "+90",  10, "TRY", 2, 5},
    {"Ukraine",        "UA", "+380", 9,  "UAH", 2, 5},
    {"United Kingdom", "GB", "+44",  10, "GBP", 2, 0},

    // Americas
    {"Argentina",       "AR", "+54",  10, "ARS", 2, 0},
    {"Bolivia",         "BO", "+591", 8,  "BOB", 2, 0},
    {"Brazil",          "BR", "+55",  11, "BRL", 2, 8},
    {"Canada",          "CA", "+1",   10, "CAD", 2, 0},
    {"Chile",           "CL", "+56",  9,  "CLP", 0, 7},
    {"Colombia",        "CO", "+57",  10, "COP", 0, 6},
    {"Costa Rica",      "CR", "+506", 8,  "CRC", 2, 5},
    {"Cuba",            "CU", "+53",  8,  "CUP", 2, 0},
    {"Dominican Rep.",  "DO", "+1",   10, "DOP", 2, 5},
    {"Ecuador",         "EC", "+593", 9,  "USD", 2, 6},
    {"El Salvador",     "SV", "+503", 8,  "USD", 2, 0},
    {"Guatemala",       "GT", "+502", 8,  "GTQ", 2, 5},
    {"Haiti",           "HT", "+509", 8,  "HTG", 2, 0},
    {"Honduras",        "HN", "+504", 8,  "HNL", 2, 0},
    {"Jamaica",         "JM", "+1",   10, "JMD", 2, 0},
    {"Mexico",          "MX", "+52",  10, "MXN", 2, 5},
    {"Nicaragua",       "NI", "+505", 8,  "NIO", 2, 0},
    {"Panama",          "PA", "+507", 8,  "PAB", 2, 0},
    {"Paraguay",        "PY", "+595", 9,  "PYG", 0, 0},
    {"Peru",            "PE", "+51",  9,  "PEN", 2, 5},
    {"Puerto Rico",     "PR", "+1",   10, "USD", 2, 0},
    {"Trinidad",        "TT", "+1",   10, "TTD", 2, 0},
    {"United States",   "US", "+1",   10, "USD", 2, 5},
    {"Uruguay",         "UY", "+598", 8,  "UYU", 2, 5},
    {"Venezuela",       "VE", "+58",  10, "VES", 2, 0},

    // Asia
    {"Afghanistan",     "AF", "+93",  9,  "AFN", 2, 0},
    {"Bangladesh",      "BD", "+880", 10, "BDT", 2, 4},
    {"China",           "CN", "+86",  11, "CNY", 2, 6},
    {"India",           "IN", "+91",  10, "INR", 2, 6},
    {"Indonesia",       "ID", "+62",  12, "IDR", 0, 5},
    {"Iran",            "IR", "+98",  10, "IRR", 0, 0},
    {"Israel",          "IL", "+972", 10, "ILS", 2, 7},
    {"Japan",           "JP", "+81",  10, "JPY", 0, 7},
    {"Kazakhstan",      "KZ", "+7",   10, "KZT", 2, 6},
    {"Kyrgyzstan",      "KG", "+996", 9,  "KGS", 2, 6},
    {"Malaysia",        "MY", "+60",  10, "MYR", 2, 5},
    {"Maldives",        "MV", "+960", 7,  "MVR", 2, 5},
    {"Myanmar",         "MM", "+95",  9,  "MMK", 0, 5},
    {"Nepal",           "NP", "+977", 10, "NPR", 2, 5},
    {"Pakistan",        "PK", "+92",  10, "PKR", 0, 5},
    {"Philippines",     "PH", "+63",  10, "PHP", 2, 4},
    {"Singapore",       "SG", "+65",  8,  "SGD", 2, 6},
    {"South Korea",     "KR", "+82",  10, "KRW", 0, 5},
    {"Sri Lanka",       "LK", "+94",  9,  "LKR", 2, 5},
    {"Taiwan",          "TW", "+886", 9,  "TWD", 0, 3},
    {"Thailand",        "TH", "+66",  9,  "THB", 2, 5},
    {"Uzbekistan",      "UZ", "+998", 9,  "UZS", 0, 6},
    {"Vietnam",         "VN", "+84",  10, "VND", 0, 6},

    // Oceania
    {"Australia",       "AU", "+61",  9,  "AUD", 2, 4},
    {"New Zealand",     "NZ", "+64",  9,  "NZD", 2, 4},
};

const QVector<CountryInfo>& CountryData::countries()
{
    return s_countries;
}

CountryInfo CountryData::findByCode(const QString& code)
{
    for (const auto& c : s_countries) {
        if (c.code.compare(code, Qt::CaseInsensitive) == 0)
            return c;
    }
    return defaultCountry();
}

CountryInfo CountryData::defaultCountry()
{
    return { "Tunisia", "TN", "+216", 8, "TND", 3, 4 };
}
