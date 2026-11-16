#ifndef RECEIPTDESIGN_H
#define RECEIPTDESIGN_H

#include <QImage>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

class LocalApi;
class LocalImages;

// What a receipt says and how it looks. One layout function produces the lines
// for both the printer and the Settings preview, so what you see is what prints.
namespace Receipt {

constexpr int ModelCount = 9;

struct Design {
    int model = 2;              // 1..ModelCount
    QSet<QString> hidden;       // parts left out
    QString topMessage;         // empty: the model's own, if it has one
    QString bottomMessage;      // empty: the usual thank-you
    QString logoSize = "Medium";
    QString totalSize = "Normal";
    QString lines = "Dashes";
    QString spacing = "Normal";
};

struct ShopInfo {
    QString name;
    QString address;
    QString phone;
    QString email;
    QString website;
    QString social;
    QString logoPath;
    QImage logoImage;
};

struct TicketItem {
    QString name;
    int quantity = 0;
    double unitPrice = 0.0;
};

struct TicketData {
    ShopInfo shop;
    Design design;
    QString ticketNumber;
    QString date;
    QString time;
    QString cashier;
    QString customer;
    QString description;
    QList<TicketItem> items;
    double subtotal = 0.0;
    double discountAmount = 0.0;
    double total = 0.0;
    double advance = 0.0;
    bool isUnpaid = false;      // true for OPEN tickets
};

// Part of a line belonging to one part of the receipt, in characters.
struct Span {
    int start = 0;
    int length = 0;
    QString part;
};

// One printed line. `part` names what it belongs to; empty when it can't be hidden.
struct Line {
    QString text;
    int align = 0;              // 0 left, 1 center, 2 right
    bool bold = false;
    bool doubleHeight = false;
    bool doubleWidth = false;
    bool reversed = false;      // white on black
    bool small = false;         // the printer's smaller font
    bool isLogo = false;
    int logoDots = 0;
    QString part;
    QList<Span> spans;          // when several parts share the line
};

QString modelName(int model);
// Goes back to what a model starts with, keeping the language and messages.
void applyModel(Design& design, int model);

QString toJson(const Design& design);
Design fromJson(const QString& json);

// A name to show for a part that can be hidden.
QString partName(const QString& part);

// Paper sizes, as the printer sees them.
int dotsFor(const QString& paperSize);
int columnsFor(const QString& paperSize);
int smallColumnsFor(const QString& paperSize);

QList<Line> layout(const TicketData& data, const QString& paperSize);

// The logo as the printer draws it: black and white, `dots` wide at most.
QImage logoBitmap(const QImage& logo, int dots);

// Reads a ticket and the saved design from the database, ready to print.
TicketData loadTicket(LocalApi* api, LocalImages* images, int ticketId);
// A made-up ticket with the real shop details, for the preview and test prints.
TicketData sampleTicket(LocalApi* api, LocalImages* images);

} // namespace Receipt

#endif // RECEIPTDESIGN_H
