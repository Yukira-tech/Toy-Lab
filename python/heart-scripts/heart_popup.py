import tkinter as tk
import random
import math
import time

# ============================================================
# 配置参数（可在此调整效果）
# ============================================================

TOTAL = 250                     # 弹窗总数（建议 200-400）
HEART_SCALE = 20                # 心形整体缩放系数
X_OFFSET = -90                  # 心形水平偏移（负值左移）
Y_OFFSET = -115                 # 心形垂直偏移（负值上移）
WIN_WIDTH, WIN_HEIGHT = 220, 100  # 每个弹窗的尺寸
SLEEP_CREATE = 0.008            # 生成每个弹窗的间隔（秒）
SLEEP_SCATTER = 0.005           # 移动每个弹窗的间隔（秒）

# ------------------------------------------------------------
# 颜色与文字素材（可自由替换）
# ------------------------------------------------------------

# 柔和背景色列表（至少 16 种）
COLORS = [
    "pink", "lightblue", "lightgreen", "lemonchiffon",
    "hotpink", "skyblue", "plum", "peachpuff",
    "lightcyan", "lightyellow", "lightcoral", "lavender",
    "mistyrose", "palegreen", "powderblue", "thistle"
]

# 弹出文字列表（可自由增删）
TEXTS = [
    "❤️ 宝宝辛苦啦", "💪 加油鸭", "🌟 你最棒", "☀️ 今天要开心", "💧 记得多喝水",
    "😎 你是最靓的仔", "🚀 未来可期", "🍀 万事胜意", "📚 天天向上", "🎲 好运连连",
    "✨ 心想事成", "🕊️ 平安喜乐", "🌈 前程似锦", "⏳ 不负韶华", "🌄 以梦为马",
    "⛵ 乘风破浪", "🏃 勇往直前", "🥤 百事可乐", "😄 笑口常开", "🌸 青春无敌",
    "⚡ 元气满满", "💡 光芒万丈", "🌿 温柔坚定", "🎯 全力以赴", "🌹 人间值得",
    "🏆 你很优秀", "🔥 继续努力", "💯 相信自己", "🚴 超越自我", "😊 天天开心",
    "💪 身体健康", "🎉 万事大吉", "📈 学业进步", "🌊 顺风顺水", "🌟 未来可期"
]

# 标题栏文字（已移除 emoji）
TITLE = "爱心小提醒"

# ============================================================
# 核心函数
# ============================================================

def get_seg(t, scale=1.0):
    """
    计算笛卡尔心形曲线上的点坐标。
    参数:
        t: 0~2π 之间的浮点数
        scale: 缩放系数
    返回:
        (x, y) 偏移量（像素），y 已反转使心形朝上
    """
    x = 16 * (math.sin(t) ** 3)
    y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
    return int(x * scale), int(-y * scale)


def is_light_color(color_name):
    """
    判断给定颜色名称是否为浅色。
    用于决定文字颜色（深色背景用白色，浅色背景用深色）。
    """
    # 这里简化处理：除 hotpink 和 plum 外，其余均为浅色
    if color_name in ["hotpink", "plum"]:
        return False
    return True


def make_styled_window(root, x, y):
    """
    创建一个带有白色标题栏和彩色主体内容的无边框窗口。
    参数:
        root: Tk 根窗口
        x, y: 窗口左上角坐标
    返回:
        创建的 Toplevel 窗口对象
    """
    win = tk.Toplevel(root)
    win.overrideredirect(True)          # 移除系统边框
    win.geometry(f"{WIN_WIDTH}x{WIN_HEIGHT}+{x}+{y}")

    # 随机选择背景色
    bg_color = random.choice(COLORS)

    # 创建画布
    canvas = tk.Canvas(win, width=WIN_WIDTH, height=WIN_HEIGHT,
                       highlightthickness=0, bd=0)
    canvas.pack(fill="both", expand=True)

    # ---------- 绘制标题栏 ----------
    TITLE_H = 25
    # 白色标题栏背景
    canvas.create_rectangle(0, 0, WIN_WIDTH, TITLE_H,
                            fill="white", outline="white")
    # 标题栏底部分隔线
    canvas.create_line(0, TITLE_H, WIN_WIDTH, TITLE_H,
                       fill="#555555", width=2)
    # 标题文字
    canvas.create_text(WIN_WIDTH // 2, TITLE_H // 2,
                       text=TITLE,
                       fill="#333333",
                       font=("微软雅黑", 10, "bold"))

    # ---------- 绘制彩色主体 ----------
    CONTENT_H = WIN_HEIGHT - TITLE_H
    # 主体背景
    canvas.create_rectangle(0, TITLE_H, WIN_WIDTH, WIN_HEIGHT,
                            fill=bg_color, outline=bg_color)

    # 根据背景亮度选择文字颜色
    if is_light_color(bg_color):
        text_color = "#333333"      # 浅色背景用深色文字
    else:
        text_color = "#FFFFFF"      # 深色背景用白色文字

    # 文字阴影（偏移 2px）
    canvas.create_text(WIN_WIDTH // 2 + 2, TITLE_H + CONTENT_H // 2 + 2,
                       text=random.choice(TEXTS),
                       fill="#999999",
                       font=("微软雅黑", 14, "bold"))
    # 主文字
    canvas.create_text(WIN_WIDTH // 2, TITLE_H + CONTENT_H // 2,
                       text=random.choice(TEXTS),
                       fill=text_color,
                       font=("微软雅黑", 14, "bold"))

    # ---------- 整体边框（浅灰色） ----------
    canvas.create_rectangle(1, 1, WIN_WIDTH - 1, WIN_HEIGHT - 1,
                            outline="#CCCCCC", width=1)

    return win


# ============================================================
# 主程序入口
# ============================================================

def main():
    # 隐藏根窗口
    root = tk.Tk()
    root.withdraw()

    # 获取屏幕尺寸
    screen_w = root.winfo_screenwidth()
    screen_h = root.winfo_screenheight()

    # 心形基准点（屏幕中心 + 偏移量）
    center_x = screen_w // 2 + X_OFFSET
    center_y = screen_h // 2 + Y_OFFSET

    # ---------- 生成所有窗口位置（心形坐标） ----------
    positions = []
    for i in range(TOTAL):
        t = (i / TOTAL) * 2 * math.pi
        dx, dy = get_seg(t, HEART_SCALE)
        positions.append((center_x + dx, center_y + dy))

    windows = []
    print(f"[*] 正在生成 {TOTAL} 个爱心弹窗...")

    # 逐个创建窗口
    for x, y in positions:
        win = make_styled_window(root, x, y)
        windows.append(win)
        root.update()
        time.sleep(SLEEP_CREATE)

    print("[+] 心形排布完成，开始逐个移动铺满屏幕...")

    # ---------- 逐个移动窗口到随机位置 ----------
    # 预计算所有目标位置（避免移动过程中随机计算延迟）
    targets = []
    for _ in range(TOTAL):
        rand_x = random.randint(0, max(0, screen_w - WIN_WIDTH))
        rand_y = random.randint(0, max(0, screen_h - WIN_HEIGHT))
        targets.append((rand_x, rand_y))

    for idx, win in enumerate(windows):
        rand_x, rand_y = targets[idx]
        win.geometry(f"+{rand_x}+{rand_y}")    # 移动窗口
        root.update()
        # 最后一个窗口移动后不等待，直接完成
        if idx != len(windows) - 1:
            time.sleep(SLEEP_SCATTER)

    print("[+] 铺满完成，3 秒后自动关闭...")
    time.sleep(3)
    root.destroy()


if __name__ == "__main__":
    main()
