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
        snprintf(cmd, sizeof(cmd), "java -Djava.net.preferIPv4Stack=true -jar mail.jar %s %s %s \"%i\" %s %s %s %s %s %s\" ", // ouverture du fichier java avec tous les paramètres nécessaire pour envoyer le mail
        config->mail_sender, config->pwd_app_sender, config->mail_smtp_server, config->mail_smtp_port, config->mail_smtp_auth, config->mail_smtp_starttls_enable, config->mail_recever[i], config->mail_subject, config->mail_body, hostname);
        int result = system(cmd); // execute la commande cmd

        if (result != 0) {
            printf("[ERREUR] Le script Java a retourné une erreur ou n'est pas installé.\n");
        }
    }
    return 0;
}

// Système de notification avec fenêtre unique
// START : démarre une nouvelle fenêtre (efface l'historique)
// MSG : ajoute un message à la fenêtre existante
// CLOSE : ferme la fenêtre après 5 secondes

void execute_notification_cmd(const char* cmd_type, const char* title, const char* message) {
    char cmd[BUFFER_SIZE * 4];
    char script_path[BUFFER_SIZE];
    
    snprintf(script_path, sizeof(script_path), "%s/notification_gui.py", getenv("PWD") ? getenv("PWD") : ".");
    
    snprintf(cmd, sizeof(cmd),
        "GUI_USER=$(loginctl list-sessions --no-legend 2>/dev/null | awk '{print $3}' | grep -v '^root$' | head -n 1); "
        "if [ -z \"$GUI_USER\" ]; then GUI_USER=$(who | awk '{print $1}' | grep -v '^root$' | head -n 1); fi; "
        "if [ -z \"$GUI_USER\" ]; then echo '[INFO] Aucun utilisateur graphique trouvé.'; exit 0; fi; "
        "DISPLAY_VAR=$(sudo -u \"$GUI_USER\" printenv DISPLAY 2>/dev/null || echo ':0'); "
        "XAUTHORITY_VAR=$(sudo -u \"$GUI_USER\" printenv XAUTHORITY 2>/dev/null); "
        "if [ -z \"$XAUTHORITY_VAR\" ]; then XAUTHORITY_VAR=\"/home/$GUI_USER/.Xauthority\"; fi; "
        "sudo -u \"$GUI_USER\" DISPLAY=\"$DISPLAY_VAR\" XAUTHORITY=\"$XAUTHORITY_VAR\" "
        "python3 \"%s\" %s '%s' '%s' &",
        script_path, cmd_type, title ? title : "", message ? message : "");
    system(cmd);
}

// Démarre une nouvelle fenêtre de notification (efface l'historique)
void start_notification_window(const char* title, const char* message) {
    execute_notification_cmd("START", title, message);
}

// Ajoute un message à la fenêtre existante
void send_notification(const char* title, const char* message, const char* urgency) {
    execute_notification_cmd("MSG", title, message);
}

// Ferme la fenêtre après 5 secondes
void close_notification_window() {
    execute_notification_cmd("CLOSE", NULL, NULL);
}

void Scan_part(char* quarantine_path, char* mount_point, char** Mail ){
    
    char cmd_scan[BUFFER_SIZE*2]; //commande du scan de ClamAV
    /*
        clamscan est la commande pour lancer l'analyse
        -r (récursif) entre dans chaque dossier et sous dossier
        --bell = fait un bip sonore quand un virus est détecté 
        --move = si un virus est détecté alors il le déplace à l'endroit en paramètre
        %s est le chemin, mount_point et les \"  permet de mettre de vrai double quote sans qu'elle soit considéré par le langage C 
    */
    snprintf(cmd_scan, sizeof(cmd_scan), "clamscan -r --bell --move=\"%s\" \"%s\"", quarantine_path, mount_point); // enregistre dans cmd_scan la chaine de caractère
        
    printf("%sLancement de ClamAV ... %s", DELIMITER, DELIMITER);
    int resultatClamAV = system(cmd_scan); // lance le scan de ClamAV et met en pause le programme

    snprintf(cmd_scan, sizeof(cmd_scan), "uvscan --secure -r --summary --move \"%s\" \"%s\"", quarantine_path, mount_point);
    printf("%sLancement de Trellix ... %s", DELIMITER, DELIMITER);

    int resultatTrellix = system(cmd_scan); // lance le scan de Trellix et met en pause le programme

    if(WEXITSTATUS(resultatTrellix) ==1 || WEXITSTATUS(resultatClamAV) ==1){
        printf("%sVirus détecté, envoi de mail à %s", DELIMITER, DELIMITER);
        for (size_t i = 0; i < config.nb_mail_recever; i++){
            printf("-%s \n",Mail[i]);
        }
        envoie_mail(&config);

        send_notification("Alerte Sécurité !", "Un virus a été détecté et mis en quarantaine.", "critical");
    } else {
        send_notification("Analyse terminée", "La clé USB a été analysée avec succès. Aucun virus détecté.", "normal");
    }

    system("sync");
}

void monter_et_scanner() {
    FILE *fp;
    char device_node[BUFFER_SIZE] = ""; //chemin de la partition de la clé
    char mount_point[] = MOUNT_POINT; //dossier dans lequel on vas monter la clé
    char quarantine_path[] = QUARANTINE; //dossier de quarantaine
    char **Mail = config.mail_recever; //liste des mail recever

    /*
    1. find cherche dans by-id les raccourcis de type "lien" (-type l) nommés "usb-*-part*"
    2. -exec readlink -f {} \; convertit ces raccourcis en vrais chemins (/dev/sdb1)
    3. sort -u supprime les doublons
    Cette méthode est insensible aux labels bizarres et cible 100% l'USB.
    */
    const char *cmd_find = "find /dev/disk/by-id/ -type l -name 'usb-*-part*' -exec readlink -f {} \\; 2>/dev/null | sort -u";
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
            -d = force le nettoyage de la partition même si elle est marquée comme propre
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
        // -o remove_hiberfile : enlève le verrou de windows
        // -o rw : On force la lecture/écriture.
        snprintf(cmd_mount, sizeof(cmd_mount), "mount -o rw,sync \"%s\" \"%s\"", device_node, mount_point);
            
        if (system(cmd_mount) != 0) { //execute cmd_mount et si erreur on passe à la suivante
            printf("Impossible de monter la partition. Passage à la suivante...\n");
            continue; 
        }
        start_notification_window("Clé USB détectée", "Analyse en cours...");
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
        
        // Fermer la fenêtre de notification après 5 secondes
        close_notification_window();
    } // Fin de la boucle while

    pclose(fp);
    
    rmdir(mount_point); // supprime le répértoire mount_point seulement s'il est vide.
}

int hotplug_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data) {
    struct libusb_device_descriptor desc;  //description du device
    libusb_get_device_descriptor(dev, &desc); // donne la description du device et la met dans dev
    
    printf("\nNew Device detected (VID:%04x PID:%04x)\n", desc.idVendor, desc.idProduct);
    
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
    //Prend tout les nouveaux devices et c'est le cmd_find qui fait le tri
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