# OSPF (OSPF Implementation) - Documentation Complète

## 📋 Vue d'ensemble

OSPF est une implémentation personnalisée du protocole de routage OSPF (Open Shortest Path First) développé en C++. Ce protocole permet la découverte automatique de voisins, l'échange d'informations de routage (LSA) et le calcul de routes optimales dans un réseau.

## ✨ Fonctionnalités

- **Découverte automatique de voisins** via messages Hello
- **Échange LSA (Link State Advertisements)** avec optimisations (compression, différentiel)
- **Calcul de routes** via l'algorithme Dijkstra
- **Application automatique des routes** au système Linux
- **Authentification HMAC** pour la sécurité
- **Interface CLI** pour la gestion et le monitoring
- **Support multi-interfaces** réseau

## 🛠️ Prérequis et Installation

### Dépendances Système

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake git

# Packages additionnels
sudo apt install -y libssl-dev  # Pour HMAC
sudo apt install -y iproute2    # Pour ip route (normalement déjà installé)
```

### Dépendances C++

Le projet utilise **nlohmann/json** qui est inclus dans le répertoire `include/`.

### Compilation

```bash
# Cloner le projet
git clone <repository-url>
cd OSPF

# Compiler
make clean
make

# Ou avec CMake (si disponible)
mkdir build && cd build
cmake ..
make
```

## 📁 Structure du Projet

```
OSPF/
├── config/
│   ├── router.conf           #Informations du routeur
├── src/
│   ├── main.cpp              # Point d'entrée et CLI
│   ├── RoutingDaemon.cpp     # Daemon principal
│   ├── PacketManager.cpp     # Gestion des paquets UDP
│   ├── LinkStateManager.cpp  # Gestion des voisins
│   ├── TopologyDatabase.hpp  # Base de données LSA et Dijkstra
│   └── RoutingTable.hpp      # Structure de la table de routage
├── include/
│   └── json.hpp              # Bibliothèque JSON
├── Makefile
└── README.md
```

## 🔧 Configuration

### Configuration Réseau

Le programme détecte automatiquement les interfaces réseau, mais vous devez configurer votre topologie réseau manuellement.

#### Exemple de Topologie

```
R_1 (192.168.1.1) ←→ R_2 (192.168.2.1) ←→ R_5 (192.168.5.1)
 ↓                     ↓                     ↓
R_4 (192.168.4.1) ←→ R_2 (10.2.0.2)    ←→ R_3 (192.168.3.1)
```


### Création du fichier config/router.conf
Mettez les informations du routeur dans ce fichier
```bash
[R]
hostname=R_1
interfaces=192.168.1.1,10.1.0.1
interfacesNames=enp0s9,enp0s8
port=5000 
```
### Configuration Firewall

Autoriser le trafic UDP sur le port 5000 :

```bash
# UFW
sudo ufw allow 5000/udp

# iptables
sudo iptables -A INPUT -p udp --dport 5000 -j ACCEPT
sudo iptables -A OUTPUT -p udp --sport 5000 -j ACCEPT
```

## 🚀 Utilisation

### Lancement du Programme

```bash
# Lancer avec les privilèges root (nécessaire pour modifier les routes)
sudo ./routing

# Interface CLI
Routing Protocol CLI
Type 'help' for available commands
routing> 
```

### Commandes CLI Disponibles

```bash
# Démarrer le daemon
routing> start

# Arrêter le daemon  
routing> stop

# Afficher le statut
routing> status

# Afficher les routes actuelles
routing> routes

# Afficher les métriques de routage
routing> metrics

# Afficher la topologie découverte
routing> topology

# Demander la liste de voisins à un routeur
routing> request_neighbors <IP>

# Quitter
routing> quit
```

### Exemple de Session Complète

```bash
sudo ./routing
routing> start
Daemon started successfully

routing> status
Daemon Status: Running
Hostname: R_1
Port: 5000
Interfaces: 192.168.1.1 10.1.0.1
Active Neighbors (1): 10.1.0.2

