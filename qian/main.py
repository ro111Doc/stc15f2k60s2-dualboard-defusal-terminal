import tkinter as tk
from tkinter import ttk
import queue
import threading

import serial
import serial.tools.list_ports


class DefusalTerminal:
    def __init__(self, root):
        self.root = root

        self.root.title("双板拆弹任务终端")
        self.root.geometry("940x700")
        self.root.resizable(False, False)
        self.root.configure(bg="#071012")

        self.current_stage = 1
        self.preview_stage = 1
        self.difficulty = 1
        self.remaining_time = 60

        self.game_started = False

        self.serial_port = None
        self.serial_thread = None
        self.serial_running = False

        self.message_queue = queue.Queue()

        self.result_window = None

        self.stages = [
            {
                "title": "光敏线路解除",
                "desc": "遮挡 A 板光敏传感器，并持续保持约 5 秒，完成第一道安全线路解除。"
            },
            {
                "title": "双板情报通信",
                "desc": "A 板向 B 板发送情报请求。请在 B 板按下 K1，获取并记住随机生成的四位密钥。"
            },
            {
                "title": "磁场保险解除",
                "desc": "使用磁铁靠近 A 板霍尔传感器，解除装置的磁场保险。"
            },
            {
                "title": "安全密钥验证",
                "desc": "使用五向导航键输入 B 板提供的四位密钥。K3 可查看一位提示，但会扣除 10 秒。"
            },
            {
                "title": "随机信号反应",
                "desc": "按下 K1 启动最终挑战。等待灯光亮起并听到蜂鸣提示后，立即再次按下 K1。提前抢按将直接失败。"
            }
        ]

        self.build_ui()

        self.update_time()
        self.update_difficulty()
        self.update_preview_stage()

        self.refresh_ports()

        self.root.after(
            80,
            self.process_serial_messages
        )

        self.root.protocol(
            "WM_DELETE_WINDOW",
            self.on_close
        )

    def build_ui(self):
        top = tk.Frame(
            self.root,
            bg="#071012"
        )
        top.pack(
            fill="x",
            padx=35,
            pady=(22, 8)
        )

        left = tk.Frame(
            top,
            bg="#071012"
        )
        left.pack(side="left")

        self.status_dot = tk.Label(
            left,
            text="●",
            fg="#75827e",
            bg="#071012",
            font=("Microsoft YaHei", 12)
        )
        self.status_dot.pack(side="left")

        self.device_status = tk.Label(
            left,
            text="设备未连接",
            fg="#7c918a",
            bg="#071012",
            font=("Microsoft YaHei", 10)
        )
        self.device_status.pack(
            side="left",
            padx=8
        )

        tk.Label(
            top,
            text="双板拆弹任务终端",
            fg="#e5f4ef",
            bg="#071012",
            font=("Microsoft YaHei", 16, "bold")
        ).pack(
            side="left",
            expand=True
        )

        tk.Label(
            top,
            text="A / B SYSTEM",
            fg="#66827a",
            bg="#071012",
            font=("Consolas", 10)
        ).pack(side="right")

        connect_frame = tk.Frame(
            self.root,
            bg="#071012"
        )
        connect_frame.pack(
            pady=(4, 12)
        )

        self.port_combo = ttk.Combobox(
            connect_frame,
            width=27,
            state="readonly"
        )
        self.port_combo.pack(
            side="left",
            padx=5
        )

        self.make_button(
            connect_frame,
            "刷新串口",
            self.refresh_ports
        ).pack(
            side="left",
            padx=5
        )

        self.connect_button = tk.Button(
            connect_frame,
            text="连接 A 板",
            command=self.toggle_serial,
            fg="#071012",
            bg="#55d8a7",
            activeforeground="#071012",
            activebackground="#72e9bb",
            relief="flat",
            bd=0,
            padx=18,
            pady=6,
            cursor="hand2",
            font=("Microsoft YaHei", 9, "bold")
        )
        self.connect_button.pack(
            side="left",
            padx=5
        )

        tk.Label(
            self.root,
            text="REMAINING TIME",
            fg="#68877e",
            bg="#071012",
            font=("Consolas", 10)
        ).pack(
            pady=(10, 0)
        )

        self.time_label = tk.Label(
            self.root,
            text="60",
            fg="#f1fff9",
            bg="#071012",
            font=("Consolas", 64, "bold")
        )
        self.time_label.pack()

        self.difficulty_label = tk.Label(
            self.root,
            text="EASY · 简单",
            fg="#91b8ac",
            bg="#10201d",
            font=("Microsoft YaHei", 10),
            padx=18,
            pady=6
        )
        self.difficulty_label.pack(
            pady=8
        )

        self.progress_frame = tk.Frame(
            self.root,
            bg="#071012"
        )
        self.progress_frame.pack(
            pady=20
        )

        self.stage_labels = []
        self.line_labels = []

        for i in range(5):
            circle = tk.Label(
                self.progress_frame,
                text=str(i + 1),
                width=3,
                fg="#58746b",
                bg="#10201d",
                font=("Consolas", 13, "bold")
            )

            circle.pack(
                side="left",
                padx=6
            )

            self.stage_labels.append(circle)

            if i < 4:
                line = tk.Label(
                    self.progress_frame,
                    text="────",
                    fg="#274039",
                    bg="#071012",
                    font=("Consolas", 12)
                )

                line.pack(side="left")

                self.line_labels.append(line)

        task_frame = tk.Frame(
            self.root,
            bg="#0c1718",
            highlightbackground="#18322d",
            highlightthickness=1
        )

        task_frame.pack(
            padx=120,
            pady=10,
            fill="x"
        )

        self.stage_number_label = tk.Label(
            task_frame,
            text="STAGE 01",
            fg="#54746a",
            bg="#0c1718",
            font=("Consolas", 10)
        )
        self.stage_number_label.pack(
            pady=(18, 5)
        )

        self.stage_title_label = tk.Label(
            task_frame,
            text="光敏线路解除",
            fg="#e9f5f1",
            bg="#0c1718",
            font=("Microsoft YaHei", 20, "bold")
        )
        self.stage_title_label.pack()

        self.stage_desc_label = tk.Label(
            task_frame,
            text="",
            fg="#839f96",
            bg="#0c1718",
            font=("Microsoft YaHei", 11),
            wraplength=620,
            justify="center"
        )
        self.stage_desc_label.pack(
            padx=30,
            pady=(12, 22)
        )

        self.nav_frame = tk.Frame(
            self.root,
            bg="#071012"
        )
        self.nav_frame.pack(
            pady=8
        )

        self.prev_button = self.make_button(
            self.nav_frame,
            "← 上一关",
            self.previous_preview
        )
        self.prev_button.pack(
            side="left",
            padx=10
        )

        self.wait_label = tk.Label(
            self.nav_frame,
            text="连接 A 板后，按 A 板 K1 开始任务",
            fg="#6c8d83",
            bg="#071012",
            width=32,
            font=("Microsoft YaHei", 10)
        )
        self.wait_label.pack(
            side="left",
            padx=20
        )

        self.next_button = self.make_button(
            self.nav_frame,
            "下一关 →",
            self.next_preview
        )
        self.next_button.pack(
            side="left",
            padx=10
        )

        bottom = tk.Frame(
            self.root,
            bg="#071012"
        )
        bottom.pack(
            fill="x",
            padx=35,
            pady=20
        )

        tk.Label(
            bottom,
            text="STC-B · COOPERATIVE DEFUSAL SYSTEM",
            fg="#45645b",
            bg="#071012",
            font=("Consolas", 9)
        ).pack(side="left")

        self.raw_label = tk.Label(
            bottom,
            text="SERIAL: WAITING",
            fg="#3e5a52",
            bg="#071012",
            font=("Consolas", 9)
        )
        self.raw_label.pack(side="right")

    def make_button(self, parent, text, command):
        return tk.Button(
            parent,
            text=text,
            command=command,
            fg="#9bb9b0",
            bg="#10201d",
            activeforeground="white",
            activebackground="#19332d",
            relief="flat",
            bd=0,
            padx=14,
            pady=7,
            cursor="hand2"
        )

    def refresh_ports(self):
        ports = list(
            serial.tools.list_ports.comports()
        )

        values = [
            f"{p.device} - {p.description}"
            for p in ports
        ]

        self.port_combo["values"] = values

        if values:
            self.port_combo.current(0)
        else:
            self.port_combo.set(
                "未发现串口"
            )

    def toggle_serial(self):
        if (
            self.serial_port
            and self.serial_port.is_open
        ):
            self.disconnect_serial()
        else:
            self.connect_serial()

    def connect_serial(self):
        selected = self.port_combo.get()

        if (
            not selected
            or selected == "未发现串口"
        ):
            self.device_status.config(
                text="未发现可用串口",
                fg="#ff6962"
            )
            return

        port_name = selected.split(" - ")[0]

        try:
            self.serial_port = serial.Serial(
                port=port_name,
                baudrate=9600,
                bytesize=8,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.2
            )

            self.serial_running = True

            self.serial_thread = threading.Thread(
                target=self.serial_reader,
                daemon=True
            )

            self.serial_thread.start()

            self.status_dot.config(
                fg="#55e2aa"
            )

            self.device_status.config(
                text=f"已连接 {port_name}",
                fg="#9bb7ae"
            )

            self.connect_button.config(
                text="断开 A 板",
                bg="#274039",
                fg="#d6e5e0"
            )

            self.wait_label.config(
                text="设备已连接，等待 A 板 K1 开始"
            )

        except Exception as error:
            self.device_status.config(
                text=f"连接失败：{error}",
                fg="#ff6962"
            )

    def disconnect_serial(self):
        self.serial_running = False

        if self.serial_port:
            try:
                self.serial_port.close()
            except Exception:
                pass

        self.serial_port = None

        self.status_dot.config(
            fg="#75827e"
        )

        self.device_status.config(
            text="设备未连接",
            fg="#7c918a"
        )

        self.connect_button.config(
            text="连接 A 板",
            bg="#55d8a7",
            fg="#071012"
        )

        self.wait_label.config(
            text="连接 A 板后，按 A 板 K1 开始任务"
        )

    def serial_reader(self):
        buffer = ""

        while self.serial_running:
            try:
                if not self.serial_port:
                    break

                chunk = self.serial_port.read(
                    self.serial_port.in_waiting or 1
                )

                if not chunk:
                    continue

                text = chunk.decode(
                    "ascii",
                    errors="ignore"
                )

                buffer += text

                while "\n" in buffer:
                    line, buffer = buffer.split(
                        "\n",
                        1
                    )

                    line = line.strip()

                    if line:
                        self.message_queue.put(
                            line
                        )

            except Exception:
                self.message_queue.put(
                    "__DISCONNECTED__"
                )
                break

    def process_serial_messages(self):
        try:
            while True:
                message = (
                    self.message_queue.get_nowait()
                )

                self.handle_serial_message(
                    message
                )

        except queue.Empty:
            pass

        self.root.after(
            80,
            self.process_serial_messages
        )

    def handle_serial_message(self, message):
        if message == "__DISCONNECTED__":
            self.disconnect_serial()
            return

        message = message.strip()

        self.raw_label.config(
            text=f"SERIAL: {message}"
        )

        if message == "RESET":
            self.reset_to_initial()

        elif message.startswith("START:"):
            try:
                parts = message.split(":")

                difficulty = int(parts[1])
                game_time = int(parts[2])
                stage = int(parts[3])

                self.start_game(
                    difficulty,
                    game_time,
                    stage
                )

            except (
                ValueError,
                IndexError
            ):
                pass

        elif message.startswith("DIFF:"):
            try:
                value = int(
                    message.split(":")[1]
                )

                self.set_difficulty(
                    value
                )

            except ValueError:
                pass

        elif message.startswith("STAGE:"):
            try:
                value = int(
                    message.split(":")[1]
                )

                self.set_stage(
                    value
                )

            except ValueError:
                pass

        elif message.startswith("TIME:"):
            try:
                value = int(
                    message.split(":")[1]
                )

                self.set_time(
                    value
                )

            except ValueError:
                pass

        elif message == "WIN":
            self.show_success()

        elif message == "LO5E":
            self.show_fail()

    def reset_to_initial(self):
        self.close_result_window()

        self.game_started = False

        self.current_stage = 1
        self.preview_stage = 1

        self.remaining_time = 60
        self.difficulty = 1

        self.update_time()
        self.update_difficulty()
        self.update_preview_stage()

        self.raw_label.config(
            text="SERIAL: RESET"
        )

        if (
            self.serial_port
            and self.serial_port.is_open
        ):
            self.status_dot.config(
                fg="#55e2aa"
            )

            self.device_status.config(
                text="设备已连接",
                fg="#9bb7ae"
            )

            self.wait_label.config(
                text="等待 A 板 K1 开始任务"
            )

        else:
            self.status_dot.config(
                fg="#75827e"
            )

            self.device_status.config(
                text="设备未连接",
                fg="#7c918a"
            )

            self.wait_label.config(
                text="连接 A 板后，按 A 板 K1 开始任务"
            )

    def start_game(
        self,
        difficulty,
        game_time,
        stage
    ):
        self.close_result_window()

        self.game_started = True

        self.difficulty = difficulty
        self.remaining_time = game_time
        self.current_stage = stage

        self.update_difficulty()
        self.update_time()
        self.update_active_stage()

        self.prev_button.config(
            state="disabled"
        )

        self.next_button.config(
            state="disabled"
        )

        self.wait_label.config(
            text="任务进行中"
        )

        self.device_status.config(
            text="任务进行中",
            fg="#55e2aa"
        )

    def set_time(self, value):
        if value < 0:
            value = 0

        self.remaining_time = value

        self.update_time()

    def update_time(self):
        self.time_label.config(
            text=str(self.remaining_time)
        )

        if self.remaining_time <= 10:
            self.time_label.config(
                fg="#ff6962"
            )
        else:
            self.time_label.config(
                fg="#f1fff9"
            )

    def set_difficulty(self, value):
        if 1 <= value <= 3:
            self.difficulty = value
            self.update_difficulty()

    def update_difficulty(self):
        if self.difficulty == 1:
            text = "EASY · 简单"

        elif self.difficulty == 2:
            text = "NORMAL · 一般"

        else:
            text = "HARD · 困难"

        self.difficulty_label.config(
            text=text
        )

    def set_stage(self, stage):
        if not 1 <= stage <= 5:
            return

        self.game_started = True

        self.current_stage = stage

        self.prev_button.config(
            state="disabled"
        )

        self.next_button.config(
            state="disabled"
        )

        self.wait_label.config(
            text=f"当前任务：第 {stage} 关"
        )

        self.update_active_stage()

    def update_active_stage(self):
        stage = self.stages[
            self.current_stage - 1
        ]

        self.stage_number_label.config(
            text=f"STAGE 0{self.current_stage}"
        )

        self.stage_title_label.config(
            text=stage["title"]
        )

        self.stage_desc_label.config(
            text=stage["desc"]
        )

        self.update_progress(
            self.current_stage,
            False
        )

    def update_preview_stage(self):
        stage = self.stages[
            self.preview_stage - 1
        ]

        self.stage_number_label.config(
            text=f"STAGE 0{self.preview_stage}"
        )

        self.stage_title_label.config(
            text=stage["title"]
        )

        self.stage_desc_label.config(
            text=stage["desc"]
        )

        self.update_progress(
            self.preview_stage,
            True
        )

        self.prev_button.config(
            state=(
                "normal"
                if self.preview_stage > 1
                else "disabled"
            )
        )

        self.next_button.config(
            state=(
                "normal"
                if self.preview_stage < 5
                else "disabled"
            )
        )

    def update_progress(
        self,
        stage,
        preview
    ):
        for i, label in enumerate(
            self.stage_labels
        ):
            number = i + 1

            if preview:
                if number == stage:
                    label.config(
                        fg="#69e8b6",
                        bg="#17352e"
                    )
                else:
                    label.config(
                        fg="#58746b",
                        bg="#10201d"
                    )

            else:
                if number < stage:
                    label.config(
                        fg="#06120e",
                        bg="#55d8a7"
                    )

                elif number == stage:
                    label.config(
                        fg="#69e8b6",
                        bg="#17352e"
                    )

                else:
                    label.config(
                        fg="#58746b",
                        bg="#10201d"
                    )

        for i, line in enumerate(
            self.line_labels
        ):
            if (
                not preview
                and i + 1 < stage
            ):
                line.config(
                    fg="#4cb88f"
                )
            else:
                line.config(
                    fg="#274039"
                )

    def previous_preview(self):
        if self.game_started:
            return

        if self.preview_stage > 1:
            self.preview_stage -= 1

            self.update_preview_stage()

    def next_preview(self):
        if self.game_started:
            return

        if self.preview_stage < 5:
            self.preview_stage += 1

            self.update_preview_stage()

    def get_success_message(self):
        if self.remaining_time >= 40:
            return "神级操作！行云流水，简直把炸弹当玩具拆！💐"

        elif self.remaining_time >= 25:
            return "漂亮！全程节奏炸裂，冷静得像在拆闹钟！🎆"

        elif self.remaining_time >= 10:
            return "牛！高压之下纹丝不乱，这波稳如泰山！🐮"

        else:
            return "绝地翻盘！读秒阶段一刀断线，心脏差点炸出胸腔——你就是传奇！💣"

    def show_success(self):
        self.game_started = False

        self.device_status.config(
            text="任务完成",
            fg="#55e2aa"
        )

        self.wait_label.config(
            text="拆弹成功，请按 A 板复位键"
        )

        success_message = self.get_success_message()

        result_text = (
            f"剩余时间：{self.remaining_time} 秒\n"
            f"{success_message}"
        )

        self.show_result(
            title="MISSION COMPLETE",
            main_text="双板协同拆弹成功",
            color="#64e8b7",
            sub_text=result_text
        )

    def show_fail(self):
        self.game_started = False

        self.device_status.config(
            text="任务失败",
            fg="#ff6962"
        )

        self.wait_label.config(
            text="任务失败，请按 A 板复位键"
        )

        self.show_result(
            title="MISSION FAILED",
            main_text="LOSE",
            color="#ff6962",
            sub_text="请按 A 板复位键准备下一轮"
        )

    def show_result(
        self,
        title,
        main_text,
        color,
        sub_text=""
    ):
        self.close_result_window()

        window = tk.Toplevel(
            self.root
        )

        self.result_window = window

        window.title(title)

        window.geometry(
            "640x410"
        )

        window.resizable(
            False,
            False
        )

        window.configure(
            bg="#071012"
        )

        window.transient(
            self.root
        )

        window.grab_set()

        tk.Label(
            window,
            text=title,
            fg=color,
            bg="#071012",
            font=(
                "Consolas",
                16,
                "bold"
            )
        ).pack(
            pady=(70, 18)
        )

        if title == "MISSION COMPLETE":
            main_font = (
                "Microsoft YaHei",
                27,
                "bold"
            )
        else:
            main_font = (
                "Consolas",
                42,
                "bold"
            )

        tk.Label(
            window,
            text=main_text,
            fg=color,
            bg="#071012",
            font=main_font
        ).pack()

        if sub_text:
            tk.Label(
                window,
                text=sub_text,
                fg="#dbeae5",
                bg="#071012",
                font=(
                    "Microsoft YaHei",
                    13
                ),
                justify="center"
            ).pack(
                pady=20
            )

        tk.Label(
            window,
            text="请按 A 板复位键准备下一轮",
            fg="#66827a",
            bg="#071012",
            font=(
                "Microsoft YaHei",
                10
            )
        ).pack(
            pady=10
        )

        tk.Label(
            window,
            text="等待 A 板复位...",
            fg="#4e6c63",
            bg="#071012",
            font=(
                "Consolas",
                10
            )
        ).pack(
            pady=5
        )

    def close_result_window(self):
        if (
            self.result_window
            and self.result_window.winfo_exists()
        ):
            try:
                self.result_window.grab_release()
            except Exception:
                pass

            self.result_window.destroy()

        self.result_window = None

    def on_close(self):
        self.serial_running = False

        if self.serial_port:
            try:
                self.serial_port.close()
            except Exception:
                pass

        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()

    app = DefusalTerminal(root)

    root.mainloop()