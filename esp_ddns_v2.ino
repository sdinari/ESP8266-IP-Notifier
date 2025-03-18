#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>

// Prototypes des fonctions
String urlEncode(String str);
bool isAlphaNumeric(char c);

// Structure pour stocker les paramètres de configuration
struct Config {
  char ssid[32];
  char password[64];
  char apiHost[64];
  char accessToken[128]; // Agrandir pour accommoder les tokens plus longs
  char checkIPUrl[64];
  bool configured;
};

Config config;
String previousIP = ""; // Variable pour stocker l'adresse IP précédente

// Serveur web sur le port 80
ESP8266WebServer server(80);

// Serveur DNS pour le portail captif
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Mode de fonctionnement
bool configMode = false;
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 300000; // 5 minutes en millisecondes

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nDémarrage de l'ESP8266...");
  
  // Initialiser l'EEPROM
  EEPROM.begin(512);
  
  // Charger la configuration depuis l'EEPROM
  loadConfig();
  
  // Vérifier si la configuration existe
  if (!config.configured) {
    Serial.println("Configuration non trouvée, démarrage en mode configuration");
    startConfigMode();
  } else {
    Serial.println("Configuration trouvée, démarrage en mode normal");
    startNormalMode();
  }
}

void loop() {
  if (configMode) {
    // Gérer les requêtes DNS en mode configuration
    dnsServer.processNextRequest();
    // Gérer les requêtes web
    server.handleClient();
  } else {
    // Mode normal - vérifier l'adresse IP périodiquement
    unsigned long currentMillis = millis();
    if (currentMillis - lastCheckTime >= checkInterval) {
      lastCheckTime = currentMillis;
      checkAndSendIP();
    }
    
    // Vérifier si le bouton de reset est pressé (simulé ici par une commande série)
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      if (command == "reset") {
        Serial.println("Réinitialisation de la configuration");
        resetConfig();
      }
    }
    
    // Aussi gérer les requêtes web en mode normal pour permettre la reconfiguration
    server.handleClient();
  }
}

void startConfigMode() {
  configMode = true;
  
  // Créer un point d'accès
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP8266-Config");
  Serial.print("Point d'accès créé. IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Configuration du serveur DNS pour rediriger toutes les requêtes vers l'ESP
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // Configuration des routes du serveur web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot);
  
  // Démarrer le serveur web
  server.begin();
  Serial.println("Serveur web démarré");
}

void startNormalMode() {
  configMode = false;
  
  // Se connecter au WiFi avec les paramètres enregistrés
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  
  Serial.print("Connexion à ");
  Serial.print(config.ssid);
  
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnecté au WiFi");
    Serial.print("Adresse IP locale: ");
    Serial.println(WiFi.localIP());
    
    // Configuration des routes du serveur web pour le mode normal
    server.on("/", HTTP_GET, handleStatus);
    server.on("/config", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/reset", HTTP_GET, []() {
      server.send(200, "text/html", "<html><body><h1>Reset de la configuration</h1><p>La configuration a été réinitialisée. Redémarrez l'appareil.</p></body></html>");
      resetConfig();
    });
    
    // Démarrer le serveur web
    server.begin();
    
    // Vérifier l'IP immédiatement au démarrage
    checkAndSendIP();
  } else {
    Serial.println("\nÉchec de connexion WiFi. Passage en mode configuration.");
    startConfigMode();
  }
}

void checkAndSendIP() {
  if (WiFi.status() == WL_CONNECTED) {
    String wanIP = getWANIPAddress();
    
    if (wanIP != "") {
      if (wanIP != previousIP) {
        Serial.println("Changement détecté de l'IP : " + wanIP);
        sendPushbulletNotificationWithIP(wanIP);
        previousIP = wanIP;
      } else {
        Serial.println("Pas de changement d'IP: " + wanIP);
      }
    } else {
      Serial.println("Impossible de récupérer l'adresse IP WAN");
    }
  }
}

