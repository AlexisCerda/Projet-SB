#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "ini.h"

#define BUFFER_SIZE 1024
#define MOUNT_POINT "/mnt/analyse_usb"
#define QUARANTINE "/quarantine"
#define MAIL {"alexis.cerdadealmeida@gmail.com","khaosprive88@gmail.com"}
#define NB_MAIL 2
#define DELIMITER "\n======================================================\n"

typedef struct {
    char* mail_sender;
    char* pwd_app_sender;
    char* mail_smtp_server;
    int mail_smtp_port;
    char* mail_smtp_auth;
    char* mail_smtp_starttls_enable;
    char** mail_recever;
    int nb_mail_recever;
    char* mail_subject;
    char* mail_body;
    char* path_quarantine;
    char* path_mount_point;
    int buffer_size;
} configuration;

configuration config;

int envoie_mail(configuration* config) {
    char hostname[BUFFER_SIZE];
    char cmd[BUFFER_SIZE*2];
    gethostname(hostname, sizeof(hostname)); //récupère le nom de la machine et le met dans hostname

    for (int i = 0; i < config->nb_mail_recever; i++) {
        snprintf(cmd, sizeof(cmd), "java -jar mail.jar %s %s %s \"%i\" %s %s %s %s %s %s\" ", // ouverture du fichier java avec tous les paramètres nécessaire pour envoyer le mail
        config->mail_sender, config->pwd_app_sender, config->mail_smtp_server, config->mail_smtp_port, config->mail_smtp_auth, config->mail_smtp_starttls_enable, config->mail_recever[i], config->mail_subject, config->mail_body, hostname);
        int result = system(cmd); // execute la commande cmd

        if (result != 0) {
            printf("[ERREUR] Le script Java a retourné une erreur ou n'est pas installé.\n");
        }
    }
    return 0;
}

void Scan_part(char* quarantine_path, char* mount_point, char** Mail ){
    
    char cmd_scan[BUFFER_SIZE*2]; //commande du scan de ClamAV
    /*
        clamscan est la commande pour lancer l'analyse
        -r (récursif) entre dans chaque dossier et sous dossier
        --bell = fait un bip sonor quand un virus est détecté 
        --move = si un virus est détecter alors il le déplace à l'endrois en paramètre
        %s est le chemin, mount_point et les \"  permet de mettre de vrai double quote sans qu'elle soit considéré par le langage C 
    */
    snprintf(cmd_scan, sizeof(cmd_scan), "clamscan -r --bell --move=\"%s\" \"%s\"", quarantine_path, mount_point); // enregistre dans cmd_scan la chaine de caractère
        
    printf("%sLancement de ClamAV ... %s", DELIMITER, DELIMITER);
    int resultatClamAV = system(cmd_scan); // lance le scan de ClamAV et met en pause le programme

    snprintf(cmd_scan, sizeof(cmd_scan), "uvscan --secure -r --summary --move \"%s\" \"%s\"", quarantine_path, mount_point);
    printf("%sLancement de Trellix ... %s", DELIMITER, DELIMITER);

    int resultatTrellix = system(cmd_scan); // lance le scan de Trellix et met en pause le programme

    if(WEXITSTATUS(resultatTrellix) ==1 || WEXITSTATUS(resultatClamAV) ==1){
        printf("%sVirus détecter envoye de mail à %s", DELIMITER, DELIMITER);
        for (size_t i = 0; i < NB_MAIL; i++){
            printf("-%s \n",Mail[i]);
        }
        envoie_mail(&config);

        char cmd_popup[BUFFER_SIZE * 2];
        
        // 1. GUI_USER=$(who | head -n 1 | awk '{print $1}') -> Récupère le nom du premier utilisateur connecté.
        // 2. sudo -u $GUI_USER -> Lance l'interface avec ce compte utilisateur.
        // 3. DISPLAY=:0 -> Force l'affichage sur l'écran principal.
        snprintf(cmd_popup, sizeof(cmd_popup), 
                 "GUI_USER=$(who | head -n 1 | awk '{print $1}'); "
                 "if [ ! -z \"$GUI_USER\" ]; then "
                 "sudo -u $GUI_USER DISPLAY=:0 zenity --warning --title='Alerte Sécurité !' "
                 "--text='Un virus a été détecté sur la clé USB et mis en quarantaine.' --width=300 & "
                 "fi");
                 
        system(cmd_popup);
    }

    system("sync");
}

