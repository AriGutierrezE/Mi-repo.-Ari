import json
import threading
import queue
import time
import sys
import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports

DEFAULT_BAUD = 115200

class SerialWorker(threading.Thread):
    def __init__(self, port, baud, out_queue, on_error):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.out_queue = out_queue
        self.on_error = on_error
        self._stop = threading.Event()
        self.ser = None

    def run(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
        except Exception as e:
            self.on_error(f"No se pudo abrir {self.port}: {e}")
            return

        buf = bytearray()
        try:
            while not self._stop.is_set():
                ch = self.ser.read(1)
                if not ch:
                    continue
                if ch in (b"\n", b"\r"):
                    if buf:
                        line = buf.decode(errors="ignore").strip()
                        buf.clear()
                        try:
                            data = json.loads(line)
                            self.out_queue.put(("data", data))
                        except json.JSONDecodeError:
                            # También soporta líneas tipo: {"distance_cm":null,"dur":0}
                            # o logs. Ignora lo que no sea JSON válido.
                            self.out_queue.put(("log", line))
                else:
                    buf.extend(ch)
        except Exception as e:
            self.on_error(f"Error de lectura: {e}")
        finally:
            try:
                if self.ser and self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass

    def stop(self):
        self._stop.set()

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("A4 - Middleware ESP32 (Ultrasónico)")
        self.geometry("560x460")
        self.minsize(560, 460)

        # Estado
        self.worker = None
        self.q = queue.Queue()
        self.connected = False
        self.last_distance = None
        self.samples = 0
        self.start_time = time.time()

        # UI
        self._build_ui()

        # Timer UI
        self.after(50, self._poll_queue)

    # ---------- UI ----------
    def _build_ui(self):
        top = ttk.Frame(self)
        top.pack(fill="x", padx=10, pady=8)

        ttk.Label(top, text="Puerto:").grid(row=0, column=0, padx=(0,6))
        self.cmb_ports = ttk.Combobox(top, width=28, state="readonly", values=self._list_ports())
        self.cmb_ports.grid(row=0, column=1, padx=4)
        ttk.Button(top, text="Refrescar", command=self._refresh_ports).grid(row=0, column=2, padx=4)

        ttk.Label(top, text="Baud:").grid(row=0, column=3, padx=(16,6))
        self.ent_baud = ttk.Entry(top, width=8)
        self.ent_baud.insert(0, str(DEFAULT_BAUD))
        self.ent_baud.grid(row=0, column=4)

        self.btn_connect = ttk.Button(top, text="Conectar", command=self._toggle_connect)
        self.btn_connect.grid(row=0, column=5, padx=(16,0))

        self.lbl_status = ttk.Label(self, text="Estado: Desconectado")
        self.lbl_status.pack(anchor="w", padx=12, pady=(2,0))

        mid = ttk.Frame(self)
        mid.pack(fill="both", expand=True, padx=10, pady=8)

        left = ttk.Frame(mid)
        left.pack(side="left", fill="both", expand=True)

        # Canvas para geometría
        self.canvas = tk.Canvas(left, width=360, height=320, bg="white")
        self.canvas.pack(padx=6, pady=6, fill="both", expand=True)

        # Panel de datos
        right = ttk.Frame(mid, width=160)
        right.pack(side="left", fill="y", padx=8)

        self.lbl_distance = ttk.Label(right, text="Distancia: -- cm", font=("Arial", 14))
        self.lbl_distance.pack(anchor="w", pady=(0,6))

        self.lbl_rate = ttk.Label(right, text="Tasa: 0.0 Hz")
        self.lbl_rate.pack(anchor="w")

        self.txt_log = tk.Text(self, height=6)
        self.txt_log.pack(fill="x", padx=10, pady=(4,8))
        self.txt_log.insert("end", "Logs...\n")
        self.txt_log.configure(state="disabled")

        # Footer
        self.lbl_hint = ttk.Label(self, text="Tip: el ESP32 debe imprimir líneas JSON como  {\"distance_cm\": 42.1}")
        self.lbl_hint.pack(anchor="w", padx=12, pady=(0,8))

    # ---------- Serial control ----------
    def _list_ports(self):
        ports = []
        for p in serial.tools.list_ports.comports():
            ports.append(p.device)
        return ports or ["(sin puertos)"]

    def _refresh_ports(self):
        vals = self._list_ports()
        self.cmb_ports.configure(values=vals)
        if vals and vals[0] != "(sin puertos)":
            self.cmb_ports.set(vals[0])

    def _toggle_connect(self):
        if not self.connected:
            port = self.cmb_ports.get()
            if not port or port == "(sin puertos)":
                messagebox.showwarning("Puertos", "No hay puertos disponibles.")
                return
            try:
                baud = int(self.ent_baud.get())
            except ValueError:
                messagebox.showerror("Baud", "Baud inválido.")
                return

            self.worker = SerialWorker(port, baud, self.q, self._on_error)
            self.worker.start()
            self.connected = True
            self.btn_connect.configure(text="Desconectar")
            self.lbl_status.configure(text=f"Estado: Conectado a {port} @ {baud}")
            self.samples = 0
            self.start_time = time.time()
        else:
            self._disconnect()

    def _disconnect(self):
        if self.worker:
            self.worker.stop()
            self.worker = None
        self.connected = False
        self.btn_connect.configure(text="Conectar")
        self.lbl_status.configure(text="Estado: Desconectado")

    def _on_error(self, msg):
        # Se llama desde el hilo de lectura
        self.q.put(("error", msg))

    # ---------- Data handling & GUI ----------
    def _poll_queue(self):
        try:
            while True:
                kind, payload = self.q.get_nowait()
                if kind == "error":
                    self._append_log(f"[ERROR] {payload}")
                    self._disconnect()
                elif kind == "log":
                    self._append_log(payload)
                elif kind == "data":
                    self._handle_data(payload)
        except queue.Empty:
            pass
        self.after(50, self._poll_queue)

    def _handle_data(self, data):
        # Espera JSON: {"distance_cm": <float|null>}
        d = data.get("distance_cm", None)
        if d is None:
            self.lbl_distance.configure(text="Distancia: -- cm (sin eco)")
            self._draw_circle(5)
        else:
            try:
                d = float(d)
            except (ValueError, TypeError):
                self._append_log(f"[WARN] Valor no numérico: {d}")
                return
            self.last_distance = d
            self.lbl_distance.configure(text=f"Distancia: {d:.2f} cm")
            # Mapa simple: 0..200 cm → radio 5..150 px
            r = max(5, min(150, int(d * 0.75)))
            self._draw_circle(r)

        # tasa
        self.samples += 1
        elapsed = max(1e-6, time.time() - self.start_time)
        rate = self.samples / elapsed
        self.lbl_rate.configure(text=f"Tasa: {rate:.1f} Hz")

    def _draw_circle(self, r):
        self.canvas.delete("all")
        # centrar en el canvas actual
        w = self.canvas.winfo_width() or 360
        h = self.canvas.winfo_height() or 320
        cx, cy = w // 2, h // 2
        self.canvas.create_oval(cx - r, cy - r, cx + r, cy + r, width=2)
        self.canvas.create_line(cx, cy, cx, cy - r, dash=(4, 3))

    def _append_log(self, msg):
        self.txt_log.configure(state="normal")
        self.txt_log.insert("end", msg + "\n")
        self.txt_log.see("end")
        self.txt_log.configure(state="disabled")

    def on_close(self):
        self._disconnect()
        self.destroy()

if __name__ == "__main__":
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()
