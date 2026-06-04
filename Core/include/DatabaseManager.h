#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr, const QString& connectionName = "default");
    ~DatabaseManager();

    // Connection management
    bool isOpen() const;
    QString databasePath() const;
    QString connectionName() const { return m_connectionName; }

    // Get the underlying QSqlDatabase for custom queries
    QSqlDatabase database() const { return m_db; }

    // Error handling
    QString lastError() const { return m_lastError; }

signals:
    // Emitted when data changes - for IPC notification
    void productChanged(int productId);
    void categoryChanged(int categoryId);
    void ticketCreated(int ticketId);
    void ticketUpdated(int ticketId);
    void expenseUpdated(int expenseId);
    void accountChanged(const QString& username);

private:
    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;

    bool openConnection();
};

#endif // DATABASEMANAGER_H