String getWANIPAddress() {
  if (WiFi.status() == WL_CONNECTED) {
    String url = String(config.checkIPUrl);
    bool isHttps = url.startsWith("https://");
    
    // Extraire le domaine et le chemin de l'URL
    String urlWithoutProtocol = url;
    
    // Supprimer le protocole de l'URL
    if (url.startsWith("http://")) {
      urlWithoutProtocol = url.substring(7);
    } else if (url.startsWith("https://")) {
      urlWithoutProtocol = url.substring(8);
    }
    
    if (isHttps) {
      // Pour les requêtes HTTPS
      WiFiClientSecure httpsClient;
      httpsClient.setInsecure(); // Désactiver la vérification des certificats
      
      // Extraire le domaine (tout ce qui précède le premier '/' ou '?')
      int separatorIndex = urlWithoutProtocol.indexOf('/');
      int queryIndex = urlWithoutProtocol.indexOf('?');
      
      // Trouver le premier séparateur (/ ou ?)
      if (queryIndex != -1 && (separatorIndex == -1 || queryIndex < separatorIndex)) {
        separatorIndex = queryIndex;
      }
      
      String host = (separatorIndex == -1) ? urlWithoutProtocol : urlWithoutProtocol.substring(0, separatorIndex);
      String pathAndQuery = (separatorIndex == -1) ? "/" : urlWithoutProtocol.substring(separatorIndex);
      
      Serial.println("Connexion HTTPS à: " + host);
      Serial.println("Chemin et paramètres: " + pathAndQuery);
      
      if (httpsClient.connect(host.c_str(), 443)) {
        // Envoyer la requête HTTP
        httpsClient.print(String("GET ") + pathAndQuery + " HTTP/1.1\r\n" +
                         "Host: " + host + "\r\n" +
                         "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n" +
                         "Connection: close\r\n\r\n");
        
        Serial.println("Requête HTTPS envoyée");
        
        // Lire la réponse HTTP complète
        String fullResponse = "";
        unsigned long timeout = millis();
        
        while ((httpsClient.connected() || httpsClient.available()) && millis() - timeout < 10000) {
          if (httpsClient.available()) {
            char c = httpsClient.read();
            fullResponse += c;
          }
        }
        
        httpsClient.stop();
        
        // Trouver la séparation entre les en-têtes et le corps (double ligne vide)
        int bodyStart = fullResponse.indexOf("\r\n\r\n");
        if (bodyStart != -1) {
          // Extraire uniquement le corps (en sautant les 4 caractères \r\n\r\n)
          String bodyOnly = fullResponse.substring(bodyStart + 4);
          bodyOnly.trim();
          
          Serial.println("En-têtes HTTPS: " + fullResponse.substring(0, bodyStart));
          Serial.println("Corps de la réponse HTTPS: [" + bodyOnly + "]");
          
          return bodyOnly;
        } else {
          Serial.println("Impossible de trouver la séparation entre en-têtes et corps");
          Serial.println("Réponse complète: " + fullResponse);
          return "";
        }
      } else {
        Serial.println("Échec de connexion HTTPS à " + host);
        httpsClient.stop();
      }
    } else {
      // Pour les requêtes HTTP standard
      WiFiClient httpClient;
      
      // Se connecter directement pour avoir un contrôle total sur le parsing
      int port = 80;
      int separatorIndex = urlWithoutProtocol.indexOf('/');
      String host = (separatorIndex == -1) ? urlWithoutProtocol : urlWithoutProtocol.substring(0, separatorIndex);
      String path = (separatorIndex == -1) ? "/" : urlWithoutProtocol.substring(separatorIndex);
      
      Serial.println("Connexion HTTP à: " + host);
      
      if (httpClient.connect(host.c_str(), port)) {
        // Envoyer la requête HTTP
        httpClient.print(String("GET ") + path + " HTTP/1.1\r\n" +
                        "Host: " + host + "\r\n" +
                        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n" +
                        "Connection: close\r\n\r\n");
                        
        Serial.println("Requête HTTP envoyée");
        
        // Lire la réponse HTTP complète
        String fullResponse = "";
        unsigned long timeout = millis();
        
        while ((httpClient.connected() || httpClient.available()) && millis() - timeout < 10000) {
          if (httpClient.available()) {
            char c = httpClient.read();
            fullResponse += c;
          }
        }
        
        httpClient.stop();
        
        // Trouver la séparation entre les en-têtes et le corps (double ligne vide)
        int bodyStart = fullResponse.indexOf("\r\n\r\n");
        if (bodyStart != -1) {
          // Extraire uniquement le corps (en sautant les 4 caractères \r\n\r\n)
          String bodyOnly = fullResponse.substring(bodyStart + 4);
          bodyOnly.trim();
          
          Serial.println("En-têtes HTTP: " + fullResponse.substring(0, bodyStart));
          Serial.println("Corps de la réponse HTTP: [" + bodyOnly + "]");
          
          return bodyOnly;
        } else {
          Serial.println("Impossible de trouver la séparation entre en-têtes et corps");
          Serial.println("Réponse complète: " + fullResponse);
          return "";
        }
      } else {
        Serial.println("Échec de connexion HTTP à " + host);
        httpClient.stop();
      }
    }
  }
  return "";
}