void monter_et_scanner() {
    FILE *fp;
    char device_node[BUFFER_SIZE] = ""; //chemin de la partition de la clé
    char mount_point[] = MOUNT_POINT; //dossier dans lequel on vas monter la clé
    char quarantine_path[] = QUARANTINE; //dossier de quarantaine
    char *Mail[] = MAIL;

    /*
        ls -1 = liste une par ligne
        /dev/disk/by-path/ = dosier que linux créer pour les devices
        usb* = tous ce qui commence par usb
        part* = tous ce qui représente une partition
        2> = les code d'erreur
        /dev/null = la corbeille
        xargs -r readlink -f = convertit en chemin réel (ex: /dev/sda1)
        sort -u = trie et supprime les doublons
    */
    const char *cmd_find = "ls -1 /dev/disk/by-path/*usb*part* 2>/dev/null | xargs -r readlink -f | sort -u"; 

    fp = popen(cmd_find, "r");

    if (fp == NULL) {
        printf("[ERREUR] Impossible d'exécuter la commande.\n");
        return;
    }

    struct stat st = {0}; // Type qui contient des infos sur le fichier
    if (stat(mount_point, &st) == -1) { // stat() récupère les infos dans le fichier et de les mettre dans st
        mkdir(mount_point, 0700); // création du dossier du mount_point et attribut tout les droit au propriétaire (rwx) 
    }
    if (stat(quarantine_path, &st) == -1) {
        mkdir(quarantine_path, 0700); 
    }

    // BOUCLE : parcourt toutes les partitions trouvées
    while (fgets(device_node, sizeof(device_node), fp) != NULL) { // lit une ligne de la sortie de cmd_find et la met dans device_node
        device_node[strcspn(device_node, "\n")] = 0; // supprime le caractère de nouvelle ligne à la fin de device_node

        if (strlen(device_node) < 5) continue; // si le chemin est trop court, on considère que ce n'est pas une partition valide et on passe à la suivante

        printf("%sDémarrage dans la partition : %s%s",DELIMITER, device_node, DELIMITER); 

        //  ### Réparation et Montage de la partition ###

        char cmd_fix[BUFFER_SIZE*2];
        char cmd_mount[BUFFER_SIZE*2];
        
        /*
            ntfsfix --> répare les partitions NTFS
            -d = force le néttoyage de la partition même si elle est marquée comme propre
            > = redirige la sortie standard vers un fichier ou un périphérique
            /dev/null = la corbeille
            >&1 : C'est la syntaxe Linux pour gérer les erreurs
            1 représente la sortie standard (le texte normal)
            2 représente la sortie d'erreur (les messages d'alerte)
            2>&1 = envoyer les erreurs et le texte à la corbeille
        */
        snprintf(cmd_fix, sizeof(cmd_fix), "ntfsfix -d \"%s\" > /dev/null 2>&1", device_node); //mise en place de cmd_fix
        system(cmd_fix); 
        
        // -t auto : Linux détect(FAT32, NTFS, exFAT...)
        // -o remove_hiberfile : enleve le verrou de windows
        // -o rw : On force la lecture/écriture.
        snprintf(cmd_mount, sizeof(cmd_mount), "mount -o rw,sync \"%s\" \"%s\"", device_node, mount_point);
            
        if (system(cmd_mount) != 0) { //execute cmd_mount et si erreur on passe à la suivante
            printf("Impossible de monter la partition. Passage à la suivante...\n");
            continue; 
        }
        printf("Montage réussi dans : %s\n", mount_point);

        // ### Scan de la partition et envoie de mail si virus détecté ###

        Scan_part(quarantine_path, mount_point, Mail);

        // ### Démontage de la partition ###

        char cmd_umount[BUFFER_SIZE*2]; // commande umount
        // umount permet de enlever la référence de la clé USB à Linux
        // %s fait référence au mount_point
        snprintf(cmd_umount, sizeof(cmd_umount), "umount \"%s\"", mount_point);
        system(cmd_umount); // exécute cmd_umount
        printf("Clé démontée\n");
    } // Fin de la boucle while

    pclose(fp);
    
    rmdir(mount_point); // supprime le répértoire mount_point seulement s'il est vide.
}

int hotplug_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data) {
    struct libusb_device_descriptor desc;  //description du device
    libusb_get_device_descriptor(dev, &desc); // donne la description du device et la met dans dev
    
    printf("\nNew Device detect (VID:%04x PID:%04x)\n", desc.idVendor, desc.idProduct);
    
    printf("Initialisation du périphérique (5 secondes)...\n");
    sleep(5); //attend 5 seconde

    monter_et_scanner();

    return 0;
}

