/**
 * Unit tests for WiFi QR code string generation
 * Mirrors: data/script.js — generateWifiQRString logic
 *
 * WiFi QR format: WIFI:T:<enc>;S:<ssid>;P:<password>;;
 * For open networks (no password): WIFI:T:nopass;S:<ssid>;;
 *
 * Special characters in SSID/password must be escaped: \ ; , " :
 */

function escapeWifi(str) {
  return str.replace(/([\\;,":])/, '\\$1');
}

function generateWifiQRString(config) {
  const hasPassword = config.password && config.password.length > 0;
  const enc = hasPassword ? (config.encryption || 'WPA') : 'nopass';

  if (hasPassword) {
    return `WIFI:T:${enc};S:${escapeWifi(config.ssid)};P:${escapeWifi(config.password)};;`;
  } else {
    return `WIFI:T:nopass;S:${escapeWifi(config.ssid)};;`;
  }
}

describe('WiFi QR String Generation', () => {
  describe('password handling', () => {
    test('WPA network with password', () => {
      const result = generateWifiQRString({
        ssid: 'MyNetwork',
        password: 'mypassword',
        encryption: 'WPA',
      });
      expect(result).toBe('WIFI:T:WPA;S:MyNetwork;P:mypassword;;');
    });

    test('open network with empty password uses nopass', () => {
      const result = generateWifiQRString({
        ssid: 'OpenNet',
        password: '',
        encryption: 'WPA',
      });
      expect(result).toBe('WIFI:T:nopass;S:OpenNet;;');
    });

    test('open network with null password uses nopass', () => {
      const result = generateWifiQRString({
        ssid: 'OpenNet',
        password: null,
      });
      expect(result).toBe('WIFI:T:nopass;S:OpenNet;;');
    });

    test('open network with undefined password uses nopass', () => {
      const result = generateWifiQRString({
        ssid: 'OpenNet',
      });
      expect(result).toBe('WIFI:T:nopass;S:OpenNet;;');
    });

    test('open network omits password field entirely', () => {
      const result = generateWifiQRString({ ssid: 'Test', password: '' });
      expect(result).not.toContain(';P:');
    });
  });

  describe('encryption type', () => {
    test('defaults to WPA when encryption not specified', () => {
      const result = generateWifiQRString({
        ssid: 'Net',
        password: 'pass',
      });
      expect(result).toBe('WIFI:T:WPA;S:Net;P:pass;;');
    });

    test('uses WEP when specified', () => {
      const result = generateWifiQRString({
        ssid: 'Net',
        password: 'pass',
        encryption: 'WEP',
      });
      expect(result).toBe('WIFI:T:WEP;S:Net;P:pass;;');
    });

    test('ignores encryption type for open networks', () => {
      const result = generateWifiQRString({
        ssid: 'Net',
        password: '',
        encryption: 'WPA',
      });
      expect(result).toContain('T:nopass');
      expect(result).not.toContain('T:WPA');
    });
  });

  describe('special characters in SSID/password', () => {
    test('escapes semicolons in SSID', () => {
      const result = generateWifiQRString({
        ssid: 'My;Network',
        password: 'pass',
      });
      expect(result).toContain('S:My\\;Network');
    });

    test('escapes backslashes in password', () => {
      const result = generateWifiQRString({
        ssid: 'Net',
        password: 'pass\\word',
      });
      expect(result).toContain('P:pass\\\\word');
    });

    test('escapes colons in SSID', () => {
      const result = generateWifiQRString({
        ssid: 'My:Net',
        password: 'pass',
      });
      expect(result).toContain('S:My\\:Net');
    });
  });

  describe('real-world scenarios', () => {
    test('AP mode with typical SSID and password', () => {
      const result = generateWifiQRString({
        ssid: 'AP',
        password: 'northlandbadmintonwifinetwork',
        encryption: 'WPA',
      });
      expect(result).toBe('WIFI:T:WPA;S:AP;P:northlandbadmintonwifinetwork;;');
    });

    test('AP mode with empty password (the original bug)', () => {
      // This was the actual bug — empty password with WPA encryption
      // caused QR code to be unreadable on Android
      const result = generateWifiQRString({
        ssid: 'AP',
        password: '',
        encryption: 'WPA',
      });
      expect(result).toBe('WIFI:T:nopass;S:AP;;');
      expect(result).not.toContain('T:WPA');
    });
  });
});
