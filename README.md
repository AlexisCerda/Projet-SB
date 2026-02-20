# Projet SB - Station de Décontamination USB

Ce projet est une solution de sécurité automatisée conçue pour détecter, scanner et nettoyer les périphériques USB dès leur insertion. Il combine un détecteur bas niveau en C, un moteur de mise à jour pour antivirus (ClamAV & Trellix), et un système de notification par email.

## 🚀 Fonctionnalités

*   **Détection automatique** : Surveillance des ports USB via `libusb` pour détecter l'insertion de périphériques de stockage.
*   **Montage & Scan** : Montage automatique du périphérique et analyse antivirale.
*   **Mise en quarantaine** : Déplacement automatique des fichiers infectés vers un dossier sécurisé.
*   **Mise à jour intelligente** : Script Bash (`MAJ_Trellix.bash`) pour maintenir à jour les définitions de virus ClamAV et Trellix (McAfee).
*   **Notifications** : Envoi d'alertes par email (via Java) contenant les détails de l'infection et le nom de la machine hôte.
*   **Configuration flexible** : Fichier `Config.ini` pour gérer les paramètres sans recompiler.

## 🛠️ Prérequis

Pour compiler et exécuter ce projet, vous avez besoin de :

*   **Système OS** : Linux (Debian/Ubuntu recommandé).
*   **Compilateur C** : `gcc`.
*   **Librairies** : `libusb-1.0-0-dev`.
*   **Java** : JRE installé (pour le module `mail.jar`).
*   **Antivirus** : ClamAV et/ou Trellix UVSCAN installés.
*   **Utilitaires** : `wget`, `unzip` (pour le script de mise à jour).

## 📦 Installation & Compilation

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

## ⚙️ Configuration

Modifiez le fichier `Config.ini` pour adapter le comportement à votre infrastructure :

```ini
[Mail]
sender=votre_email@gmail.com
password=votre_mot_de_passe_app
smtp_server=smtp.gmail.com
smtp_port=587
...
```

## ▶️ Utilisation

1.  **Lancer le détecteur** (nécessite les droits root pour l'accès USB et le montage) :
    ```bash
    sudo ./VirusDetector
    ```

2.  **Mettre à jour les antivirus** :
    Exécutez le script de mise à jour périodiquement (via cron par exemple) :
    ```bash
    sudo ./MAJ_Trellix.bash
    ```

## 🏗️ Architecture

*   `VirusDetector.c` : Programme principal (C + libusb).
*   `MAJ_Trellix.bash` : Script de mise à jour des bases virales.
*   `mail.jar` : Utilitaire d'envoi de mails.
*   `ini.c` / `ini.h` : Parser de fichier de configuration.
