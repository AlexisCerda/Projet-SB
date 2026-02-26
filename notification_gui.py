#!/usr/bin/env python3
import sys
import tkinter as tk
from tkinter import scrolledtext
import os
from datetime import datetime
import threading
import time

class NotificationWindow:
    def __init__(self):
        self.home_dir = os.path.expanduser("~")
        self.logfile = os.path.join(self.home_dir, '.virusdetector_log.txt')
        self.lockfile = os.path.join(self.home_dir, '.virusdetector_lock')
        self.commandfile = os.path.join(self.home_dir, '.virusdetector_cmd')
        self.close_timer = None
        self.init_ok = False
        
        # Créer la fenêtre (peut echouer si pas de DISPLAY)
        try:
            self.root = tk.Tk()
        except tk.TclError as exc:
            print(f"[WARN] Interface graphique indisponible: {exc}")
            self.running = False
            return
        self.root.title("VirusDetector - Notifications")
        self.root.geometry("600x400")
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        # Zone de texte avec scrollbar
        self.text = scrolledtext.ScrolledText(self.root, wrap=tk.WORD, font=("Arial", 11))
        self.text.pack(expand=True, fill='both', padx=10, pady=10)
        
        # Bouton pour fermer
        btn = tk.Button(self.root, text="Fermer", command=self.on_closing, font=("Arial", 10))
        btn.pack(pady=5)
        
        # Créer le fichier lock
        with open(self.lockfile, 'w') as f:
            f.write(str(os.getpid()))
        
        # Lancer le thread de surveillance
        self.running = True
        self.watch_thread = threading.Thread(target=self.watch_commands, daemon=True)
        self.watch_thread.start()
        self.init_ok = True
        
    def add_message(self, title, message):
        """Ajoute un message à la fenêtre et au log"""
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        log_entry = f"[{timestamp}] {title}: {message}\n"
        
        # Ajouter au log
        try:
            with open(self.logfile, 'a') as f:
                f.write(log_entry)
        except Exception as e:
            print(f"[WARN] Impossible d'écrire dans le log: {e}")
        
        # Ajouter à la fenêtre
        if not self.init_ok:
            return
        self.text.config(state=tk.NORMAL)
        self.text.insert(tk.END, log_entry)
        self.text.see(tk.END)
        self.text.config(state=tk.DISABLED)
        
        # Annuler le timer de fermeture si actif
        if self.close_timer:
            self.root.after_cancel(self.close_timer)
            self.close_timer = None
    
    def clear_log(self):
        """Efface l'historique"""
        try:
            if os.path.exists(self.logfile):
                os.remove(self.logfile)
            self.text.config(state=tk.NORMAL)
            self.text.delete(1.0, tk.END)
            self.text.config(state=tk.DISABLED)
        except Exception as e:
            print(f"[WARN] Impossible d'effacer le log: {e}")
    
    def schedule_close(self):
        """Programme la fermeture dans 5 secondes"""
        if self.close_timer:
            self.root.after_cancel(self.close_timer)
        self.close_timer = self.root.after(5000, self.on_closing)
    
    def watch_commands(self):
        """Surveille le fichier de commandes pour les nouveaux messages"""
        while self.running:
            try:
                if os.path.exists(self.commandfile):
                    with open(self.commandfile, 'r') as f:
                        lines = f.readlines()
                    
                    for line in lines:
                        line = line.strip()
                        if line.startswith("MSG:"):
                            parts = line[4:].split("|||", 1)
                            if len(parts) == 2:
                                self.add_message(parts[0], parts[1])
                        elif line == "CLEAR":
                            self.clear_log()
                        elif line == "CLOSE":
                            self.schedule_close()
                    
                    # Effacer le fichier de commandes après traitement
                    os.remove(self.commandfile)
            except Exception as e:
                print(f"[WARN] Erreur de surveillance: {e}")
            
            time.sleep(0.5)
    
    def on_closing(self):
        """Fermeture propre"""
        self.running = False
        try:
            if os.path.exists(self.lockfile):
                os.remove(self.lockfile)
            if os.path.exists(self.commandfile):
                os.remove(self.commandfile)
        except:
            pass
        self.root.destroy()
    
    def run(self):
        if hasattr(self, "root"):
            self.root.mainloop()

def send_command(command):
    """Envoie une commande à la fenêtre existante"""
    home_dir = os.path.expanduser("~")
    commandfile = os.path.join(home_dir, '.virusdetector_cmd')
    
    with open(commandfile, 'a') as f:
        f.write(command + '\n')

def window_exists():
    """Vérifie si une fenêtre est déjà ouverte"""
    home_dir = os.path.expanduser("~")
    lockfile = os.path.join(home_dir, '.virusdetector_lock')
    
    if os.path.exists(lockfile):
        try:
            with open(lockfile, 'r') as f:
                pid = int(f.read().strip())
            # Vérifier si le processus existe
            os.kill(pid, 0)
            return True
        except:
            # Le processus n'existe plus, nettoyer le lock
            try:
                os.remove(lockfile)
            except:
                pass
    return False

if __name__ == "__main__":
    if len(sys.argv) >= 2:
        command = sys.argv[1]
        
        if command == "START":
            # Démarrer nouvelle fenêtre et effacer l'historique
            if not window_exists():
                home_dir = os.path.expanduser("~")
                logfile = os.path.join(home_dir, '.virusdetector_log.txt')
                if os.path.exists(logfile):
                    os.remove(logfile)
                
                window = NotificationWindow()
                if len(sys.argv) >= 4:
                    window.add_message(sys.argv[2], " ".join(sys.argv[3:]))
                window.run()
            else:
                # Fenêtre existe, effacer et envoyer message
                send_command("CLEAR")
                if len(sys.argv) >= 4:
                    send_command(f"MSG:{sys.argv[2]}|||{' '.join(sys.argv[3:])}")
        
        elif command == "MSG":
            # Envoyer un message à la fenêtre existante
            if len(sys.argv) >= 4:
                if window_exists():
                    send_command(f"MSG:{sys.argv[2]}|||{' '.join(sys.argv[3:])}")
                else:
                    # Ouvrir une nouvelle fenêtre si elle n'existe pas
                    window = NotificationWindow()
                    window.add_message(sys.argv[2], " ".join(sys.argv[3:]))
                    window.run()
        
        elif command == "CLOSE":
            # Fermer la fenêtre après 5 secondes
            if window_exists():
                send_command("CLOSE")
    else:
        # Par défaut, ouvrir une fenêtre
        window = NotificationWindow()
        window.run()

