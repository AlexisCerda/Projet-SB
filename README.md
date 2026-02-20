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
Rappel le programme dois être appelé avec un sudo

##  Explication des fonctionnalitées

Voici quelques explications/rappel sur les méthodes utilisé durant ce projet.

Pour démmarrer le programme automatiquement : (/etc/systemd/system/virusdetector.service)
    - Créer un service systemd qui lance le programme au démarrage de la machine :<br>
        # 1. Recharger les configurations de Systemd<br>
        sudo systemctl daemon-reload<br><br>
        # 2. Activer le lancement automatique au démarrage du PC<br>
        sudo systemctl enable virusdetector.service<br><br>
        # 3. Démarrer le programme immédiatement<br>
        sudo systemctl start virusdetector.service<br><br>
    - Mon fichier virusdetector.service :<br><br>
        [Unit]<br>
        Description=Service de détection de virus sur clé USB<br>
        After=network.target<br>
        [Service]<br>
        # Dossier de travail (très important pour qu'il trouve ton Config.ini)<br>
        WorkingDirectory=/opt/VirusDetector/<br>
        # Le chemin exact vers ton exécutable<br>
        ExecStart=/opt/VirusDetector/VirusDetector<br>
        # Redémarrer automatiquement en cas de crash<br>
        Restart=always<br>
        # Attendre 3 secondes avant de redémarrer pour éviter de surcharger le système en cas d'erreur en boucle<br>
        RestartSec=3<br>
        # Lancer avec les droits root (nécessaire pour libusb et mount)<br>
        User=root<br>
        # Lancer automatiquement au démarrage de l'ordinateur<br>
        WantedBy=multi-user.target<br><br>
    - Pour vérifier que le service fonctionne :<br>
        sudo systemctl status virusdetector.service<br>
    
Pour le Cron :  

    - 0 0 * * * /opt/VirusDetector/MAJ_Trellix.bash >> /var/log/maj_clamavtrellix.log 2>&1 dans le crontab pour exécuter la commande tout les 24 heures à minuit
    - mm hh jj MMM JJJ tâche > log
    -crontab -e pour éditer le cron

Pour les MAJ des scanner :<br>
    -ClamAV : commande freshclam<br>
    -Trellix : <br>

        cd /tmp

        wget http://update.nai.com/products/commonupdater/avvdat-9999.zip -O avvdat.zip

        unzip -o avvdat.zip

        sudo mv *.dat /usr/local/uvscan/

        rm avvdat.zip

        /usr/local/uvscan/uvscan --version

Pour Trelllix le scan se fait avec :<br><br>
    uvscan --secure -r --summary --move <quarantine_dir> <target_dir><br>
    options :<br>
    - --secure = fait l'option --analyse et --unzip<br>
    - -r = récursif, entre dans chaque dossier et sous dossier<br>
    - --analyse = analyse les fichier de manière heuristique et pas seulement avec les signatures<br>
    - --unzip = décompresse les fichiers compressés pour les analyser<br>
    - --move = permet de déplacer tout les virus dans un fichier concidérer comme quarantaine<br>
    - --summary = affiche un résumé de l'analyse à la fin<br><br>

cls704upg.pdf --> Doc Treellix<br>

Pour envoyer un mail avec Java :<br><br>
    -il faut un script java qui utilise la librairie Jakarta pour envoyer des mail<br>
    -le script doit prendre en paramètre les informations de connexion et les informations du mail (destinataire, sujet corps du message et tous les paramètre pour config le serveur SMTP)<br><br>

Récupéré le nom d'un ordinateur :<br>
    -gethostname() qui prend en paramètre un tableau de char et sa taille, et qui remplit le tableau avec le nom de la machine<br>

Ouvrir un fichier ini en C :<br><br>
    -il faut un handler qui va faire le lien entre les sections et les clés du fichier ini et la struct configuration<br>
    -ini_parse() qui prend en paramètre le nom du fichier, le handler et un pointeur vers la struct configuration<br>

Pour ClamAV le scan se fait avec :<br><br>
    clamdscan [*options*] [*file/directory/-*]<br>
    options :<br>
    - --recursive --> analyse tout les dossier et sous dossier<br>
    - --bell = sound bell on virus detection --> Fait un son lors de la détection de virus<br>
    - --move = permet de déplacer tout les virus dans un fichier concidérer comme quarantaine<br><br>


https://docs.clamav.net/manual/Usage/Scanning.html#daemon --> DOC<br>


Une clé USB marche :<br><br>
    -On lui attribut une adresse lors de l'insertion et une classe<br>
    -Un périphérique USB peut avoir jusqu’à 32 canaux actifs : 16 dans le contrôleur hôte et 16 hors du contrôleur.<br>
    -Les paquets sont de trois types de base : poignée de main, jeton, données. (un paquet = 1 octet, handler token data = poignée de main jeton données dans le programme )<br>
    -La norme USB veut que chaque appareil USB aient 3 descriptions :<br>
            Device descriptor --> informations générales (Product ID et Vendor ID dans le code ou IManufacturer)<br>
            Configuration descriptor --> les différents états du composant USB (pour avoir BNumInterfaces, utile quand on en cherche une en particulier)<br>
            Interface descriptor --> Une interface peut être considérée comme un ensemble d’ «Endpoint ». (Un Endpoint est l’extrémité d’un « pipe »)<br>
    -PID et VID codée sur 16 bits --> reconnaitre un Constructeur et un produit avec des identifiants unique<br><br>


https://www.malekal.com/usb-universal-serial-bus/ --> Explication synthétique du fonctionnement des USB<br>
http://u.s.b.free.fr/pdf/L_USB_et_sa_norme_v1.pdf --> PDF de la norme USB<br>

