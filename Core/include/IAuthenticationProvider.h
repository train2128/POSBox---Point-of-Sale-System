#ifndef IAUTHENTICATIONPROVIDER_H
#define IAUTHENTICATIONPROVIDER_H

#include <QString>

/**
 * Authentication result structure
 */
struct AuthResult {
    bool success;
    QString username;
    QString role;
    QString displayName;
    QString errorMessage;

    AuthResult() : success(false) {}

    static AuthResult Success(const QString& username, const QString& role, const QString& displayName) {
        AuthResult result;
        result.success = true;
        result.username = username;
        result.role = role;
        result.displayName = displayName;
        return result;
    }

    static AuthResult Failure(const QString& errorMessage) {
        AuthResult result;
        result.success = false;
        result.errorMessage = errorMessage;
        return result;
    }
};

/**
 * Abstract authentication provider interface
 * Allows switching between local database and remote server authentication
 */
class IAuthenticationProvider {
public:
    virtual ~IAuthenticationProvider() = default;

    /**
     * Authenticate user with username and password
     * @return AuthResult with success status and user info
     */
    virtual AuthResult authenticate(const QString& username, const QString& password) = 0;

    /**
     * Check if the authentication provider is available and ready
     * @return true if provider can be used
     */
    virtual bool isAvailable() const = 0;

    /**
     * Get provider name for debugging/logging
     */
    virtual QString providerName() const = 0;
};

#endif // IAUTHENTICATIONPROVIDER_H