void sendPushbulletNotificationWithIP(String ipAddress) {
  WiFiClientSecure httpsClient;
  httpsClient.setInsecure(); // Désactiver la vérification des certificats
  
  // Modifier le format de la requête pour Telegram
  if (httpsClient.connect(config.apiHost, 443)) {
    // Construire l'URL avec le texte du message
    String messageText = "Votre IP WAN est : " + ipAddress;
    String urlEncodedMessage = urlEncode(messageText);
    String url = String(config.accessToken) + urlEncodedMessage;
    
    // Envoi de la requête GET (pour Telegram)
    httpsClient.print(String("GET /") + url + " HTTP/1.1\r\n" +
                     "Host: " + config.apiHost + "\r\n" +
                     "Connection: close\r\n\r\n");
    
    Serial.println("Notification API envoyée");
    
    // Attendre une réponse
    unsigned long timeout = millis();
    while (httpsClient.connected() && millis() - timeout < 10000) {
      if (httpsClient.available()) {
        String line = httpsClient.readStringUntil('\n');
        if (line == "\r") {
          Serial.println("Headers reçus");
          break;
        }
      }
    }
    
    // Lire le reste de la réponse
    String responseBody = "";
    while (httpsClient.available()) {
      char c = httpsClient.read();
      responseBody += c;
    }
    
    Serial.println("Réponse: " + responseBody);
  } else {
    Serial.println("Échec de connexion à l'API");
  }
  
  httpsClient.stop();
}

// Fonction pour charger la configuration depuis l'EEPROM
void loadConfig() {
  EEPROM.get(0, config);
  
  // Vérifier si la configuration est valide
  if (config.configured != true) {
    // Initialiser avec des valeurs par défaut
    strcpy(config.ssid, "");
    strcpy(config.password, "");
    strcpy(config.apiHost, "api.telegram.org");  // Valeur par défaut pour Telegram
    strcpy(config.accessToken, "");
    strcpy(config.checkIPUrl, "http://ifconfig.me");
    config.configured = false;
  }
}

// Fonction pour sauvegarder la configuration dans l'EEPROM
void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

// Fonction pour réinitialiser la configuration
void resetConfig() {
  config.configured = false;
  saveConfig();
  delay(1000);
  ESP.restart();
}

// Gestionnaire pour la page racine
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuration ESP8266</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += ".container { max-width: 500px; margin: 0 auto; }";
  html += "label { display: block; margin-top: 10px; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 8px; margin-top: 5px; }";
  html += ".password-container { position: relative; }";
  html += ".toggle-password { position: absolute; right: 10px; top: 14px; cursor: pointer; }";
  html += "button { background-color: #4CAF50; color: white; padding: 10px 15px; margin-top: 20px; border: none; cursor: pointer; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Configuration ESP8266</h1>";
  html += "<form action='/save' method='POST'>";
  
  // Afficher des champs vides pour la première configuration
  String ssidValue = config.configured ? String(config.ssid) : "";
  String passwordValue = config.configured ? String(config.password) : "";
  String checkIPUrlValue = config.configured ? String(config.checkIPUrl) : "http://ifconfig.me";
  String apiHostValue = config.configured ? String(config.apiHost) : "api.telegram.org";
  String accessTokenValue = config.configured ? String(config.accessToken) : "";
  
  html += "<label for='ssid'>SSID WiFi:</label>";
  html += "<input type='text' id='ssid' name='ssid' value='" + ssidValue + "'>";
  
  html += "<label for='password'>Mot de passe WiFi:</label>";
  html += "<div class='password-container'>";
  html += "<input type='password' id='password' name='password' value='" + passwordValue + "'>";
  html += "<span class='toggle-password' onclick='togglePassword()'>👁️</span>";
  html += "</div>";
  
  html += "<label for='checkIPUrl'>URL pour vérifier l'IP:</label>";
  html += "<input type='text' id='checkIPUrl' name='checkIPUrl' value='" + checkIPUrlValue + "'>";
  
  html += "<label for='apiHost'>Hôte API de notification:</label>";
  html += "<input type='text' id='apiHost' name='apiHost' value='" + apiHostValue + "'>";
  
  html += "<label for='accessToken'>Token d'accès API:</label>";
  html += "<input type='text' id='accessToken' name='accessToken' value='" + accessTokenValue + "'>";
  
  html += "<button type='submit'>Enregistrer</button>";
  html += "</form>";
  html += "<script>";
  html += "function togglePassword() {";
  html += "  var passwordField = document.getElementById('password');";
  html += "  if (passwordField.type === 'password') {";
  html += "    passwordField.type = 'text';";
  html += "  } else {";
  html += "    passwordField.type = 'password';";
  html += "  }";
  html += "}";
  html += "</script>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// Gestionnaire pour la page d'état
