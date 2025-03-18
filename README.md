# ESP8266 IP Notifier 

Ce projet consiste en un dispositif basé sur ESP8266 qui surveille l'adresse IP publique (WAN) de votre réseau et vous envoie une notification via Telegram lorsqu'elle change.


![image](https://github.com/user-attachments/assets/235a1641-03c9-402a-a53d-fa2b0072e3d6)  ![image](https://github.com/user-attachments/assets/cea98393-4ae1-40f4-8f75-a262341b2317)






## Fonctionnalités

- Vérification périodique de l'adresse IP publique
- Interface web de configuration accessible depuis n'importe quel navigateur
- Mode Point d'Accès (AP) pour la configuration initiale
- Notifications via Telegram lorsque l'IP change
- Persistance de la configuration dans la mémoire EEPROM
- Compatibilité avec les services HTTP et HTTPS pour la vérification d'IP
- Interface web pour vérifier l'état et reconfigurer le dispositif

## Matériel requis

- Carte ESP8266 (NodeMCU, Wemos D1 Mini, ou similaire)
- Alimentation USB
- Connexion WiFi

## Logiciels requis

- Arduino IDE
- Bibliothèques ESP8266
- Accès à un compte Telegram

## Installation

### 1. Configurer Arduino IDE

1. Installez la dernière version d'Arduino IDE
2. Ajoutez le support ESP8266 via le gestionnaire de cartes : 
   - Allez dans Fichier > Préférences
   - Dans "URL de gestionnaire de cartes supplémentaires", ajoutez : `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Allez dans Outils > Type de carte > Gestionnaire de cartes
   - Recherchez "esp8266" et installez la dernière version

### 2. Installer les bibliothèques requises

Installez les bibliothèques suivantes via le gestionnaire de bibliothèques (Croquis > Inclure une bibliothèque > Gérer les bibliothèques) :

- ESP8266WiFi
- ESP8266WebServer
- ESP8266HTTPClient
- DNSServer
- EEPROM

### 3. Télécharger et compiler le code

1. Clonez ce dépôt ou téléchargez le code source
2. Ouvrez le fichier .ino dans Arduino IDE
3. Sélectionnez votre carte ESP8266 dans Outils > Type de carte
4. Compilez et téléversez le code vers votre ESP8266

## Configuration de Telegram

Pour recevoir des notifications via Telegram, vous devez créer un bot et obtenir un token d'accès.

### Création d'un bot Telegram

1. Ouvrez Telegram et cherchez "@BotFather"
2. Démarrez une conversation avec BotFather et envoyez la commande `/newbot`
3. Suivez les instructions pour nommer votre bot
4. BotFather vous donnera un token d'accès (API token) qui ressemble à ceci : `123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghi`

### Obtenir votre Chat ID

1. Cherchez votre nouveau bot et commencez une conversation avec lui
2. Envoyez n'importe quel message à votre bot
3. Visitez l'URL suivante dans votre navigateur : `https://api.telegram.org/bot<VOTRE_TOKEN>/getUpdates` (remplacez `<VOTRE_TOKEN>` par le token obtenu précédemment)
4. Cherchez la valeur `"id"` dans la section `"chat"` de la réponse JSON - c'est votre Chat ID

### Format du token pour ce projet

Pour ce projet, le token doit être formaté comme suit:
```
bot<TOKEN>/sendMessage?chat_id=<CHAT_ID>&text=
```

Exemple:
```
bot123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghi/sendMessage?chat_id=987654321&text=
```

## Première utilisation

1. Allumez l'ESP8266. Au premier démarrage, il créera un point d'accès WiFi nommé "ESP8266-Config"
2. Connectez-vous à ce réseau WiFi depuis votre téléphone ou ordinateur
3. Ouvrez un navigateur et accédez à l'adresse `192.168.4.1`
4. Configurez les paramètres suivants :
   - SSID WiFi : le nom de votre réseau WiFi
   - Mot de passe WiFi : le mot de passe de votre réseau WiFi
   - URL pour vérifier l'IP : une URL comme `http://ifconfig.me` ou `https://checkip.amazonaws.com`
   - Hôte API de notification : `api.telegram.org`
   - Token d'accès API : le token formaté comme expliqué ci-dessus
5. Cliquez sur "Enregistrer"
6. L'ESP8266 redémarrera et se connectera à votre réseau WiFi
   
   ![image](https://github.com/user-attachments/assets/bb2f5433-ecea-4d1d-9aee-d42aaab702a2)


## Utilisation

Une fois configuré, l'ESP8266 vérifie périodiquement votre adresse IP publique (toutes les 5 minutes par défaut). Si l'adresse change, il envoie une notification via Telegram.

Pour accéder à l'interface web de gestion :
1. Assurez-vous que l'ESP8266 est allumé et connecté à votre réseau WiFi
2. Trouvez l'adresse IP locale de l'ESP8266 (visible dans l'interface de votre routeur ou via un scanner de réseau)
3. Ouvrez un navigateur et accédez à cette adresse IP

L'interface web vous permet de :
- Voir la dernière IP publique détectée
- Modifier la configuration
- Réinitialiser l'appareil

## Dépannage

### L'ESP8266 ne se connecte pas au WiFi
- Vérifiez que le SSID et le mot de passe sont corrects
- Assurez-vous que votre routeur est compatible avec les appareils ESP8266

### Les notifications Telegram ne fonctionnent pas
- Vérifiez que le format du token est correct
- Assurez-vous d'avoir démarré une conversation avec votre bot
- Vérifiez que l'ID de chat est correct

### L'adresse IP n'est pas détectée
- Essayez un autre service de vérification d'IP (comme `https://ifconfig.me/ip` ou `https://checkip.amazonaws.com`)
- Certains services peuvent être bloqués par votre FAI

### Réinitialisation de la configuration
Si vous devez réinitialiser la configuration :
1. Accédez à l'interface web et cliquez sur "Réinitialiser"
2. Ou ouvrez le moniteur série dans Arduino IDE, connectez-vous à l'ESP8266 et envoyez la commande `reset`

## Personnalisation

### Modification de l'intervalle de vérification
Par défaut, l'ESP8266 vérifie l'IP toutes les 5 minutes. Pour changer cet intervalle, modifiez la ligne suivante dans le code :
```cpp
const unsigned long checkInterval = 300000; // 5 minutes en millisecondes
```

### Changement du nom du point d'accès
Pour modifier le nom du point d'accès en mode configuration, modifiez la ligne suivante :
```cpp
WiFi.softAP("ESP8266-Config");
```

## Contribution

Les contributions à ce projet sont les bienvenues. N'hésitez pas à soumettre des problèmes (issues) ou des demandes de fusion (pull requests).

## Licence

Ce projet est distribué sous la licence MIT. Voir le fichier LICENSE pour plus d'informations.

## Auteur

[Votre Nom] - [Votre Email/GitHub]

## Remerciements

- Bibliothèque ESP8266 pour Arduino
- Communauté Arduino et ESP8266
- API Telegram pour la simplicité d'utilisationr
