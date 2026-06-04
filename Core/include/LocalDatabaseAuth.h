#ifndef LOCALDATABASEAUTH_H
#define LOCALDATABASEAUTH_H

#include "IAuthenticationProvider.h"

class DatabaseManager;

/**
 * Local SQLite database authentication provider
 */
class LocalDatabaseAuth : public IAuthenticationProvider {
public:
    explicit LocalDatabaseAuth(DatabaseManager* dbManager);
    ~LocalDatabaseAuth() override = default;

    AuthResult authenticate(const QString& username, const QString& password) override;
    bool isAvailable() const override;
    QString providerName() const override { return "Local Database"; }

private:
    DatabaseManager* m_dbManager;
};

#endif // LOCALDATABASEAUTH_H
