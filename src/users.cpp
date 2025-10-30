#include "users.h"
#include "mbedtls/sha256.h"

// NVS keys
const char* UserManager::PREF_NAMESPACE = "users";
const char* UserManager::PREF_ADMIN_USER = "admin_user";
const char* UserManager::PREF_ADMIN_PASS = "admin_pass";
const char* UserManager::PREF_OPERATOR_COUNT = "op_count";
const char* UserManager::PREF_OPERATOR_PREFIX = "op_"; // op_0_user, op_0_pass, etc.

UserManager::UserManager()
    : adminUsername("admin")
    , adminPasswordHash("")  // Will be set on first load
{
}

void UserManager::begin() {
    load();
}

void UserManager::load() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, true)) {
        Serial.println("Failed to open user preferences (read-only). Using defaults.");
        setDefaults();
        save(); // Save defaults
        return;
    }

    // Load admin credentials
    adminUsername = prefs.getString(PREF_ADMIN_USER, "admin");
    String storedPass = prefs.getString(PREF_ADMIN_PASS, "");

    // Migration: Check if password is already hashed
    // A valid hash is exactly 64 hex characters (SHA-256)
    if (storedPass.length() == 0 || !isValidHash(storedPass)) {
        // First time or plaintext - hash the password
        Serial.println("Migrating admin password to hashed format");
        if (storedPass.length() == 0) {
            storedPass = "admin"; // Default password
        }
        adminPasswordHash = hashPassword(storedPass);
        prefs.end();
        save(); // Save the hashed version
        prefs.begin(PREF_NAMESPACE, true); // Reopen as read-only
    } else {
        // Already hashed (valid 64-char hex string)
        adminPasswordHash = storedPass;
    }

    // Load operators
    operators.clear();
    int operatorCount = prefs.getInt(PREF_OPERATOR_COUNT, 0);
    bool needsMigration = false;

    for (int i = 0; i < operatorCount && i < MAX_OPERATORS; i++) {
        String userKey = String(PREF_OPERATOR_PREFIX) + String(i) + "_user";
        String passKey = String(PREF_OPERATOR_PREFIX) + String(i) + "_pass";

        String username = prefs.getString(userKey.c_str(), "");
        String password = prefs.getString(passKey.c_str(), "");

        if (username.length() > 0 && password.length() > 0) {
            User op;
            op.username = username;

            // Migration: Check if password is already hashed (valid 64-char hex)
            if (!isValidHash(password)) {
                Serial.printf("Migrating password for operator '%s' to hashed format\n", username.c_str());
                op.password = hashPassword(password);
                needsMigration = true;
            } else {
                // Already hashed
                op.password = password;
            }

            op.role = OPERATOR;
            operators.push_back(op);
        }
    }

    prefs.end();

    // Save migrated passwords
    if (needsMigration) {
        save();
    }

    Serial.printf("Loaded %d operator(s) from NVS\n", operators.size());
}

void UserManager::save() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, false)) {
        Serial.println("Failed to open user preferences for writing.");
        return;
    }

    // Save admin credentials (hash, not plaintext)
    prefs.putString(PREF_ADMIN_USER, adminUsername);
    prefs.putString(PREF_ADMIN_PASS, adminPasswordHash);

    // Save operators
    prefs.putInt(PREF_OPERATOR_COUNT, operators.size());

    for (size_t i = 0; i < operators.size() && i < MAX_OPERATORS; i++) {
        String userKey = String(PREF_OPERATOR_PREFIX) + String(i) + "_user";
        String passKey = String(PREF_OPERATOR_PREFIX) + String(i) + "_pass";

        prefs.putString(userKey.c_str(), operators[i].username);
        prefs.putString(passKey.c_str(), operators[i].password);
    }

    prefs.end();

    Serial.printf("Saved %d operator(s) to NVS\n", operators.size());
}

void UserManager::setDefaults() {
    adminUsername = "admin";
    adminPasswordHash = hashPassword("admin");
    operators.clear();

    Serial.println("User credentials reset to factory defaults");
}

UserRole UserManager::authenticate(const String& username, const String& password) {
    // Check admin
    if (username == adminUsername && verifyPassword(password, adminPasswordHash)) {
        Serial.println("User authenticated as ADMIN");
        return ADMIN;
    }

    // Check operators
    for (const auto& op : operators) {
        if (op.username == username && verifyPassword(password, op.password)) {
            Serial.println("User authenticated as OPERATOR");
            return OPERATOR;
        }
    }

    // Authentication failed
    Serial.println("Authentication failed");
    return VIEWER;
}

