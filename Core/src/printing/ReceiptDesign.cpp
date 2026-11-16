#include "ReceiptDesign.h"
#include "Prefixes.h"
#include "LocalApi.h"
#include "LocalImages.h"
#include <QCoreApplication>
#include <QDate>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QTime>

namespace {

using namespace Receipt;

enum class Items { OneLine, TwoLines, Table, NameThenPrice };

// What sets each model apart beyond which parts it shows.
struct ModelStyle {
    bool compactDetails;    // ticket, date and time share a line
    Items items;
    bool titleBand;
    bool bigOrder;
    bool alignLeft;
    bool bigShopName;
};

ModelStyle styleOf(int model)
{
    switch (model) {
    case 1: return {true, Items::OneLine, false, false, false, false};
    case 3: return {false, Items::Table, false, false, false, true};
    case 4: return {true, Items::OneLine, false, false, false, false};
    case 5: return {true, Items::NameThenPrice, false, false, true, false};
    case 6: return {true, Items::TwoLines, true, false, false, true};
    case 7: return {true, Items::OneLine, false, true, false, true};
    case 8: return {false, Items::OneLine, false, false, false, true};
    case 9: return {true, Items::OneLine, false, false, false, false};
    default: return {false, Items::TwoLines, false, false, false, true};
    }
}

// Receipt wording follows the app language (Settings > App Settings).
QString say(const char* text)
{
    return QCoreApplication::translate("Receipt", text);
}

QString money(double value)
{
    return QString::number(value, 'f', AppConstants::PRICE_DECIMALS);
}

// Builds the lines, one small step at a time.
class Builder
{
public:
    Builder(const Design& design, int columns, int smallColumns)
        : d(design)
        , small(design.spacing == QLatin1String("Smallest"))
        , width(small ? smallColumns : columns)
        , defaultAlign(styleOf(design.model).alignLeft ? 0 : 1)
    {
    }

    bool shown(const QString& part) const { return !d.hidden.contains(part); }

    Line base(const QString& text, int align, const QString& part) const
    {
        Line line;
        line.text = text;
        line.align = align;
        line.part = part;
        line.small = small;
        return line;
    }

    void text(const QString& value, const QString& part = QString(), int align = -1, bool bold = false)
    {
        Line line = base(value, align < 0 ? defaultAlign : align, part);
        line.bold = bold;
        lines << line;
    }

    // Long words are cut so nothing runs off the paper.
    QStringList broken(const QString& value, int room) const
    {
        QStringList result;
        QString current;
        for (const QString& word : value.split(' ', Qt::SkipEmptyParts)) {
            QString rest = word;
            if (!current.isEmpty() && current.size() + 1 + rest.size() > room) {
                result << current;
                current.clear();
            }
            while (rest.size() > room) {
                result << rest.left(room);
                rest = rest.mid(room);
            }
            current += (current.isEmpty() ? QString() : QStringLiteral(" ")) + rest;
        }
        if (!current.isEmpty())
            result << current;
        return result;
    }

    void wrapped(const QString& value, const QString& part, int align = -1, bool bold = false)
    {
        for (const QString& piece : broken(value, width))
            text(piece, part, align, bold);
    }

    void big(const QString& value, const QString& part)
    {
        for (const QString& piece : broken(value, width)) {
            Line line = base(piece, defaultAlign, part);
            line.bold = true;
            line.doubleHeight = !small;
            lines << line;
        }
    }

    void row(const QString& left, const QString& right, const QString& part = QString(), bool bold = false)
    {
        if (left.size() + 1 + right.size() > width) {
            wrapped(left, part, 0, bold);
            wrapped(right, part, 2, bold);
            return;
        }
        Line line = base(left.leftJustified(width - right.size()) + right, 0, part);
        line.bold = bold;
        lines << line;
    }

