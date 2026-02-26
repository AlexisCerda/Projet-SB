#!/usr/bin/env python3
import sys
import tkinter as tk
from tkinter import scrolledtext
import os
from datetime import datetime

def show_notification(title, message):
    # Fichier de log
    logfile = '/tmp/virusdetector_log.txt'
    
    # Ajouter le message au log
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    log_entry = f"[{timestamp}] {title}: {message}\n"
    
    with open(logfile, 'a') as f:
        f.write(log_entry)
    
    # Créer la fenêtre
    root = tk.Tk()
    root.title("VirusDetector - Notifications")
    root.geometry("600x400")
    
    # Zone de texte avec scrollbar
    text = scrolledtext.ScrolledText(root, wrap=tk.WORD, font=("Arial", 11))
    text.pack(expand=True, fill='both', padx=10, pady=10)
    
    # Charger l'historique des logs
    if os.path.exists(logfile):
        with open(logfile, 'r') as f:
            text.insert(tk.END, f.read())
    
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
