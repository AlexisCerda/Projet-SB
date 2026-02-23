# Projet SB - Station de Décontamination USB

Ce projet est une solution de sécurité automatisée conçue pour détecter, scanner et nettoyer les périphériques USB dès leur insertion. Il combine un détecteur bas niveau en C, un moteur de mise à jour pour antivirus (ClamAV & Trellix), et un système de notification par email.

##  Fonctionnalités

*   **Détection automatique** : Surveillance des ports USB via `libusb` pour détecter l'insertion de périphériques de stockage.
*   **Montage & Scan** : Montage automatique du périphérique et analyse antivirale.
*   **Mise en quarantaine** : Déplacement automatique des fichiers infectés vers un dossier sécurisé.
*   **Mise à jour intelligente** : Script Bash (`MAJ_Trellix.bash`) pour maintenir à jour les définitions de virus ClamAV et Trellix (McAfee).
*   **Notifications** : Envoi d'alertes par email (via Java) contenant les détails de l'infection et le nom de la machine hôte.
*   **Configuration flexible** : Fichier `Config.ini` pour gérer les paramètres sans recompiler.

##  Prérequis

Pour compiler et exécuter ce projet, vous avez besoin de :

*   **Système OS** : Linux (Debian/Ubuntu recommandé).
*   **Compilateur C** : `gcc`.
*   **Librairies** : `libusb-1.0-0-dev`.
*   **Java** : JRE installé (pour le module `mail.jar`).
*   **Antivirus** : ClamAV et/ou Trellix UVSCAN installés.
*   **Utilitaires** : `wget`, `unzip` (pour le script de mise à jour).

##  Installation & Compilation

1.  **Cloner le dépôt** :
    ```bash
    git clone https://github.com/AlexisCerda/Projet-SB.git
    cd Projet-SB
    ```

2.  **Installer les dépendances** :
    ```bash
    sudo apt update
    sudo apt install build-essential libusb-1.0-0-dev default-jre clamav wget unzip
    ```

3.  **Compiler le détecteur** :
    ```bash
    gcc -o VirusDetector VirusDetector.c ini.c -lusb-1.0
    ```

4.  **Préparer l'environnement** :
    Assurez-vous que les dossiers de montage et de quarantaine existent (ou modifiez `Config.ini`) :
    ```bash
    sudo mkdir -p /mnt/analyse_usb
    sudo mkdir -p /quarantine
    ```

##  Configuration

Modifiez le fichier `Config.ini` pour adapter le comportement à votre infrastructure :

```ini
[Mail]
sender=votre_email@gmail.com
password=votre_mot_de_passe_app
smtp_server=smtp.gmail.com
smtp_port=587
...
```

##  Utilisation

