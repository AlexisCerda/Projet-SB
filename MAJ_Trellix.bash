#!/bin/bash
echo "--- Mise à jour de ClamAV et de Trellix ---"
freshclam # Mise à jour de ClamAV
INSTALL_DIR="/usr/local/uvscan" # Répertoire de de Trellix
TEMP_DIR="/tmp/uvscan_update" # Répertoire temporaire pour les téléchargements
URL_BASE="http://update.nai.com/products/commonupdater" # URL des dat de Trellix 

echo "--- Mise à jour de Trellix UVSCAN ---"

rm -rf $TEMP_DIR # Supprime le répertoire s'il existe déjà (-rf pour forcer la suppression sans confirmation)
mkdir -p $TEMP_DIR # Crée le répertoire temporaire (-p pour ne pas planter s'il existe déjà)
cd $TEMP_DIR || exit # On se déplace dedans

echo "Récupération oem.ini..."
wget -q $URL_BASE/oem.ini -O oem.ini #web get oem.ini et le sauvegarde sous le nom oem.ini, on force son nom pour éviter les problèmes de nom de fichier

if [ ! -s oem.ini ]; then # Vérifie si oem.ini est vide ou n'existe pas
    echo "ERREUR : oem.ini est vide ou introuvable."
    exit 1
fi

# Création de la variable d'environnement LATEST_VER qui contient la version des dat de Trellix, on utilise grep pour trouver la ligne contenant "DATVersion="
# head = prendre que la première ligne trouvée, cut = extraire la partie après le signe égal,tr = supprimer les retours chariot éventuels
LATEST_VER=$(grep "DATVersion=" oem.ini | head -n 1 | cut -d= -f2 | tr -d '\r') 

if [[ ! "$LATEST_VER" =~ ^[0-9]+$ ]]; then # Vérifie que LATEST_VER ne contient que des chiffres
    echo "ERREUR : Le numéro de version récupéré est invalide : '$LATEST_VER'"
    exit 1
fi

echo "Dernière version identifiée : [$LATEST_VER]"

FILENAME="avvdat-$LATEST_VER.zip" 
echo "Téléchargement de $FILENAME ..."
wget "$URL_BASE/$FILENAME" # Téléchargement du fichier dat correspondant à la dernière version identifiée

if [ -f "$FILENAME" ]; then # Vérifie que le fichier a été téléchargé avec succès
    echo "Extraction..."
    unzip -o -q "$FILENAME" # Extraction du fichier zip, -o pour écraser les fichiers existants sans demander, -q pour ne pas afficher les détails de l'extraction
    
    echo "Installation des fichiers dans $INSTALL_DIR..."
    mv *.dat "$INSTALL_DIR/" # Déplace les fichiers dat extraits vers le répertoire d'installation de Trellix

    cd ..
    rm -rf $TEMP_DIR # On supprime le répertoire temporaire et son contenu 
    
    echo "--- Terminé ! État actuel : ---"
    "$INSTALL_DIR/uvscan" --version | grep "Dat set version:" # Affiche la version actuelle de Trellix
else
    echo "ERREUR : Le téléchargement de $FILENAME a échoué."
    exit 1
fi