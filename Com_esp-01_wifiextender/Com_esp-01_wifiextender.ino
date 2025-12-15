// Board: Generic ESP8266 Module
// NAPT example released to public domain

#if LWIP_FEATURES && !LWIP_IPV6

#define HAVE_NETDUMP 0

// Customizable WiFi credentials with fallback support
#ifndef PRIMARY_SSID
#define PRIMARY_SSID "seeen"
#define PRIMARY_PASSWORD "qweasdzxc123"
#endif

#ifndef SECONDARY_SSID
#define SECONDARY_SSID "PONDOK AGNIA 2"
#define SECONDARY_PASSWORD "01191998"
#endif

#ifndef EXTENDER_SSID
#define EXTENDER_SSID "Dulham"
#endif

#ifndef EXTENDER_PASSWORD
#define EXTENDER_PASSWORD "qweasdzxc123"
#endif

#include <ESP8266WiFi.h>
#include <lwip/napt.h>
#include <lwip/dns.h>

#define NAPT 1000
#define NAPT_PORT 10
#define MAX_CONNECTION_ATTEMPTS 20  // 20 attempts * 500ms = 10 seconds per network
#define CONNECTION_DELAY 500

#if HAVE_NETDUMP

#include <NetDump.h>

void dump(int netif_idx, const char* data, size_t len, int out, int success) {
  (void)success;
  Serial.print(out ? F("out ") : F(" in "));
  Serial.printf("%d ", netif_idx);

  // optional filter example: if (netDump_is_ARP(data))
  {
    netDump(Serial, data, len);
    // netDumpHex(Serial, data, len);
  }
}
#endif

bool connectToWiFi(const char* ssid, const char* password, const char* networkName) {
  Serial.printf("Attempting to connect to %s: %s\n", networkName, ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < MAX_CONNECTION_ATTEMPTS) {
    Serial.print('.');
    delay(CONNECTION_DELAY);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nSuccessfully connected to %s\n", networkName);
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.printf("\nFailed to connect to %s after %d attempts\n", networkName, attempts);
    WiFi.disconnect();
    delay(1000);
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("\n\nCustom WiFi Range Extender with Fallback\n");
  Serial.printf("Heap on start: %d\n", ESP.getFreeHeap());

#if HAVE_NETDUMP
  phy_capture = dump;
#endif

  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);  // Don't save WiFi config to flash
  
  // Try to connect to primary WiFi first
  bool connected = connectToWiFi(PRIMARY_SSID, PRIMARY_PASSWORD, "Primary WiFi");
  
  // If primary WiFi fails, try secondary WiFi
  if (!connected) {
    Serial.println("Trying backup WiFi...");
    connected = connectToWiFi(SECONDARY_SSID, SECONDARY_PASSWORD, "Secondary WiFi");
  }
  
  // If still not connected, enter AP-only mode
  if (!connected) {
    Serial.println("Failed to connect to any WiFi network. Starting in AP-only mode...");
    // We'll continue anyway to provide AP access
  } else {
    Serial.printf("Connected! DNS: %s / %s\n", 
                  WiFi.dnsIP(0).toString().c_str(), 
                  WiFi.dnsIP(1).toString().c_str());
  }

  // Configure DHCP server
  auto& server = WiFi.softAPDhcpServer();
  if (connected) {
    // Use the DNS from the connected network
    server.setDns(WiFi.dnsIP(0));
  } else {
    // Use Google DNS as fallback (8.8.8.8)
    server.setDns(IPAddress(8, 8, 8, 8));
  }

  // Configure AP
  WiFi.softAPConfig(
    IPAddress(172, 217, 28, 254), 
    IPAddress(172, 217, 28, 254), 
    IPAddress(255, 255, 255, 0));
  
  WiFi.softAP(EXTENDER_SSID, EXTENDER_PASSWORD);
  Serial.printf("Extender AP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("AP SSID: %s\n", EXTENDER_SSID);

  // Only enable NAT if we're connected to a WiFi network
  if (connected) {
    Serial.printf("Heap before NAPT: %d\n", ESP.getFreeHeap());
    err_t ret = ip_napt_init(NAPT, NAPT_PORT);
    Serial.printf("ip_napt_init(%d,%d): ret=%d (OK=%d)\n", NAPT, NAPT_PORT, (int)ret, (int)ERR_OK);
    
    if (ret == ERR_OK) {
      ret = ip_napt_enable_no(SOFTAP_IF, 1);
      Serial.printf("ip_napt_enable_no(SOFTAP_IF): ret=%d (OK=%d)\n", (int)ret, (int)ERR_OK);
      
      if (ret == ERR_OK) { 
        Serial.printf("WiFi Network '%s' is now NATed behind connected network\n", EXTENDER_SSID); 
      }
    }
    
    Serial.printf("Heap after NAPT init: %d\n", ESP.getFreeHeap());
    if (ret != ERR_OK) { 
      Serial.printf("NAPT initialization failed\n"); 
    } else {
      Serial.println("Extender is fully operational with internet access!");
    }
  } else {
    Serial.println("Extender is in AP-only mode (no internet access)");
  }
  
  Serial.println("\n=== Extender Status ===");
  Serial.printf("Connected to STA: %s\n", connected ? "Yes" : "No");
  if (connected) {
    Serial.printf("STA IP: %s\n", WiFi.localIP().toString().c_str());
  }
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("AP Clients: %d\n", WiFi.softAPgetStationNum());
  Serial.println("======================\n");
}

#else

void setup() {
  Serial.begin(115200);
  Serial.printf("\n\nNAPT not supported in this configuration\n");
}

#endif

void loop() {
  // Optional: Monitor connection status and attempt reconnection if needed
  static unsigned long lastCheck = 0;
  const unsigned long CHECK_INTERVAL = 30000;  // Check every 30 seconds
  
  if (millis() - lastCheck > CHECK_INTERVAL) {
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Attempting to reconnect...");
      
      // Try both networks again
      bool connected = false;
      
      // Try primary first
      connected = connectToWiFi(PRIMARY_SSID, PRIMARY_PASSWORD, "Primary WiFi");
      
      // If primary fails, try secondary
      if (!connected) {
        connected = connectToWiFi(SECONDARY_SSID, SECONDARY_PASSWORD, "Secondary WiFi");
      }
      
      if (connected) {
        Serial.println("Reconnection successful!");
        // Re-enable NAT
        ip_napt_enable_no(SOFTAP_IF, 1);
      } else {
        Serial.println("Failed to reconnect to any network.");
      }
    } else {
      // Connection is still good
      static int counter = 0;
      if (++counter % 10 == 0) {  // Print status every 5 minutes (10 * 30 seconds)
        Serial.println("Extender is running normally.");
        Serial.printf("Connected clients: %d\n", WiFi.softAPgetStationNum());
      }
    }
  }
  
  // Small delay to prevent watchdog reset
  delay(100);
}