    // Short parts on as few lines as fit, each still its own part.
    void shared(const QList<QPair<QString, QString>>& parts, const QString& separator)
    {
        Line line = base(QString(), defaultAlign, QString());
        for (const auto& piece : parts) {
            if (piece.second.size() > width) {  // too long to share: its own lines
                if (!line.text.isEmpty())
                    lines << line;
                line = base(QString(), defaultAlign, QString());
                wrapped(piece.second, piece.first);
                continue;
            }
            const QString addition = (line.text.isEmpty() ? QString() : separator) + piece.second;
            if (!line.text.isEmpty() && line.text.size() + addition.size() > width) {
                lines << line;
                line = base(QString(), defaultAlign, QString());
            }
            if (!line.text.isEmpty())
                line.text += separator;
            line.spans << Span{int(line.text.size()), int(piece.second.size()), piece.first};
            line.text += piece.second;
        }
        if (!line.text.isEmpty())
            lines << line;
    }

    void gap()
    {
        if (d.spacing == QLatin1String("Smallest") || d.spacing == QLatin1String("Compact"))
            return;
        text(QString(), QString(), 0);
        if (d.spacing == QLatin1String("Airy"))
            text(QString(), QString(), 0);
    }

    void rule()
    {
        if (d.lines == QLatin1String("None")) {
            gap();
            return;
        }
        const QChar character = d.lines == QLatin1String("Double") ? QChar('=')
                                : d.lines == QLatin1String("Dots") ? QChar('.') : QChar('-');
        text(QString(width, character), QString(), 0);
    }

    void band(const QString& value, const QString& part = QString())
    {
        const QString clipped = value.left(width);
        const int pad = (width - int(clipped.size())) / 2;
        Line line = base((QString(pad, ' ') + clipped).leftJustified(width), 0, part);
        line.reversed = true;
        line.bold = true;
        lines << line;
    }

    const Design& d;
    const bool small;
    const int width;
    const int defaultAlign;
    QList<Line> lines;
};

void addItems(Builder& b, const QList<TicketItem>& items, Items form)
{
    const bool unitPrices = b.shown("unitPrice");
    const int width = b.width;

    if (form == Items::Table) {
        const bool wide = width >= 42;
        const int amountWidth = wide ? 10 : 9;
        const int unitWidth = (wide && unitPrices) ? 9 : 0;
        const int qtyWidth = 5;
        const int nameWidth = width - amountWidth - unitWidth - qtyWidth;
        const QString unitHead = unitWidth ? QStringLiteral("P.U.").rightJustified(unitWidth) : QString();
        Line head = b.base(QCoreApplication::translate("Receipt", "Item").leftJustified(nameWidth)
                               + QCoreApplication::translate("Receipt", "Qty").rightJustified(qtyWidth) + unitHead
                               + QCoreApplication::translate("Receipt", "Amount").rightJustified(amountWidth),
                           0, QString());
        head.bold = true;
        b.lines << head;
        b.text(QString(width, '-'), QString(), 0);
        for (const TicketItem& item : items) {
            const QStringList pieces = b.broken(item.name, nameWidth);
            b.text(pieces.value(0).leftJustified(nameWidth)
                       + QString::number(item.quantity).rightJustified(qtyWidth)
                       + (unitWidth ? money(item.unitPrice).rightJustified(unitWidth) : QString())
                       + money(item.quantity * item.unitPrice).rightJustified(amountWidth),
                   QString(), 0);
            for (int i = 1; i < pieces.size(); ++i)
                b.text(pieces[i], QString(), 0);
        }
        return;
    }

    for (const TicketItem& item : items) {
        const QString amount = money(item.quantity * item.unitPrice);
        if (form == Items::NameThenPrice) {
            b.row(item.name, amount);
            if (unitPrices && item.quantity > 1)
                b.text(QString("%1 x %2").arg(item.quantity).arg(money(item.unitPrice)), "unitPrice", 0);
        } else if (form == Items::OneLine && !unitPrices) {
            b.row(QString("%1 %2").arg(item.quantity).arg(item.name), amount);
        } else {
            b.wrapped(item.name, QString(), 0);
            if (unitPrices)
                b.row(QString("  %1 x %2").arg(item.quantity).arg(money(item.unitPrice)), amount, "unitPrice");
            else
                b.row(QString("  x%1").arg(item.quantity), amount);
        }
    }
}

void addTotal(Builder& b, double total)
{
    const Design& d = b.d;
    const QString word = QCoreApplication::translate("Receipt", "TOTAL");
    const QString amount = money(total);
    const QString currency = QString::fromLatin1(AppConstants::CURRENCY);

    if (d.totalSize == QLatin1String("Band")) {
        b.band(word + "  " + amount + " " + currency);
    } else if (d.totalSize == QLatin1String("Huge") && !b.small) {
        b.gap();
        b.text(word, QString(), 1, true);
        Line big = b.base(amount, 1, QString());
        big.bold = true;
        big.doubleHeight = true;
        big.doubleWidth = true;
        b.lines << big;
        b.text(currency, QString(), 1);
    } else if (d.totalSize == QLatin1String("Large") && !b.small) {
        b.row(word, amount + " " + currency, QString(), true);
        b.lines.last().doubleHeight = true;
    } else {
        b.row(word, amount + " " + currency, QString(), true);
    }
}

} // namespace