//Reprend le fichier ini et place les valeurs dans la struct configuration
static int handler( void* config, const char* section, const char* name, const char* value)
{
    configuration* pConfig = (configuration*)config ;

    if (strcmp(section,"") == 0) {
        if (strcmp(name,"mail_sender")== 0){
            pConfig->mail_sender = strdup(value);
        }
        else if (strcmp(name,"pwd_app_sender")== 0){
            pConfig->pwd_app_sender = strdup(value);
        }
        else if (strcmp(name,"mail_smtp_server")== 0){
            pConfig->mail_smtp_server = strdup(value);
        }
        else if (strcmp(name,"mail_smtp_port")== 0){
            pConfig->mail_smtp_port = atoi(value);
        }
        else if (strcmp(name,"mail_smtp_auth")== 0){
            pConfig->mail_smtp_auth = strdup(value);
        }
        else if (strcmp(name,"mail_smtp_starttls_enable")== 0){
            pConfig->mail_smtp_starttls_enable = strdup(value);
        }
        else if (strcmp(name, "mail_recever") == 0) {
            char* list = strdup(value); 
            char* token = strtok(list, ","); // On coupe aux virgules ou espaces

            while (token != NULL) {
                pConfig->mail_recever = realloc(pConfig->mail_recever, sizeof(char*) * (pConfig->nb_mail_recever + 1)); // on augmente la taille du tableau de mail_recever
                pConfig->mail_recever[pConfig->nb_mail_recever] = strdup(token); // on ajoute le mail à la liste
                pConfig->nb_mail_recever++; // on augmente le nombre de mail recever
                token = strtok(NULL, ",");
            }
            free(list);
        }
        else if (strcmp(name,"mail_subject")== 0){
            pConfig->mail_subject = strdup(value);
        }
        else if (strcmp(name,"mail_body")== 0){
            pConfig->mail_body = strdup(value);
        }
        else if (strcmp(name,"path_quarantine")== 0){
            pConfig->path_quarantine = strdup(value);
        }
        else if (strcmp(name,"path_mount_point")== 0){
            pConfig->path_mount_point = strdup(value);
        }
        else if (strcmp(name,"buffer_size")== 0){
            pConfig->buffer_size = atoi(value);
        }
    }
    
    else {
        return 0;
    }
    return 1;
}

int MAJ_scanner(){
    char cmd_update[BUFFER_SIZE*2]; // cmd pour la mise à jour des scanners
    snprintf(cmd_update, sizeof(cmd_update), "sudo ./MAJ_Trellix.bash"); // MAJ_Trellix.bash est un script qui met à jour les signatures de Trellix
    printf("%sMise à jour de ClamAV et de Trellix ... %s", DELIMITER, DELIMITER);
    int result = system(cmd_update); // Execution de cmd_update pour Trellix
    return result; // retourne le résultat de la mise à jour de Trellix, si c'est différent de 0 alors il y a eu une erreur
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    // Valeurs par défaut
    config.mail_sender = NULL;
    config.pwd_app_sender = NULL;
    config.mail_smtp_server = NULL;
    config.mail_smtp_port = 587;
    config.mail_smtp_auth = "true";
    config.mail_smtp_starttls_enable = "true";
    config.mail_recever = NULL;
    config.nb_mail_recever = 0;
    config.mail_subject = NULL;
    config.mail_body = NULL;
    config.path_quarantine = NULL;
    config.path_mount_point = NULL;
    config.buffer_size = 1024;

    // Analyse du fichier (le nom du fichier, le handler, et le pointeur vers la struct)
    if (ini_parse("Config.ini", handler, &config) < 0) {
        printf("Impossible de charger 'Config.ini'\n");
        return 1;
    }

    
    if (MAJ_scanner() == 1){
        exit(1);
    }
    

    libusb_context *ctx = NULL; //contexte
    libusb_hotplug_callback_handle handle; //ouverture

    libusb_init(&ctx); //initialisation du contexte

    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        printf("Erreur : Hotplug non supporté.\n");
        return 1;
    }

    //Mise en place du callback
    //Prend tout les nouveaux devices et c'est le cmd_find qui fait le trie
    libusb_hotplug_register_callback(ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0, 
                                     LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, 
                                     LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, NULL, &handle);

    printf("%sEn attente de clé USB%s", DELIMITER, DELIMITER);


    
    while (1) {
        libusb_handle_events_completed(ctx, NULL);
    }
    
    libusb_exit(ctx);
    return 0;
}

