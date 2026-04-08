#pragma once
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <esp_wpa2.h>

// Touch calibration for CYD 2.4" (rotation 1)
#define TOUCH_X_MIN 304
#define TOUCH_X_MAX 3928
#define TOUCH_Y_MIN 243
#define TOUCH_Y_MAX 3909

extern TFT_eSPI tft;

// Capture last WiFi disconnect reason for diagnosing EAP failures
static volatile uint8_t g_wifi_last_reason = 0;
static void wifi_evt_handler(WiFiEvent_t e, WiFiEventInfo_t info) {
    if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        g_wifi_last_reason = info.wifi_sta_disconnected.reason;
    }
}

// On-screen keyboard layout
static const char* kb_rows[] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm.-_@"
};
static const int kb_row_count = 4;

struct WiFiSetup {
    String ssid;
    String username;
    String password;
    bool is_enterprise;
    bool connected;
    Preferences prefs;

    // Keyboard state
    int kb_y_start;
    int key_w;
    int key_h;
    bool shift_on;
    String input_text;
    int cursor_pos;
    bool editing_password;

    void init() {
        connected = false;
        is_enterprise = false;
        shift_on = false;
        kb_y_start = 116;
        key_w = 30;
        key_h = 24;
        cursor_pos = 0;
        editing_password = false;
    }

    // Map raw touch to screen coordinates (rotation 1: 320x240)
    // Uses getTouchRaw/getTouchRawZ (matches reference CYD project)
    bool getTouchPoint(int &tx, int &ty) {
        if (tft.getTouchRawZ() < 300) return false;  // Pressure threshold
        uint16_t rx, ry;
        tft.getTouchRaw(&rx, &ry);
        tx = map(rx, TOUCH_X_MIN, TOUCH_X_MAX, 0, 319);
        ty = map(ry, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 239);
        tx = constrain(tx, 0, 319);
        ty = constrain(ty, 0, 239);
        return true;
    }

    // Load saved credentials
    bool loadCredentials() {
        prefs.begin("wifi", true);
        ssid = prefs.getString("ssid", "");
        password = prefs.getString("pass", "");
        username = prefs.getString("user", "");
        is_enterprise = prefs.getBool("ent", false);
        prefs.end();
        return ssid.length() > 0;
    }