namespace Receipt {

QString modelName(int model)
{
    static const char* const names[] = {QT_TRANSLATE_NOOP("Receipt", "Simple"), QT_TRANSLATE_NOOP("Receipt", "Standard"), QT_TRANSLATE_NOOP("Receipt", "Detailed"), QT_TRANSLATE_NOOP("Receipt", "Big Total"), QT_TRANSLATE_NOOP("Receipt", "Modern"),
                                        QT_TRANSLATE_NOOP("Receipt", "Bold Bands"), QT_TRANSLATE_NOOP("Receipt", "Café"), QT_TRANSLATE_NOOP("Receipt", "Workshop"), QT_TRANSLATE_NOOP("Receipt", "Paper Saver")};
    return say(names[qBound(1, model, ModelCount) - 1]);
}

void applyModel(Design& design, int model)
{
    design.model = qBound(1, model, ModelCount);
    design.logoSize = "Medium";
    design.totalSize = "Normal";
    design.lines = "Dashes";
    design.spacing = "Normal";
    design.hidden = {"address", "email", "website", "social", "customer",
                     "description", "advance", "leftToPay", "signature"};

    switch (design.model) {
    case 1:
        design.hidden += QSet<QString>{"logo", "phone", "cashier", "unitPrice", "subtotal"};
        break;
    case 3:
        design.lines = "Double";
        design.hidden -= QSet<QString>{"address", "email", "website"};
        break;
    case 4:
        design.totalSize = "Huge";
        design.hidden += QSet<QString>{"logo", "phone", "cashier", "unitPrice", "subtotal"};
        break;
    case 5:
        design.lines = "None";
        design.spacing = "Airy";
        design.hidden += QSet<QString>{"logo", "phone", "cashier", "subtotal"};
        break;
    case 6:
        design.lines = "None";
        design.logoSize = "Small";
        design.totalSize = "Band";
        design.hidden += QSet<QString>{"phone"};
        break;
    case 7:
        design.lines = "Dots";
        design.hidden -= QSet<QString>{"social"};
        design.hidden += QSet<QString>{"phone", "cashier", "unitPrice", "subtotal"};
        break;
    case 8:
        design.hidden -= QSet<QString>{"customer", "description", "advance", "leftToPay", "signature"};
        design.hidden += QSet<QString>{"logo", "time", "unitPrice", "subtotal", "bottomMessage"};
        break;
    case 9:
        design.lines = "None";
        design.spacing = "Smallest";
        design.hidden += QSet<QString>{"logo", "phone", "unitPrice", "subtotal"};
        break;
    default:
        break;
    }
}

QString toJson(const Design& design)
{
    QStringList parts(design.hidden.cbegin(), design.hidden.cend());
    parts.sort();
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {"model", design.model},
        {"hidden", QJsonArray::fromStringList(parts)},
        {"top", design.topMessage},
        {"bottom", design.bottomMessage},
        {"logo_size", design.logoSize},
        {"total_size", design.totalSize},
        {"lines", design.lines},
        {"spacing", design.spacing},
    }).toJson(QJsonDocument::Compact));
}