routing> routes
=== Current Routing Table ===
Router: R_1
----------------------------------------
Destination         Next Hop       Interface      Metric    
----------------------------------------
192.168.2.0/24      R_2            enp0s8         1         
10.2.0.0/24         R_2            enp0s8         1         
192.168.3.0/24      R_2            enp0s8         1         
=================================
Total routes: 3
```

## 🔒 Sécurité

### Authentification HMAC

Tous les paquets sont authentifiés avec HMAC-SHA256. La clé par défaut est hardcodée :

```cpp
// Dans PacketManager.cpp
std::string hmacKey = "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV";
```

**⚠️ Important:** Changez cette clé en production !

### Permissions

Le programme nécessite les privilèges root pour :
- Modifier les routes système (`ip route`)
- Écouter sur les ports réseau
- Accéder aux interfaces réseau

## 📊 Monitoring et Debug

### Logs Système

Les routes appliquées sont visibles dans les logs système :

```bash
# Voir les routes ajoutées
sudo journalctl -f | grep "ip route"

# Vérifier les routes système
ip route show
```

### Vérification de Connectivité

```bash
# Tester la connectivité vers un réseau distant
ping -c 3 192.168.2.1

# Tracer les routes
traceroute 192.168.3.1

# Vérifier les interfaces
ip addr show
```

### Debug du Protocole

Pour activer les logs de debug (en développement), modifiez les fichiers source et recompilez.

## 🔧 Dépannage

### Problèmes Courants

**1. Pas de voisins découverts**
```bash
# Vérifier la connectivité réseau
ping -c 3 <IP_voisin>

# Vérifier le firewall
sudo ufw status
```

**2. Routes non appliquées**
```bash
# Vérifier les privilèges
whoami  # doit être root

# Vérifier les interfaces
ip addr show
```

**3. Authentification HMAC échoue**
- Vérifier que tous les routeurs utilisent la même clé HMAC
- Vérifier l'heure système (décalage trop important peut poser problème)

**4. Performance**
```bash
# Vérifier l'utilisation CPU
top -p $(pgrep routing)

# Vérifier les paquets réseau
sudo tcpdump -i any port 5000
```

## 📈 Optimisation

### Paramètres Configurables

Dans `RoutingDaemon.cpp`, vous pouvez ajuster :

```cpp
// Intervalle d'envoi des Hello (millisecondes)
std::this_thread::sleep_for(std::chrono::milliseconds(2000));

// Intervalle de calcul des routes (millisecondes)  
int sleepTime = 5000;

// Timeout pour les voisins inactifs
// Dans LinkStateManager.cpp
auto timeout = std::chrono::seconds(10);
```

### Optimisations LSA

Le système utilise automatiquement :
- **LSA_FULL_COMPRESSED** : Premier envoi avec compression
- **LSA_DIFFERENTIAL** : Envois suivants avec seulement les changements

## 🧪 Tests

### Test de Base

```bash
# Lancer 2 routeurs sur des machines différentes
# Machine 1 (R_1)
sudo hostname R_1
sudo ./routing
routing> start

# Machine 2 (R_2)  
sudo hostname R_2
sudo ./routing
routing> start

# Vérifier la découverte mutuelle
routing> status
```

### Test de Convergence

```bash
# Arrêter un routeur intermédiaire
routing> stop

# Vérifier que les routes sont recalculées sur les autres
# Sur les autres routeurs
routing> routes
```

## 📝 Fichiers de Configuration

Le programme ne nécessite pas de fichiers de configuration externes. Toute la configuration se fait via :

1. **Configuration réseau système** (interfaces, IPs)
2. **Hostname système** 
3. **Paramètres compilés** dans le code source

Pour une configuration plus avancée, modifiez les constantes dans le code source et recompilez.

## 🚨 Limitations Connues

- **Clé HMAC hardcodée** (à changer en production)
- **Port fixe** (5000) 
- **Pas de support IPv6**
- **Pas de zones OSPF** (single area seulement)
- **Pas de persistence** des routes après redémarrage

## 📞 Support

Pour les problèmes et questions :
1. Vérifiez les logs système
2. Testez la connectivité réseau de base
3. Vérifiez les permissions et privilèges
4. Consultez la section dépannage

---

**Version:** 1.0  
**Dernière mise à jour:** Décembre 2024