bool UserManager::addOperator(const String& username, const String& password) {
    // Check if max operators reached
    if (operators.size() >= MAX_OPERATORS) {
        Serial.println("Cannot add operator: maximum limit reached");
        return false;
    }

    // Check for empty credentials
    if (username.length() == 0 || password.length() == 0) {
        Serial.println("Cannot add operator: empty username or password");
        return false;
    }

    // Check minimum password length (security requirement)
    if (password.length() < MIN_PASSWORD_LENGTH) {
        Serial.printf("Cannot add operator: password must be at least %d characters\n", MIN_PASSWORD_LENGTH);
        return false;
    }

    // Check if username already exists
    if (usernameExists(username)) {
        Serial.println("Cannot add operator: username already exists");
        return false;
    }

    // Add operator with hashed password
    User newOp;
    newOp.username = username;
    newOp.password = hashPassword(password);  // Store hash, not plaintext
    newOp.role = OPERATOR;
    operators.push_back(newOp);

    save();

    Serial.printf("Added operator '%s'\n", username.c_str());
    return true;
}

bool UserManager::removeOperator(const String& username) {
    // Find and remove operator
    for (auto it = operators.begin(); it != operators.end(); ++it) {
        if (it->username == username) {
            operators.erase(it);
            save();
            Serial.printf("Removed operator '%s'\n", username.c_str());
            return true;
        }
    }

    Serial.printf("Cannot remove operator: '%s' not found\n", username.c_str());
    return false;
}

bool UserManager::changePassword(const String& username, const String& oldPassword, const String& newPassword) {
    // Check new password is not empty
    if (newPassword.length() == 0) {
        Serial.println("Cannot change password: new password is empty");
        return false;
    }

    // Check minimum password length (security requirement)
    if (newPassword.length() < MIN_PASSWORD_LENGTH) {
        Serial.printf("Cannot change password: new password must be at least %d characters\n", MIN_PASSWORD_LENGTH);
        return false;
    }

    // Check admin
    if (username == adminUsername) {
        if (verifyPassword(oldPassword, adminPasswordHash)) {
            adminPasswordHash = hashPassword(newPassword);  // Store new hash
            save();
            Serial.printf("Password changed for admin user '%s'\n", username.c_str());
            return true;
        } else {
            Serial.println("Cannot change password: incorrect old password");
            return false;
        }
    }

    // Check operators
    for (auto& op : operators) {
        if (op.username == username) {
            if (verifyPassword(oldPassword, op.password)) {
                op.password = hashPassword(newPassword);  // Store new hash
                save();
                Serial.printf("Password changed for operator '%s'\n", username.c_str());
                return true;
            } else {
                Serial.println("Cannot change password: incorrect old password");
                return false;
            }
        }
    }

    Serial.printf("Cannot change password: user '%s' not found\n", username.c_str());
    return false;
}

std::vector<String> UserManager::getOperators() {
    std::vector<String> usernames;
    for (const auto& op : operators) {
        usernames.push_back(op.username);
    }
    return usernames;
}

bool UserManager::usernameExists(const String& username) {
    // Check admin
    if (username == adminUsername) {
        return true;
    }

    // Check operators
    for (const auto& op : operators) {
        if (op.username == username) {
            return true;
        }
    }

    return false;
}

bool UserManager::factoryReset() {
    Serial.println("Performing factory reset...");

    setDefaults();
    save();

    Serial.println("Factory reset complete");
    return true;
}

String UserManager::hashPassword(const String& password) {
    // Use SHA-256 to hash the password
    unsigned char hash[32];
    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);

    // Error handling: Check if initialization succeeds
    int ret = mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (not SHA-224)
    if (ret != 0) {
        Serial.printf("SHA-256 start failed with error: %d\n", ret);
        mbedtls_sha256_free(&ctx);
        return ""; // Return empty string on error
    }

    ret = mbedtls_sha256_update(&ctx, (unsigned char*)password.c_str(), password.length());
    if (ret != 0) {
        Serial.printf("SHA-256 update failed with error: %d\n", ret);
        mbedtls_sha256_free(&ctx);
        return "";
    }

    ret = mbedtls_sha256_finish(&ctx, hash);
    if (ret != 0) {
        Serial.printf("SHA-256 finish failed with error: %d\n", ret);
        mbedtls_sha256_free(&ctx);
        return "";
    }

    mbedtls_sha256_free(&ctx);

    // Convert hash to hexadecimal string
    String hashStr = "";
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        hashStr += hex;
    }

    return hashStr;
}

bool UserManager::verifyPassword(const String& password, const String& hash) {
    String computedHash = hashPassword(password);

    // If hashing failed, return false
    if (computedHash.length() == 0) {
        Serial.println("Password verification failed: hashing error");
        return false;
    }

    return computedHash.equals(hash);
}

bool UserManager::isValidHash(const String& str) {
    // SHA-256 hash in hex is exactly 64 characters
    if (str.length() != 64) {
        return false;
    }

    // Check if all characters are valid hex (0-9, a-f)
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }

    return true;
}
