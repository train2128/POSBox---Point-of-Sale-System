#ifndef TICKETPRINTMANAGER_H
#define TICKETPRINTMANAGER_H

#include <QObject>
#include <QSqlRecord>
#include <QList>

class QTextDocument;
class QTextCursor;
class QTextCharFormat;
class QTextBlockFormat;
class QPrinter;
class DatabaseManager;
class QWidget;

struct CustomerInvoiceData {
    QString companyName;
    QString matriculeFiscal;
    QString address;
    QString phone;
    bool isEmpty() const {
        return companyName.isEmpty() && matriculeFiscal.isEmpty() &&
               address.isEmpty() && phone.isEmpty();
    }
};

/**
 * Handles ticket invoice generation and printing.
 * Produces an A4 invoice with shop header, items table, totals, stamp box, and footer.
 */
class TicketPrintManager : public QObject
{
    Q_OBJECT

public:
    explicit TicketPrintManager(DatabaseManager* db, QWidget* parent = nullptr);

    // Show print preview dialog then print
    void printTicketWithPreview(int ticketId, const CustomerInvoiceData& customerData = CustomerInvoiceData());

private slots:
    void printPreview(QPrinter* printer);

private:
    // Document generation
    void generateInvoiceDocument(QTextDocument* document, int ticketId, const CustomerInvoiceData& customerData);
    void addCompanyHeader(QTextCursor& cursor);
    void addInvoiceTitle(QTextCursor& cursor, const QString& ticketNumber, const QString& date);
    void addCompanyInfo(QTextCursor& cursor);
    void addCustomerDetails(QTextCursor& cursor, const CustomerInvoiceData& customerData);
    void addItemsTable(QTextCursor& cursor, const QList<QSqlRecord>& items,
                       double subtotal, double discountAmount, double total);
    void addSummary(QTextCursor& cursor, double total);
    void addStampAndSignature(QTextCursor& cursor);
    void addFooter(QTextCursor& cursor);

    // PDF save
    void savePdfToInvoices(const QString& ticketNumber, QTextDocument* document);

    // DB helpers
    QSqlRecord getShopProfileRecord();
    QSqlRecord getTicketRecord(int ticketId);
    QList<QSqlRecord> getTicketItemsRecord(int ticketId);
    QString documentsRootPath() const;

    // Number to French words (for invoice total line)
    QString numberToFrenchWords(double amount);
    QString formatCurrency(double amount);

    // Text format helpers
    QTextCharFormat getTitleFormat();
    QTextCharFormat getHeaderFormat();
    QTextCharFormat getNormalFormat();
    QTextCharFormat getBoldFormat();
    QTextCharFormat getCompanyNameFormat();
    QTextBlockFormat getCenterFormat();
    QTextBlockFormat getLeftFormat();
    QTextBlockFormat getRightFormat();

    DatabaseManager*  m_db;
    QWidget*          m_parent;
    QTextDocument*    m_currentDocument;
    int               m_currentTicketId;
    CustomerInvoiceData m_currentCustomerData;
};

#endif // TICKETPRINTMANAGER_H