/*
Tester avec un Anti Malware, eicar
UTILISER SUDO
gcc -o VirusDetector -Wall VirusDetector.c ini.c -lusb-1.0
Pour démmarrer le programme automatiquement : (/etc/systemd/system/virusdetector.service)
    -Créer un service systemd qui lance le programme au démarrage de la machine :
        # 1. Recharger les configurations de Systemd
        sudo systemctl daemon-reload

        # 2. Activer le lancement automatique au démarrage du PC
        sudo systemctl enable virusdetector.service

        # 3. Démarrer le programme immédiatement
        sudo systemctl start virusdetector.service

    - Mon fichier virusdetector.service :
        [Unit]
        Description=Service de détection de virus sur clé USB
        After=network.target

        [Service]
        # Dossier de travail (très important pour qu'il trouve ton Config.ini)
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
    - Pour vérifier que le service fonctionne :
        sudo systemctl status virusdetector.service
    
Pour le Cron :
    - 0 0 * * * /opt/VirusDetector/MAJ_Trellix.bash >> /var/log/maj_clamavtrellix.log 2>&1 dans le crontab pour exécuter la commande tout les 24 heures à minuit
    - mm hh jj MMM JJJ tâche > log
    -crontab -e pour éditer le cron

Pour les MAJ des scanner :
    -ClamAV : commande freshclam
    -Trellix : 
        cd /tmp

        wget http://update.nai.com/products/commonupdater/avvdat-9999.zip -O avvdat.zip

        unzip -o avvdat.zip

        sudo mv *.dat /usr/local/uvscan/

        rm avvdat.zip

        /usr/local/uvscan/uvscan --version

Pour Trelllix le scan se fait avec :
    uvscan --secure -r --summary --move <quarantine_dir> <target_dir>

    options :
    - --secure = fait l'option --analyse et --unzip
    - -r = récursif, entre dans chaque dossier et sous dossier
    - --analyse = analyse les fichier de manière heuristique et pas seulement avec les signatures
    - --unzip = décompresse les fichiers compressés pour les analyser
    - --move = permet de déplacer tout les virus dans un fichier concidérer comme quarantaine
    - --summary = affiche un résumé de l'analyse à la fin

cls704upg.pdf --> Doc Treellix

Pour envoyer un mail avec Java :
    -il faut un script java qui utilise la librairie Jakarta pour envoyer des mail
    -le script doit prendre en paramètre les informations de connexion et les informations du mail (destinataire, sujet, corps du message et tous les paramètre pour config le serveur SMTP)

Récupéré le nom d'un ordinateur :
    -gethostname() qui prend en paramètre un tableau de char et sa taille, et qui remplit le tableau avec le nom de la machine

Ouvrir un fichier ini en C :
    -il faut un handler qui va faire le lien entre les sections et les clés du fichier ini et la struct configuration
    -ini_parse() qui prend en paramètre le nom du fichier, le handler et un pointeur vers la struct configuration

Pour ClamAV le scan se fait avec :
    clamdscan [*options*] [*file/directory/-*]

    options :
    - --recursive --> analyse tout les dossier et sous dossier
    - --bell = sound bell on virus detection --> Fait un son lors de la détection de virus
    - --move = permet de déplacer tout les virus dans un fichier concidérer comme quarantaine


https://docs.clamav.net/manual/Usage/Scanning.html#daemon --> DOC


Une clé USB marche :
    -On lui attribut une adresse lors de l'insertion et une classe
    -Un périphérique USB peut avoir jusqu’à 32 canaux actifs : 16 dans le contrôleur hôte et 16 hors du contrôleur.
    -Les paquets sont de trois types de base : poignée de main, jeton, données. (un paquet = 1 octet, handler token data = poignée de main jeton données dans le programme )
    -La norme USB veut que chaque appareil USB aient 3 descriptions :
            Device descriptor --> informations générales (Product ID et Vendor ID dans le code ou IManufacturer)
            Configuration descriptor --> les différents états du composant USB (pour avoir BNumInterfaces, utile quand on en cherche une en particulier)
            Interface descriptor --> Une interface peut être considérée comme un ensemble d’ «Endpoint ». (Un Endpoint est l’extrémité d’un « pipe »)
    -PID et VID codée sur 16 bits --> reconnaitre un Constructeur et un produit avec des identifiants unique


https://www.malekal.com/usb-universal-serial-bus/ --> Explication synthétique du fonctionnement des USB
http://u.s.b.free.fr/pdf/L_USB_et_sa_norme_v1.pdf --> PDF de la norme USB
*/