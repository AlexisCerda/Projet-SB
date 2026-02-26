#!/usr/bin/env python3
import sys
import tkinter as tk
from tkinter import scrolledtext
import os
from datetime import datetime

def show_notification(title, message):
    # Utiliser un fichier log dans le répertoire home de l'utilisateur pour éviter les problèmes de permissions
    home_dir = os.path.expanduser("~")
    logfile = os.path.join(home_dir, '.virusdetector_log.txt')
    
    # Ajouter le message au log
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    log_entry = f"[{timestamp}] {title}: {message}\n"
    
    try:
        with open(logfile, 'a') as f:
            f.write(log_entry)
        # Assurer les bonnes permissions (lecture/écriture pour tous)
        os.chmod(logfile, 0o666)
    except Exception as e:
        print(f"[WARN] Impossible d'écrire dans le log: {e}")
    
    # Créer la fenêtre
    root = tk.Tk()
    root.title("VirusDetector - Notifications")
    root.geometry("600x400")
    
    # Zone de texte avec scrollbar
    text = scrolledtext.ScrolledText(root, wrap=tk.WORD, font=("Arial", 11))
    text.pack(expand=True, fill='both', padx=10, pady=10)
    
    # Charger l'historique des logs
    if os.path.exists(logfile):
        try:
            with open(logfile, 'r') as f:
                text.insert(tk.END, f.read())
        except Exception as e:
            text.insert(tk.END, f"Erreur de lecture du log: {e}\n\n")
            text.insert(tk.END, log_entry)
    else:
        text.insert(tk.END, log_entry)
    
    # Scroller jusqu'à la fin
    text.see(tk.END)
    text.config(state=tk.DISABLED)
    
    # Bouton pour fermer
    btn = tk.Button(root, text="Fermer", command=root.destroy, font=("Arial", 10))
    btn.pack(pady=5)
    
    # Auto-fermer après 10 secondes
    root.after(10000, root.destroy)
    
    root.mainloop()

if __name__ == "__main__":
    if len(sys.argv) >= 3:
        title = sys.argv[1]
        message = " ".join(sys.argv[2:])
        show_notification(title, message)
    else:
        show_notification("VirusDetector", "Notification")
