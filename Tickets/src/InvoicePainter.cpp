#include "InvoicePainter.h"
#include "Prefixes.h"
#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QPaintDevice>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QtMath>

namespace {

constexpr double PageWidth = 210.0;   // All layout values are millimetres.
constexpr double PageHeight = 297.0;
constexpr double Left = 16.0;
constexpr double Width = PageWidth - 2 * Left;

constexpr double RowMaxHeight = 8.5;
constexpr double RowMaxPoints = 10.0;
constexpr double RowMinPoints = 3.0;
constexpr double NameMinPoints = 7.0;   // A long name shrinks to this, then is cut.

constexpr double FooterY = 279.0;
constexpr double SignatureHeight = 32.0;
constexpr double SignatureY = FooterY - SignatureHeight - 4.0;
constexpr double TotalsRowHeight = 7.0;
constexpr double TotalBarHeight = 9.0;
constexpr double TableHeaderHeight = 9.0;
constexpr double TableMinY = 85.0;

const QColor Ink("#111111");
const QColor Muted("#6B6B6B");
const QColor HairLine("#D9D9D9");

QString money(double amount)
{
    return AppHelpers::formatPrice(amount);
}



QString joined(const QString& first, const QString& second, const QString& separator)
{
    if (first.isEmpty()) return second;
    if (second.isEmpty()) return first;
    return first + separator + second;
}

// Draws in millimetres on a device of any resolution.
class Canvas
{
public:
    explicit Canvas(QPainter& painter)
        : m_painter(painter)
        , m_dpi(painter.device()->logicalDpiX())
    {
    }

    double mm(double value) const { return value * m_dpi / 25.4; }

    QFont font(double points, bool bold, double spacingPoints = 0.0) const
    {
        QFont f("Arial");
        f.setPointSizeF(points);
        f.setBold(bold);
        // Relative spacing: absolute spacing comes out far wider in a PDF.
        if (spacingPoints > 0.0)
            f.setLetterSpacing(QFont::PercentageSpacing, 100.0 + 100.0 * spacingPoints / points);
        return f;
    }

    double textWidth(const QString& text, double points, bool bold) const
    {
        return QFontMetricsF(font(points, bold), m_painter.device()).horizontalAdvance(text) * 25.4 / m_dpi;
    }

    double wrappedHeight(const QString& text, double width, double points, bool bold) const
    {
        const QRectF bounds(0, 0, mm(width), mm(PageHeight));
        const QRectF used = QFontMetricsF(font(points, bold), m_painter.device())
                                .boundingRect(bounds, Qt::TextWordWrap, text);
        return used.height() * 25.4 / m_dpi;
    }

    void text(const QRectF& box, int flags, const QString& value, double points, bool bold = false,
              const QColor& color = Ink, double spacingPoints = 0.0)
    {
        const QFont f = font(points, bold, spacingPoints);
        const QRectF area(mm(box.x()), mm(box.y()), mm(box.width()), mm(box.height()));
        const QString shown = (flags & Qt::TextWordWrap)
            ? value
            : QFontMetricsF(f, m_painter.device()).elidedText(value, Qt::ElideRight, area.width());
        m_painter.setFont(f);
        m_painter.setPen(color);
        m_painter.drawText(area, flags, shown);
    }

    void fill(const QRectF& box, const QColor& color)
    {
        m_painter.fillRect(QRectF(mm(box.x()), mm(box.y()), mm(box.width()), mm(box.height())), color);
    }

    void line(double x, double y, double width, double thickness, const QColor& color)
    {
        fill(QRectF(x, y - thickness / 2, width, thickness), color);
    }

    void frame(const QRectF& box, double thickness, const QColor& color)
    {
        QPen pen(color);
        pen.setWidthF(mm(thickness));
        m_painter.setPen(pen);
        m_painter.setBrush(Qt::NoBrush);
        m_painter.drawRect(QRectF(mm(box.x()), mm(box.y()), mm(box.width()), mm(box.height())));
    }

