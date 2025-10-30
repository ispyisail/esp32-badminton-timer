#include "users.h"

// NVS keys
const char* UserManager::PREF_NAMESPACE = "users";
const char* UserManager::PREF_ADMIN_USER = "admin_user";
const char* UserManager::PREF_ADMIN_PASS = "admin_pass";
const char* UserManager::PREF_OPERATOR_COUNT = "op_count";
const char* UserManager::PREF_OPERATOR_PREFIX = "op_"; // op_0_user, op_0_pass, etc.

UserManager::UserManager()
    : adminUsername("admin")
    , adminPassword("admin")
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
    adminPassword = prefs.getString(PREF_ADMIN_PASS, "admin");

    // Load operators
    operators.clear();
    int operatorCount = prefs.getInt(PREF_OPERATOR_COUNT, 0);

    for (int i = 0; i < operatorCount && i < MAX_OPERATORS; i++) {
        String userKey = String(PREF_OPERATOR_PREFIX) + String(i) + "_user";
        String passKey = String(PREF_OPERATOR_PREFIX) + String(i) + "_pass";

        String username = prefs.getString(userKey.c_str(), "");
        String password = prefs.getString(passKey.c_str(), "");

        if (username.length() > 0 && password.length() > 0) {
            User op;
            op.username = username;
            op.password = password;
            op.role = OPERATOR;
            operators.push_back(op);
        }
    }

    prefs.end();

    Serial.printf("Loaded %d operator(s) from NVS\n", operators.size());
}

void UserManager::save() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, false)) {
        Serial.println("Failed to open user preferences for writing.");
        return;
    }

    // Save admin credentials
    prefs.putString(PREF_ADMIN_USER, adminUsername);
    prefs.putString(PREF_ADMIN_PASS, adminPassword);

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
    adminPassword = "admin";
    operators.clear();

    Serial.println("User credentials reset to factory defaults");
}

UserRole UserManager::authenticate(const String& username, const String& password) {
    // Check admin
    if (username == adminUsername && password == adminPassword) {
        Serial.println("User authenticated as ADMIN");
        return ADMIN;
    }

    // Check operators
    for (const auto& op : operators) {
        if (op.username == username && op.password == password) {
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
    if (password.length() < 4) {
        Serial.println("Cannot add operator: password must be at least 4 characters");
        return false;
    }

    // Check if username already exists
    if (usernameExists(username)) {
        Serial.println("Cannot add operator: username already exists");
        return false;
    }

    // Add operator
    User newOp;
    newOp.username = username;
    newOp.password = password;
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
    if (newPassword.length() < 4) {
        Serial.println("Cannot change password: new password must be at least 4 characters");
        return false;
    }

    // Check admin
    if (username == adminUsername) {
        if (adminPassword == oldPassword) {
            adminPassword = newPassword;
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
            if (op.password == oldPassword) {
                op.password = newPassword;
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