void handleStatus() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>État ESP8266</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += ".container { max-width: 500px; margin: 0 auto; }";
  html += ".info { margin: 10px 0; }";
  html += "button { background-color: #4CAF50; color: white; padding: 10px 15px; margin-top: 20px; border: none; cursor: pointer; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>État ESP8266</h1>";
  
  html += "<div class='info'><strong>WiFi:</strong> " + String(config.ssid) + "</div>";
  html += "<div class='info'><strong>IP locale:</strong> " + WiFi.localIP().toString() + "</div>";
  html += "<div class='info'><strong>Dernière IP WAN:</strong> " + previousIP + "</div>";
  html += "<div class='info'><strong>URL vérification IP:</strong> " + String(config.checkIPUrl) + "</div>";
  html += "<div class='info'><strong>Hôte API:</strong> " + String(config.apiHost) + "</div>";
  
  html += "<button onclick=\"window.location.href='/config'\">Configuration</button> ";
  html += "<button onclick=\"if(confirm('Êtes-vous sûr de vouloir réinitialiser?')) window.location.href='/reset'\">Réinitialiser</button> ";
  html += "<button onclick=\"window.location.reload()\">Rafraîchir</button>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// Gestionnaire pour sauvegarder les paramètres
void handleSave() {
  // Effacer les valeurs existantes avant d'enregistrer les nouvelles
  memset(config.ssid, 0, sizeof(config.ssid));
  memset(config.password, 0, sizeof(config.password));
  memset(config.checkIPUrl, 0, sizeof(config.checkIPUrl));
  memset(config.apiHost, 0, sizeof(config.apiHost));
  memset(config.accessToken, 0, sizeof(config.accessToken));
  
  // Copier les nouvelles valeurs
  if (server.hasArg("ssid")) strncpy(config.ssid, server.arg("ssid").c_str(), sizeof(config.ssid) - 1);
  if (server.hasArg("password")) strncpy(config.password, server.arg("password").c_str(), sizeof(config.password) - 1);
  if (server.hasArg("checkIPUrl")) strncpy(config.checkIPUrl, server.arg("checkIPUrl").c_str(), sizeof(config.checkIPUrl) - 1);
  if (server.hasArg("apiHost")) strncpy(config.apiHost, server.arg("apiHost").c_str(), sizeof(config.apiHost) - 1);
  if (server.hasArg("accessToken")) strncpy(config.accessToken, server.arg("accessToken").c_str(), sizeof(config.accessToken) - 1);
  
  config.configured = true;
  saveConfig();
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuration enregistrée</title>";
  html += "<style>body { font-family: Arial, sans-serif; margin: 20px; } .container { max-width: 500px; margin: 0 auto; }</style>";
  html += "</head><body><div class='container'>";
  html += "<h1>Configuration enregistrée</h1>";
  html += "<p>Les paramètres ont été enregistrés. L'appareil va redémarrer dans 5 secondes.</p>";
  html += "<script>setTimeout(function(){ window.location.href='/'; }, 5000);</script>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
  
  // Redémarrer l'ESP après un délai
  delay(1000);
  ESP.restart();
}

// Fonction pour encoder les caractères spéciaux dans les URL
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isAlphaNumeric(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// Fonction pour vérifier si un caractère est alphanumérique
bool isAlphaNumeric(char c) {
  return (c >= '0' && c <= '9') || 
         (c >= 'A' && c <= 'Z') || 
         (c >= 'a' && c <= 'z');
}