1.  **Lancer le détecteur** (nécessite les droits root pour l'accès USB et le montage) :
    ```bash
    sudo ./VirusDetector
    ```

2.  **Mettre à jour les antivirus** :
    Exécutez le script de mise à jour périodiquement (via cron par exemple) :
    ```bash
    sudo ./MAJ_Trellix.bash
    ```

##  Architecture

*   `VirusDetector.c` : Programme principal (C + libusb).
*   `MAJ_Trellix.bash` : Script de mise à jour des bases virales.
*   `mail.jar` : Utilitaire d'envoi de mails.
*   `ini.c` / `ini.h` : Parser de fichier de configuration.

##  Tests

Pour tester si l'antivirus marche correctement utilisez eicar

##  Explication des fonctionnalités

Voici quelques explications et rappels sur les méthodes utilisées durant ce projet.

### Démarrage automatique du programme (systemd)

**Fichier de service** : `/etc/systemd/system/virusdetector.service`

**Étapes d'installation** :

1. Recharger les configurations de Systemd
   ```bash
   sudo systemctl daemon-reload
   ```

2. Activer le lancement automatique au démarrage du PC
   ```bash
   sudo systemctl enable virusdetector.service
   ```

3. Démarrer le programme immédiatement
   ```bash
   sudo systemctl start virusdetector.service
   ```

**Fichier de service** :

```ini
[Unit]
Description=Service de détection de virus sur clé USB
After=network.target

[Service]
# Dossier de travail
WorkingDirectory=/opt/VirusDetector/
# Le chemin exact vers ton exécutable
ExecStart=/opt/VirusDetector/VirusDetector
# Redémarrer automatiquement en cas de crash
Restart=always
# Attendre 3 secondes avant de redémarrer pour éviter de surcharger le système en cas d'erreur en boucle
RestartSec=3
# Lancer avec les droits root (nécessaire pour libusb et mount)
User=root
# Lancer automatiquement au démarrage de l'ordinateur
WantedBy=multi-user.target
```

**Vérification du service** :
```bash
sudo systemctl status virusdetector.service
```

### Configuration Cron

Ajouter dans le crontab (via `crontab -e`) :
```bash
0 0 * * * /opt/VirusDetector/MAJ_Trellix.bash >> /var/log/maj_clamavtrellix.log 2>&1
```

Cela exécutera la commande tous les 24 heures à minuit.

**Format du cron** : `mm hh jj MMM JJJ tâche > log`

### Mise à jour des scanner antivirus

**ClamAV** :
```bash
freshclam
```

**Trellix** :
```bash
cd /tmp
wget http://update.nai.com/products/commonupdater/avvdat-9999.zip -O avvdat.zip
unzip -o avvdat.zip
sudo mv *.dat /usr/local/uvscan/
rm avvdat.zip
/usr/local/uvscan/uvscan --version
```

### Scan avec Trellix

```bash
uvscan --secure -r --summary --move <quarantine_dir> <target_dir>
```

**Options disponibles** :
- `--secure` : combine `--analyse` et `--unzip`
- `-r` : récursif, analyse tous les dossiers et sous-dossiers
- `--analyse` : analyse heuristique (au-delà des signatures)
- `--unzip` : décompresse les fichiers compressés pour analyse
- `--move` : déplace les virus détectés dans le répertoire de quarantaine
- `--summary` : affiche un résumé de l'analyse à la fin

**Documentation** : cls704upg.pdf

### Envoi de mail avec Java

- Un script Java utilise la librairie Jakarta pour envoyer des mails
- Le script prend en paramètre :
  - Les informations de connexion
  - Les informations du mail (destinataire, sujet, corps du message)
  - Tous les paramètres pour configurer le serveur SMTP

### Récupération du nom de l'ordinateur

```c
gethostname(hostname_array, size);
```

- Prend en paramètre un tableau de caractères et sa taille
- Remplit le tableau avec le nom de la machine

### Ouverture d'un fichier INI en C

- Utilise un handler reliant les sections et clés du fichier INI à la structure de configuration
- Utilise la fonction `ini_parse()` :
  ```c
  ini_parse(filename, handler, &config_struct);
  ```
- Paramètres : nom du fichier, handler, pointeur vers la structure de configuration
### Model de Config.ini

    mail_sender = mon@mail
    pwd_app_sender = "motdepassedapplication"
    mail_smtp_server = "smtp.gmail.com" (pour gmail)
    mail_smtp_port = 587 (pour gmail)
    mail_smtp_auth = "true"
    mail_smtp_starttls_enable = "true"
    mail_recever = mail@admin, mail@admin2
    Nb_mail_recever = 2
    mail_subject = "sujet"
    mail_body = "cotenu
    path_quarantine = "path/dossier de quarantaine"
    path_mount_point = "path/dossier de montage"
    Buffer_size = taille des buffer (1024)

### Scan avec ClamAV

```bash
clamdscan [options] [file/directory/-]
```

**Options disponibles** :
- `--recursive` : analyse tous les dossiers et sous-dossiers
- `--bell` : émet un son lors de la détection d'un virus
- `--move` : déplace les virus détectés dans le répertoire de quarantaine

**Documentation** : [ClamAV Scanning](https://docs.clamav.net/manual/Usage/Scanning.html#daemon)

### Fonctionnement d'une clé USB

- **Adresse et classe** : chaque clé reçoit une adresse lors de l'insertion et se classe parmi les périphériques USB
- **Canaux actifs** : un périphérique USB peut avoir jusqu'à 32 canaux (16 dans le contrôleur hôte, 16 hors)
- **Paquets** : trois types de base :
  - Poignée de main (handshake)
  - Jeton (token)
  - Données (data)

**Descripteurs USB** (norme USB) :
- **Device descriptor** : informations générales (Product ID, Vendor ID, manufacturier)
- **Configuration descriptor** : états du composant USB, nombre d'interfaces
- **Interface descriptor** : ensemble d'Endpoints (extrémités d'un « pipe »)

**Identifiants** :
- **PID et VID** : codés sur 16 bits, identifier le constructeur et le produit de façon unique

**Ressources** :
- [Explication synthétique du fonctionnement des USB](https://www.malekal.com/usb-universal-serial-bus/)
- [PDF de la norme USB](http://u.s.b.free.fr/pdf/L_USB_et_sa_norme_v1.pdf)