Design fromJson(const QString& json)
{
    Design design;
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    applyModel(design, o.value("model").toInt(2));
    if (o.isEmpty())
        return design;
    if (o.contains("hidden")) {
        design.hidden.clear();
        const QJsonArray hidden = o.value("hidden").toArray();
        for (const QJsonValue& value : hidden)
            design.hidden.insert(value.toString());
    }
    design.topMessage = o.value("top").toString();
    design.bottomMessage = o.value("bottom").toString();
    design.logoSize = o.value("logo_size").toString(design.logoSize);
    design.totalSize = o.value("total_size").toString(design.totalSize);
    design.lines = o.value("lines").toString(design.lines);
    design.spacing = o.value("spacing").toString(design.spacing);
    return design;
}

QString partName(const QString& part)
{
    static const QHash<QString, const char*> names{
        {"logo", QT_TRANSLATE_NOOP("Receipt", "Logo")}, {"topMessage", QT_TRANSLATE_NOOP("Receipt", "Top message")}, {"shopName", QT_TRANSLATE_NOOP("Receipt", "Shop name")},
        {"address", QT_TRANSLATE_NOOP("Receipt", "Address")},
        {"phone", QT_TRANSLATE_NOOP("Receipt", "Phone")}, {"email", QT_TRANSLATE_NOOP("Receipt", "Email")}, {"ticketNumber", QT_TRANSLATE_NOOP("Receipt", "Ticket number")},
        {"date", QT_TRANSLATE_NOOP("Receipt", "Date")}, {"time", QT_TRANSLATE_NOOP("Receipt", "Time")}, {"cashier", QT_TRANSLATE_NOOP("Receipt", "Cashier")}, {"customer", QT_TRANSLATE_NOOP("Receipt", "Customer")},
        {"description", QT_TRANSLATE_NOOP("Receipt", "Work done")}, {"unitPrice", QT_TRANSLATE_NOOP("Receipt", "Unit prices")}, {"subtotal", QT_TRANSLATE_NOOP("Receipt", "Subtotal")},
        {"discount", QT_TRANSLATE_NOOP("Receipt", "Discount")}, {"advance", QT_TRANSLATE_NOOP("Receipt", "Advance")}, {"leftToPay", QT_TRANSLATE_NOOP("Receipt", "Left to pay")},
        {"website", QT_TRANSLATE_NOOP("Receipt", "Website")}, {"social", QT_TRANSLATE_NOOP("Receipt", "Social media")},
        {"bottomMessage", QT_TRANSLATE_NOOP("Receipt", "Bottom message")}, {"signature", QT_TRANSLATE_NOOP("Receipt", "Signature")}};
    const auto it = names.constFind(part);
    return it == names.constEnd() ? part : say(it.value());
}

int dotsFor(const QString& paperSize)
{
    // Printable width at 203 dpi.
    if (paperSize == QLatin1String("58mm"))   return 384;
    if (paperSize == QLatin1String("76mm"))   return 512;
    if (paperSize == QLatin1String("82.5mm")) return 608;
    if (paperSize == QLatin1String("104mm"))  return 816;
    if (paperSize == QLatin1String("112mm"))  return 880;
    return 576;
}

int columnsFor(const QString& paperSize)
{
    if (paperSize == QLatin1String("58mm"))   return 32;
    if (paperSize == QLatin1String("76mm"))   return 42;
    if (paperSize == QLatin1String("82.5mm")) return 50;
    if (paperSize == QLatin1String("104mm"))  return 64;
    if (paperSize == QLatin1String("112mm"))  return 68;
    return 48;
}