    void image(const QImage& source, const QRectF& box)
    {
        m_painter.drawImage(QRectF(mm(box.x()), mm(box.y()), mm(box.width()), mm(box.height())), source);
    }

private:
    QPainter& m_painter;
    double m_dpi;
};

// A labelled block of lines under the header. Returns where it ends.
struct PartyBlock {
    QString label;
    QString name;
    QStringList lines;
};

double drawParty(Canvas& canvas, double x, double y, double width, const PartyBlock& block)
{
    canvas.text({x, y, width, 5}, Qt::AlignLeft, block.label, 7.5, true, Muted, 0.36);
    y += 5;
    if (!block.name.isEmpty()) {
        const double height = qMax(6.0, canvas.wrappedHeight(block.name, width, 12, true) + 0.5);
        canvas.text({x, y, width, height}, Qt::AlignLeft | Qt::TextWordWrap, block.name, 12, true);
        y += height;
    }
    for (const QString& line : block.lines) {
        const double height = qMax(5.0, canvas.wrappedHeight(line, width, 9, false) + 1.0);
        canvas.text({x, y, width, height}, Qt::AlignLeft | Qt::TextWordWrap, line, 9);
        y += height;
    }
    return y;
}

void drawHeader(Canvas& canvas, const InvoiceContent& invoice)
{
    constexpr double LogoMaxWidth = 70.0, LogoMaxHeight = 30.0, LogoY = 14.0;

    if (!invoice.logo.isNull()) {
        const double aspect = double(invoice.logo.width()) / invoice.logo.height();
        double height = LogoMaxHeight;
        double width = height * aspect;
        if (width > LogoMaxWidth) {
            width = LogoMaxWidth;
            height = width / aspect;
        }
        canvas.image(invoice.logo, {Left, LogoY + (LogoMaxHeight - height) / 2, width, height});
    } else {
        canvas.text({Left, LogoY, 90, 30}, Qt::AlignLeft | Qt::AlignVCenter, invoice.shopName, 20, true);
    }

    canvas.text({Left, 14, Width, 13}, Qt::AlignRight | Qt::AlignTop, QCoreApplication::translate("Invoice", "INVOICE"), 28, true, Ink, 0.72);
    canvas.text({Left, 29, Width, 6}, Qt::AlignRight, QCoreApplication::translate("Invoice", "No. %1").arg(invoice.number), 11, true);
    canvas.text({Left, 35, Width, 6}, Qt::AlignRight, QCoreApplication::translate("Invoice", "Date: %1").arg(invoice.date), 10, false, Muted);
    canvas.line(Left, 49, Width, 0.6, Ink);
}

double drawParties(Canvas& canvas, const InvoiceContent& invoice)
{
    constexpr double Top = 54.0;
    const double columnWidth = Width / 2 - 4;

    PartyBlock shop;
    shop.label = QCoreApplication::translate("Invoice", "FROM");
    shop.name = invoice.shopName;
    const QString shopPlace = joined(invoice.shopAddress, invoice.shopPostalCity, ", ");
    if (!shopPlace.isEmpty()) shop.lines << shopPlace;
    return drawParty(canvas, Left, Top, columnWidth, shop);
}

// Returns where the table ends.
double drawItems(Canvas& canvas, const InvoiceContent& invoice, double top, double available)
{
    constexpr double QuantityWidth = 18.0, UnitWidth = 34.0, TotalWidth = 34.0;
    const double nameWidth = Width - QuantityWidth - UnitWidth - TotalWidth;
    const double quantityX = Left + nameWidth;
    const double unitX = quantityX + QuantityWidth;
    const double totalX = Left + Width - TotalWidth - 1;
    constexpr int Right = Qt::AlignRight | Qt::AlignVCenter;
    constexpr int LeftAligned = Qt::AlignLeft | Qt::AlignVCenter;
    constexpr double Spacing = 0.22;

    canvas.text({Left + 1, top, nameWidth, 8}, LeftAligned, QCoreApplication::translate("Invoice", "DESCRIPTION"), 8, true, Ink, Spacing);
    canvas.text({quantityX, top, QuantityWidth, 8}, Right, QCoreApplication::translate("Invoice", "QTY"), 8, true, Ink, Spacing);
    canvas.text({unitX, top, UnitWidth, 8}, Right, QCoreApplication::translate("Invoice", "UNIT PRICE"), 8, true, Ink, Spacing);
    canvas.text({totalX, top, TotalWidth, 8}, Right, QCoreApplication::translate("Invoice", "TOTAL"), 8, true, Ink, Spacing);
    canvas.line(Left, top + 8.5, Width, 0.4, Ink);

    double y = top + TableHeaderHeight;
    if (invoice.items.isEmpty()) {
        canvas.text({Left + 1, y, Width, RowMaxHeight}, LeftAligned, QCoreApplication::translate("Invoice", "No items."), RowMaxPoints, false, Muted);
        return y + RowMaxHeight;
    }

    const double rowHeight = qMin(RowMaxHeight, available / invoice.items.size());
    const double points = qBound(RowMinPoints, rowHeight * 2.0, RowMaxPoints);
    const double nameSpace = nameWidth - 2;

    for (const InvoiceContent::Item& item : invoice.items) {
        double namePoints = points;
        const double fullWidth = canvas.textWidth(item.name, points, false);
        if (fullWidth > nameSpace)
            namePoints = qMax(qMin(points, NameMinPoints), points * nameSpace / fullWidth);

        canvas.text({Left + 1, y, nameSpace, rowHeight}, LeftAligned, item.name, namePoints);
        canvas.text({quantityX, y, QuantityWidth, rowHeight}, Right, QString::number(item.quantity), points);
        canvas.text({unitX, y, UnitWidth, rowHeight}, Right, money(item.unitPrice), points);
        canvas.text({totalX, y, TotalWidth, rowHeight}, Right, money(item.lineTotal), points, true);
        y += rowHeight;
        canvas.line(Left, y, Width, 0.15, HairLine);
    }
    return y;
}

void drawTotals(Canvas& canvas, const InvoiceContent& invoice, double y, bool showDiscount)
{
    constexpr double BlockWidth = 80.0;
    const double x = Left + Width - BlockWidth;

    if (showDiscount) {
        const auto row = [&](const QString& label, const QString& value) {
            canvas.text({x + 2, y, 40, TotalsRowHeight}, Qt::AlignLeft | Qt::AlignVCenter, label, 9.5, false, Muted);
            canvas.text({x, y, BlockWidth - 2, TotalsRowHeight}, Qt::AlignRight | Qt::AlignVCenter, value, 9.5, true);
            y += TotalsRowHeight;
        };
        row(QCoreApplication::translate("Invoice", "Subtotal"), money(invoice.subtotal));
        row(QCoreApplication::translate("Invoice", "Discount"), "- " + money(invoice.discount));
    }

    canvas.fill({x, y, BlockWidth, TotalBarHeight}, Ink);
    canvas.text({x + 2, y, 30, TotalBarHeight}, Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("Invoice", "TOTAL"), 10.5, true, Qt::white, 0.22);
    canvas.text({x, y, BlockWidth - 2, TotalBarHeight}, Qt::AlignRight | Qt::AlignVCenter, money(invoice.total), 12, true, Qt::white);
}

void drawClosing(Canvas& canvas, const InvoiceContent& invoice)
{
    constexpr double BoxWidth = 70.0;
    const double boxX = Left + Width - BoxWidth;
    const QString place = invoice.shopCity.isEmpty() ? invoice.date
                                                     : QCoreApplication::translate("Invoice", "%1, %2").arg(invoice.shopCity, invoice.date);
    canvas.frame({boxX, SignatureY, BoxWidth, SignatureHeight}, 0.25, Ink);
    canvas.text({boxX, SignatureY + 2, BoxWidth, 6}, Qt::AlignCenter, QCoreApplication::translate("Invoice", "Stamp and signature"), 8.5, true);
    canvas.text({boxX, SignatureY + SignatureHeight - 7, BoxWidth, 5}, Qt::AlignCenter, place, 8, false, Muted);
}

void drawFooter(Canvas& canvas, const InvoiceContent& invoice)
{
    if (invoice.footerItems.isEmpty())
        return;
    canvas.line(Left, FooterY, Width, 0.2, HairLine);
    canvas.text({Left, FooterY + 2, Width, 10}, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                invoice.footerItems.join(QString::fromUtf8("   •   ")), 8, false, Muted);
}

} // namespace

namespace InvoicePainter {


void paint(QPainter& painter, const InvoiceContent& invoice)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    Canvas canvas(painter);

    // Designed for A4; another paper size chosen in the print dialog still fits.
    const double scale = qMin(painter.device()->width() / canvas.mm(PageWidth),
                              painter.device()->height() / canvas.mm(PageHeight));
    painter.scale(scale, scale);

    drawHeader(canvas, invoice);
    const double partiesEnd = drawParties(canvas, invoice);

    const bool showDiscount = invoice.discount >= 0.0005;
    const double totalsHeight = (showDiscount ? 2 : 0) * TotalsRowHeight + TotalBarHeight + 2.0;
    const double tableTop = qMax(TableMinY, partiesEnd + 5.0);
    const double available = SignatureY - 4.0 - totalsHeight - tableTop - TableHeaderHeight;

    const double tableEnd = drawItems(canvas, invoice, tableTop, available);
    drawTotals(canvas, invoice, tableEnd + 2.0, showDiscount);
    drawClosing(canvas, invoice);
    drawFooter(canvas, invoice);

    painter.restore();
}

} // namespace InvoicePainter