    // Save credentials
    void saveCredentials() {
        prefs.begin("wifi", false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", password);
        prefs.putString("user", username);
        prefs.putBool("ent", is_enterprise);
        prefs.end();
    }

    // Try connecting with saved credentials
    bool autoConnect(int timeout_sec = 10) {
        if (!loadCredentials()) return false;
        return connectWiFi(timeout_sec);
    }

    bool connectWiFi(int timeout_sec = 10) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(10, 10);
        tft.print("Connecting to: ");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.println(ssid);

        if (is_enterprise) {
            tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
            tft.setCursor(10, 28);
            tft.printf("EAP user: %s", username.c_str());
        }

        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        WiFi.onEvent(wifi_evt_handler);
        g_wifi_last_reason = 0;

        if (is_enterprise) {
            // PEAP/MSCHAPv2 (most common). Use username as both outer identity and inner.
            // No CA cert -> server not validated (acceptable for IoT device).
            esp_wifi_sta_wpa2_ent_clear_ca_cert();
            esp_wifi_sta_wpa2_ent_clear_cert_key();
            esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)username.c_str(), username.length());
            esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username.c_str(), username.length());
            esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password.c_str(), password.length());
            esp_wifi_sta_wpa2_ent_set_ttls_phase2_method(ESP_EAP_TTLS_PHASE2_MSCHAPV2);
            esp_wifi_sta_wpa2_ent_enable();
            // Enterprise auth often takes longer
            if (timeout_sec < 25) timeout_sec = 25;
            WiFi.begin(ssid.c_str());
        } else {
            WiFi.begin(ssid.c_str(), password.c_str());
        }

        int dots = 0;
        unsigned long start = millis();
        tft.setCursor(10, 40);
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > (unsigned long)timeout_sec * 1000) {
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.setCursor(10, 70);
                tft.println("Connection failed!");
                tft.setCursor(10, 90);
                tft.printf("status=%d reason=%u", WiFi.status(), g_wifi_last_reason);
                // Common reasons: 15=4WAY_HANDSHAKE_TIMEOUT, 204=HANDSHAKE_TIMEOUT,
                // 202=AUTH_FAIL (wrong user/pass), 205=GROUP_KEY_UPDATE_TIMEOUT
                delay(4000);
                return false;
            }
            tft.print(".");
            dots++;
            delay(500);
        }
        connected = true;
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 70);
        tft.print("Connected! IP: ");
        tft.println(WiFi.localIP().toString());
        delay(1500);
        return true;
    }

    // Scan and show WiFi networks, let user pick one
    int scanAndShow() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(10, 5);
        tft.println("Scanning WiFi...");

        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);

        int n = WiFi.scanNetworks();
        if (n == 0) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.println("No networks found!");
            delay(2000);
            return -1;
        }

        // Show up to 8 networks
        int maxShow = min(n, 8);
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(10, 5);
        tft.println("Select WiFi Network:");
        tft.drawFastHLine(0, 22, 320, TFT_DARKGREY);

        for (int i = 0; i < maxShow; i++) {
            int y = 26 + i * 26;
            bool ent = (WiFi.encryptionType(i) == WIFI_AUTH_WPA2_ENTERPRISE);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(10, y + 4);
            String name = WiFi.SSID(i);
            if (name.length() > 22) name = name.substring(0, 22) + "..";
            tft.print(name);

            // Enterprise badge
            if (ent) {
                tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
                tft.setTextFont(1);
                tft.setCursor(tft.getCursorX() + 3, y + 8);
                tft.print("EAP");
                tft.setTextFont(2);
            }

            // Signal strength indicator
            int rssi = WiFi.RSSI(i);
            uint16_t color = rssi > -50 ? TFT_GREEN : (rssi > -70 ? TFT_YELLOW : TFT_RED);
            tft.setTextColor(color, TFT_BLACK);
            tft.setCursor(280, y + 4);
            tft.printf("%ddB", rssi);

            if (i < maxShow - 1) {
                tft.drawFastHLine(5, y + 25, 310, TFT_DARKGREY);
            }
        }

        // Wait for touch selection
        while (true) {
            int tx, ty;
            if (getTouchPoint(tx, ty)) {
                for (int i = 0; i < maxShow; i++) {
                    int y = 26 + i * 26;
                    if (ty >= y && ty < y + 26) {
                        ssid = WiFi.SSID(i);
                        is_enterprise = (WiFi.encryptionType(i) == WIFI_AUTH_WPA2_ENTERPRISE);
                        WiFi.scanDelete();
                        return i;
                    }
                }
                delay(100);
            }
            delay(50);
        }
    }

    // Draw on-screen keyboard
    void drawKeyboard() {
        for (int row = 0; row < kb_row_count; row++) {
            const char* keys = kb_rows[row];
            int len = strlen(keys);
            int x_offset = (320 - len * key_w) / 2;
            int y = kb_y_start + row * key_h;

            for (int k = 0; k < len; k++) {
                int x = x_offset + k * key_w;
                tft.drawRect(x, y, key_w - 1, key_h - 1, TFT_DARKGREY);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextFont(2);
                char c = keys[k];
                if (shift_on && c >= 'a' && c <= 'z') c -= 32;
                tft.setCursor(x + key_w / 2 - 5, y + 5);
                tft.print(c);
            }
        }

        // Bottom row: SHIFT, SPACE, BKSP, OK
        int by = kb_y_start + kb_row_count * key_h;

        // SHIFT button
        tft.fillRoundRect(5, by, 50, key_h - 1, 3, shift_on ? TFT_BLUE : TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE);
        tft.setTextFont(1);
        tft.setCursor(12, by + 9);
        tft.print("SHIFT");

        // SPACE button
        tft.fillRoundRect(60, by, 110, key_h - 1, 3, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(95, by + 9);
        tft.print("SPACE");

        // BKSP button
        tft.fillRoundRect(175, by, 60, key_h - 1, 3, TFT_DARKGREY);
        tft.setTextColor(TFT_YELLOW);
        tft.setCursor(188, by + 9);
        tft.print("BKSP");

        // OK button
        tft.fillRoundRect(240, by, 75, key_h - 1, 3, TFT_GREEN);
        tft.setTextColor(TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(260, by + 5);
        tft.print("OK");
    }

    // Draw text input field
    void drawInputField() {
        tft.fillRect(10, 85, 300, 30, TFT_BLACK);
        tft.drawRect(10, 85, 300, 30, TFT_CYAN);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(15, 91);
        if (editing_password) {
            // Show last char, rest as *
            for (int i = 0; i < (int)input_text.length(); i++) {
                if (i == (int)input_text.length() - 1) {
                    tft.print(input_text[i]);
                } else {
                    tft.print('*');
                }
            }
        } else {
            tft.print(input_text);
        }
        // Cursor blink
        tft.setTextColor(TFT_CYAN);
        tft.print("_");
    }

    // Handle keyboard touch
    // Returns: 0=still editing, 1=OK pressed, -1=cancel
    int handleKeyboardTouch() {
        int tx, ty;
        if (!getTouchPoint(tx, ty)) return 0;

        // Check keyboard rows
        for (int row = 0; row < kb_row_count; row++) {
            const char* keys = kb_rows[row];
            int len = strlen(keys);
            int x_offset = (320 - len * key_w) / 2;
            int y = kb_y_start + row * key_h;

            if (ty >= y && ty < y + key_h) {
                for (int k = 0; k < len; k++) {
                    int x = x_offset + k * key_w;
                    if (tx >= x && tx < x + key_w) {
                        char c = keys[k];
                        if (shift_on && c >= 'a' && c <= 'z') c -= 32;
                        if (input_text.length() < 32) {
                            input_text += c;
                        }
                        drawInputField();
                        delay(200);
                        return 0;
                    }
                }
            }
        }

        // Check bottom row buttons
        int by = kb_y_start + kb_row_count * key_h;
        if (ty >= by && ty < by + key_h) {
            if (tx >= 5 && tx < 55) {
                // SHIFT
                shift_on = !shift_on;
                drawKeyboard();
                delay(200);
            } else if (tx >= 60 && tx < 170) {
                // SPACE
                if (input_text.length() < 32) input_text += ' ';
                drawInputField();
                delay(200);
            } else if (tx >= 175 && tx < 235) {
                // BKSP
                if (input_text.length() > 0) {
                    input_text.remove(input_text.length() - 1);
                }
                drawInputField();
                delay(200);
            } else if (tx >= 240 && tx < 315) {
                // OK
                delay(200);
                return 1;
            }
        }
        return 0;
    }

    // Password entry screen
    String getPasswordInput() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(10, 10);
        tft.print("WiFi: ");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.println(ssid);

        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(10, 40);
        tft.println("Enter Password:");

        // "No Password" button
        tft.fillRoundRect(10, 60, 120, 20, 3, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE);
        tft.setTextFont(1);
        tft.setCursor(20, 66);
        tft.print("No Password");

        input_text = "";
        editing_password = true;
        drawInputField();
        drawKeyboard();

        while (true) {
            int tx, ty;
            // Check "No Password" button
            if (getTouchPoint(tx, ty)) {
                if (ty >= 60 && ty < 80 && tx >= 10 && tx < 130) {
                    editing_password = false;
                    return "";
                }
            }

            int result = handleKeyboardTouch();
            if (result == 1) {
                editing_password = false;
                return input_text;
            }
            delay(30);
        }
    }

    // Username entry screen (for WPA2-Enterprise)
    String getUsernameInput() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(10, 10);
        tft.print("WiFi: ");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(ssid);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.setTextFont(1);
        tft.print(" [EAP]");
        tft.setTextFont(2);

        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(10, 40);
        tft.println("Enter Username:");

        // "@" hint for email-style usernames
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextFont(1);
        tft.setCursor(10, 60);
        tft.print("e.g. user@domain.com");
        tft.setTextFont(2);

        input_text = "";
        editing_password = false;
        drawInputField();
        drawKeyboard();

        while (true) {
            int result = handleKeyboardTouch();
            if (result == 1) {
                return input_text;
            }
            delay(30);
        }
    }

    // Full WiFi setup flow
    bool runSetup() {
        // First try auto-connect
        if (autoConnect(8)) return true;

        // Manual setup
        while (true) {
            int sel = scanAndShow();
            if (sel < 0) continue;

            // If Enterprise, ask for username first
            if (is_enterprise) {
                username = getUsernameInput();
            } else {
                username = "";
            }

            password = getPasswordInput();
            if (connectWiFi(15)) {
                saveCredentials();
                return true;
            }
            // Failed - try again
        }
    }
};