int smallColumnsFor(const QString& paperSize)
{
    return dotsFor(paperSize) / 9;  // the smaller font is 9 dots wide
}

QList<Line> layout(const TicketData& data, const QString& paperSize)
{
    const Design& design = data.design;
    const ModelStyle style = styleOf(design.model);
    Builder b(design, columnsFor(paperSize), smallColumnsFor(paperSize));
    const ShopInfo& shop = data.shop;

    // ── Shop ────────────────────────────────────────────────────────────────
    if (b.shown("logo") && !shop.logoImage.isNull()) {
        const qreal share = design.logoSize == QLatin1String("Small") ? 0.24
                            : design.logoSize == QLatin1String("Large") ? 0.5 : 0.34;
        Line logo;
        logo.isLogo = true;
        logo.part = "logo";
        logo.logoDots = int(dotsFor(paperSize) * share);
        logo.align = b.defaultAlign;
        b.lines << logo;
    }

    QString top = design.topMessage;
    if (top.isEmpty() && design.model == 7)
        top = QCoreApplication::translate("Receipt", "Welcome!");
    if (b.shown("topMessage") && !top.isEmpty())
        b.wrapped(top, "topMessage");

    if (b.shown("shopName")) {
        const QString name = shop.name.isEmpty() ? QCoreApplication::translate("Receipt", "Shop Name") : shop.name;
        if (style.bigShopName)
            b.big(name, "shopName");
        else
            b.wrapped(name, "shopName", -1, true);
    }
    if (b.shown("address") && !shop.address.isEmpty())
        b.wrapped(shop.address, "address");
    if (b.shown("phone") && !shop.phone.isEmpty())
        b.wrapped(shop.phone, "phone");
    if (b.shown("email") && !shop.email.isEmpty())
        b.wrapped(shop.email, "email");

    if (data.isUnpaid) {
        b.gap();
        b.band(QCoreApplication::translate("Receipt", "NOT PAID"));
    }

    // ── Ticket details ──────────────────────────────────────────────────────
    const QString number = data.ticketNumber.isEmpty() ? QString() : "#" + data.ticketNumber;
    bool numberDone = !b.shown("ticketNumber") || number.isEmpty();
    if (style.titleBand) {
        b.gap();
        if (!numberDone)
            b.band(QCoreApplication::translate("Receipt", "RECEIPT ") + number, "ticketNumber");
        numberDone = true;
    } else if (style.bigOrder) {
        b.rule();
        if (!numberDone)
            b.big(QCoreApplication::translate("Receipt", "Order ") + number, "ticketNumber");
        numberDone = true;
    } else if (!style.compactDetails) {
        b.gap();
    }

    if (style.compactDetails) {
        QList<QPair<QString, QString>> parts;
        const QString numberText = numberDone ? QString() : number;
        // Each compact model keeps the order it was designed with.
        if (design.model != 1 && !numberText.isEmpty())
            parts << qMakePair(QStringLiteral("ticketNumber"), numberText);
        if (b.shown("date") && !data.date.isEmpty())
            parts << qMakePair(QStringLiteral("date"), data.date);
        if (b.shown("time") && !data.time.isEmpty())
            parts << qMakePair(QStringLiteral("time"), data.time);
        if (design.model == 1 && !numberText.isEmpty())
            parts << qMakePair(QStringLiteral("ticketNumber"), numberText);
        if (b.shown("cashier") && !data.cashier.isEmpty())
            parts << qMakePair(QStringLiteral("cashier"), QCoreApplication::translate("Receipt", "Cashier ") + data.cashier);
        if (b.shown("customer") && !data.customer.isEmpty())
            parts << qMakePair(QStringLiteral("customer"), QCoreApplication::translate("Receipt", "Customer ") + data.customer);
        b.shared(parts, design.model == 9 ? QStringLiteral(" ") : QStringLiteral("  "));
    } else {
        if (!numberDone)
            b.row(QCoreApplication::translate("Receipt", "Ticket"), number, "ticketNumber");
        if (b.shown("date") && !data.date.isEmpty())
            b.row(QCoreApplication::translate("Receipt", "Date"), data.date, "date");
        if (b.shown("time") && !data.time.isEmpty())
            b.row(QCoreApplication::translate("Receipt", "Time"), data.time, "time");
        if (b.shown("cashier") && !data.cashier.isEmpty())
            b.row(QCoreApplication::translate("Receipt", "Cashier"), data.cashier, "cashier");
        if (b.shown("customer") && !data.customer.isEmpty())
            b.row(QCoreApplication::translate("Receipt", "Customer"), data.customer, "customer");
    }
    b.rule();

    if (b.shown("description") && !data.description.isEmpty()) {
        b.text(QCoreApplication::translate("Receipt", "Work"), "description", 0, true);
        b.wrapped(data.description, "description", 0);
        b.rule();
    }

    // ── Items and totals ────────────────────────────────────────────────────
    addItems(b, data.items, style.items);
    b.rule();

    if (b.shown("subtotal"))
        b.row(QCoreApplication::translate("Receipt", "Subtotal"), money(data.subtotal), "subtotal");
    if (b.shown("discount") && data.discountAmount > 0)
        b.row(QCoreApplication::translate("Receipt", "Discount"), "-" + money(data.discountAmount), "discount");
    addTotal(b, data.total);

    if (data.advance > 0) {
        const QString currency = QString::fromLatin1(AppConstants::CURRENCY);
        if (b.shown("advance"))
            b.row(QCoreApplication::translate("Receipt", "Advance"), money(data.advance), "advance");
        if (b.shown("leftToPay"))
            b.row(QCoreApplication::translate("Receipt", "Left to pay"),
                  money(qMax(0.0, data.total - data.advance)) + " " + currency, "leftToPay", true);
    }

    // ── End ─────────────────────────────────────────────────────────────────
    b.gap();
    if (b.shown("website") && !shop.website.isEmpty())
        b.wrapped(shop.website, "website");
    if (b.shown("social") && !shop.social.isEmpty()) {
        b.text(QCoreApplication::translate("Receipt", "Follow us"), "social", -1, true);
        b.wrapped(shop.social, "social");
        b.gap();
    }
    if (b.shown("bottomMessage")) {
        const QString bottom = design.bottomMessage.isEmpty()
                                   ? QCoreApplication::translate("Receipt", "Thank you, see you soon!")
                                   : design.bottomMessage;
        b.wrapped(bottom, "bottomMessage");
    }
    if (b.shown("signature")) {
        b.wrapped(QCoreApplication::translate("Receipt", "Please keep this ticket to collect your order."), "signature");
        Line blank = b.base(QString(), 0, "signature");
        b.lines << blank;
        b.text(QCoreApplication::translate("Receipt", "Signature"), "signature", 0);
        b.lines << blank;
        b.text(QString(b.width * 2 / 3, '_'), "signature", 0);
    }
    return b.lines;
}

