#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

/**
 * @brief User roles for access control
 */
enum UserRole {
    VIEWER = 0,   // Can only view timer
    OPERATOR = 1, // Can control timer and modify settings
    ADMIN = 2     // Full access including user management and factory reset
};

/**
 * @brief User account structure
 */
struct User {
    String username;
    String password;
    UserRole role;
};

/**
 * @brief User management class for role-based authentication
 *
 * Manages three types of users:
 * - Viewers: No authentication needed, read-only access
 * - Operators: Username/password, can control timer
 * - Admin: Special account, full access
 */
class UserManager {
public:
    /**
     * @brief Constructor
     */
    UserManager();

    /**
     * @brief Initialize user management and load from NVS
     */
    void begin();

    /**
     * @brief Authenticate a user
     * @param username Username
     * @param password Password
     * @return UserRole if authenticated, VIEWER if failed
     */
    UserRole authenticate(const String& username, const String& password);

    /**
     * @brief Add a new operator (admin only)
     * @param username Username for new operator
     * @param password Password for new operator
     * @return True if added successfully
     */
    bool addOperator(const String& username, const String& password);

    /**
     * @brief Remove an operator (admin only)
     * @param username Username to remove
     * @return True if removed successfully
     */
    bool removeOperator(const String& username);

    /**
     * @brief Change user password
     * @param username Username
     * @param oldPassword Current password
     * @param newPassword New password
     * @return True if changed successfully
     */
    bool changePassword(const String& username, const String& oldPassword, const String& newPassword);

    /**
     * @brief Get list of all operators (admin only)
     * @return Vector of operator usernames
     */
    std::vector<String> getOperators();

    /**
     * @brief Reset to factory defaults
     * @return True if reset successfully
     */
    bool factoryReset();

    /**
     * @brief Get admin username
     */
    String getAdminUsername() const { return adminUsername; }

    /**
     * @brief Check if username exists
     */
    bool usernameExists(const String& username);

private:
    String adminUsername;
    String adminPasswordHash;  // Changed: store hash instead of plaintext
    std::vector<User> operators;

    static const int MAX_OPERATORS = 10;
    static const char* PREF_NAMESPACE;
    static const char* PREF_ADMIN_USER;
    static const char* PREF_ADMIN_PASS;
    static const char* PREF_OPERATOR_COUNT;
    static const char* PREF_OPERATOR_PREFIX;

    /**
     * @brief Load users from NVS
     */
    void load();

    /**
     * @brief Save users to NVS
     */
    void save();

    /**
     * @brief Set default admin credentials
     */
    void setDefaults();

    /**
     * @brief Hash a password using SHA-256
     * @param password Plaintext password
     * @return Hexadecimal string representation of hash
     */
    String hashPassword(const String& password);

    /**
     * @brief Verify password against stored hash
     * @param password Plaintext password to check
     * @param hash Stored password hash
     * @return True if password matches hash
     */
    bool verifyPassword(const String& password, const String& hash);
};