QImage logoBitmap(const QImage& logo, int dots)
{
    if (logo.isNull() || dots <= 0)
        return {};
    const QImage scaled = logo.scaled(dots, dots, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage flat(scaled.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);  // transparent logos print on white
    QPainter painter(&flat);
    painter.drawImage(0, 0, scaled);
    painter.end();
    return flat.convertToFormat(QImage::Format_Mono, Qt::MonoOnly | Qt::OrderedDither);
}

namespace {

QString joined(const DataRow& row, std::initializer_list<const char*> keys, const QString& separator)
{
    QStringList values;
    for (const char* key : keys) {
        const QString value = row.value(key).toString().trimmed();
        if (!value.isEmpty())
            values << value;
    }
    return values.join(separator);
}

ShopInfo loadShop(const DataRow& row, LocalImages* images)
{
    ShopInfo shop;
    shop.name = row.value("shop_name").toString();
    shop.address = joined(row, {"address", "postal", "city"}, ", ");
    shop.phone = row.value("phone").toString();
    shop.email = row.value("email").toString();
    shop.website = row.value("website").toString();
    shop.social = joined(row, {"facebook", "instagram", "tiktok", "whatsapp"}, "  ");
    shop.logoPath = row.value("logo_path").toString();
    if (images && !shop.logoPath.isEmpty())
        shop.logoImage = images->image(shop.logoPath);
    return shop;
}

} // namespace

TicketData loadTicket(LocalApi* api, LocalImages* images, int ticketId)
{
    TicketData data;
    const DataRow profile = api->profile();
    data.shop = loadShop(profile, images);
    data.design = fromJson(profile.value("receipt_design").toString());

    const DataRow ticket = api->ticket(ticketId);
    data.ticketNumber = ticket.value("ticket_number").toString();
    const QDate date = QDate::fromString(ticket.value("date").toString(), "yyyy-MM-dd");
    data.date = date.isValid() ? AppHelpers::formatDate(date) : ticket.value("date").toString();
    data.time = ticket.value("time").toString().left(5);
    data.cashier = ticket.value("seller_display_name").toString();
    if (data.cashier.isEmpty())
        data.cashier = ticket.value("seller_username").toString();
    data.customer = ticket.value("customer_name").toString();
    data.description = ticket.value("description").toString();
    data.subtotal = ticket.value("subtotal").toDouble();
    data.discountAmount = ticket.value("discount_amount").toDouble();
    data.total = ticket.value("total").toDouble();
    data.advance = ticket.value("deposit_amount").toDouble();
    data.isUnpaid = ticket.value("status").toString() == QLatin1String("OPEN");

    const QList<DataRow> rows = api->ticketItems(ticketId);
    for (const DataRow& row : rows) {
        TicketItem item;
        const QString name = row.value("product_name").toString();
        const QString variant = row.value("variant_name").toString();
        item.name = variant.isEmpty() ? name : QString("%1 - %2").arg(name, variant);
        item.quantity = row.value("quantity").toInt();
        item.unitPrice = row.value("unit_price").toDouble();
        data.items << item;
    }
    return data;
}

TicketData sampleTicket(LocalApi* api, LocalImages* images)
{
    TicketData data;
    const DataRow profile = api ? api->profile() : DataRow();
    data.shop = loadShop(profile, images);
    data.design = fromJson(profile.value("receipt_design").toString());

    // Shop details not filled in yet still show where they would print.
    const auto fill = [](QString& value, const QString& placeholder) {
        if (value.trimmed().isEmpty())
            value = placeholder;
    };
    fill(data.shop.name, QCoreApplication::translate("Receipt", "Your Shop"));
    fill(data.shop.address, QCoreApplication::translate("Receipt", "(address)"));
    fill(data.shop.phone, QCoreApplication::translate("Receipt", "(phone)"));
    fill(data.shop.email, QCoreApplication::translate("Receipt", "(email)"));
    fill(data.shop.website, QCoreApplication::translate("Receipt", "(website)"));
    fill(data.shop.social, QCoreApplication::translate("Receipt", "(social media)"));

    data.ticketNumber = "1042";
    data.date = AppHelpers::formatDate(QDate::currentDate());
    data.time = QTime::currentTime().toString("HH:mm");
    data.cashier = "Sara";
    data.customer = "Karim Haddad";
    data.description = QCoreApplication::translate("Receipt", "Broken screen, replace and test");
    data.items = {{QCoreApplication::translate("Receipt", "Cappuccino"), 2, 3.5}, {QCoreApplication::translate("Receipt", "Croissant"), 3, 2.0}, {QCoreApplication::translate("Receipt", "Fresh orange juice"), 1, 5.5}, {QCoreApplication::translate("Receipt", "Bagel"), 1, 5.0}};
    data.subtotal = 23.5;
    data.discountAmount = 2.35;
    data.total = 21.15;
    data.advance = 10.0;
    return data;
}

} // namespace Receipt
