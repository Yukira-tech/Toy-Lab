import tkinter as tk
from tkinter import ttk, messagebox
import json
import os
import time
import random

# ====================================================================
# 1. 配置文件与初始化
# ====================================================================

USER_FILE = "users.json"          # 用户信息存储文件（JSON格式）
BILL_FILE = "bills.jsonl"         # 账单存储文件（JSONL格式，每行一条记录）

def init_file():
    """
    初始化数据文件，如果不存在则创建。
    - users.json 写入默认空用户结构
    - bills.jsonl 创建空文件
    """
    if not os.path.exists(USER_FILE):
        with open(USER_FILE, "w", encoding="utf-8") as f:
            json.dump({"account": "", "password": ""}, f, ensure_ascii=False, indent=4)
    if not os.path.exists(BILL_FILE):
        open(BILL_FILE, "w", encoding="utf-8").close()

def load_user():
    """加载用户信息，若文件损坏或不存在则返回空字典"""
    try:
        with open(USER_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {"account": "", "password": ""}

def save_user(account, password):
    """保存用户账号密码到 users.json"""
    data = {"account": account, "password": password}
    with open(USER_FILE, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=4)

def load_all_bills():
    """
    加载 bills.jsonl 中的所有账单记录。
    若某条记录没有 'id' 字段，则自动生成一个临时 ID（基于时间戳+随机数），
    确保后续删除操作有依据。
    返回账单列表（每条为字典）。
    """
    lst = []
    if not os.path.exists(BILL_FILE):
        return lst
    try:
        with open(BILL_FILE, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                bill = json.loads(line)
                # 兼容旧数据：自动补 id
                if "id" not in bill:
                    bill["id"] = int(time.time() * 1000) + random.randint(0, 999)
                lst.append(bill)
    except Exception:
        pass
    return lst

def append_bill_record(item):
    """
    新增一条账单记录，自动生成唯一 ID，并以 JSONL 格式追加到文件末尾。
    """
    item["id"] = int(time.time() * 1000) + random.randint(0, 999)
    with open(BILL_FILE, mode="a", encoding="utf-8") as fp:
        line_text = json.dumps(item, ensure_ascii=False)
        fp.write(line_text + "\n")

def rewrite_all_bills(bills):
    """
    用新的账单列表覆盖整个 bills.jsonl 文件。
    此操作会重写所有记录，用于删除或批量修改。
    """
    with open(BILL_FILE, "w", encoding="utf-8") as f:
        for bill in bills:
            f.write(json.dumps(bill, ensure_ascii=False) + "\n")

# 程序启动时初始化文件
init_file()


# ====================================================================
# 2. 登录页面
# ====================================================================

class LoginPage:
    """登录界面，包含账号、密码输入及显示密码复选框"""
    def __init__(self, root, switch_register, enter_main):
        """
        参数:
            root: Tk根窗口
            switch_register: 回调函数，切换到注册页面
            enter_main: 回调函数，登录成功后进入主页面
        """
        self.root = root
        self.switch_register = switch_register
        self.enter_main = enter_main
        self.root.title("登录")
        self.root.geometry("600x500")
        self.root.resizable(False, False)

        # 主框架
        self.frame = ttk.Frame(root, padding=80)
        self.frame.pack(fill=tk.BOTH, expand=True)

        # 标题
        ttk.Label(self.frame, text="用户登录", font=("SimHei",22)).pack(pady=(20,40))

        # 账号输入
        ttk.Label(self.frame, text="账号", font=("SimHei",12)).pack(anchor="w")
        self.account_var = tk.StringVar()
        ttk.Entry(self.frame, textvariable=self.account_var, font=("SimHei",12)).pack(fill=tk.X, pady=(0,18))

        # 密码输入
        ttk.Label(self.frame, text="密码", font=("SimHei",12)).pack(anchor="w")
        self.pwd_var = tk.StringVar()
        self.pwd_entry = ttk.Entry(self.frame, textvariable=self.pwd_var, show="*", font=("SimHei",12))
        self.pwd_entry.pack(fill=tk.X, pady=(0,5))

        # 显示密码复选框
        self.show_pwd_var = tk.BooleanVar(value=False)
        chk = ttk.Checkbutton(self.frame, text="显示密码", variable=self.show_pwd_var,
                              command=self.toggle_pwd_show)
        chk.pack(anchor="w", pady=(0,30))

        # 按钮行：登录和去注册
        btn_frame = ttk.Frame(self.frame)
        btn_frame.pack(fill=tk.X)
        ttk.Button(btn_frame, text="登录", command=self.login).pack(side=tk.RIGHT, padx=(10,0))
        ttk.Button(btn_frame, text="去注册", command=self.switch_register).pack(side=tk.RIGHT)

    def toggle_pwd_show(self):
        """切换密码显示/隐藏"""
        if self.show_pwd_var.get():
            self.pwd_entry.config(show="")
        else:
            self.pwd_entry.config(show="*")

    def login(self):
        """登录验证：检查账号密码是否匹配存储的用户信息"""
        acc = self.account_var.get().strip()
        pwd = self.pwd_var.get().strip()
        if not acc or not pwd:
            messagebox.showwarning("提示","账号密码不能为空")
            return
        user = load_user()
        if user["account"] == acc and user["password"] == pwd:
            messagebox.showinfo("成功","登录成功")
            self.enter_main()       # 切换到主页面
        else:
            messagebox.showerror("错误","账号或者密码错误")

    def destroy(self):
        """销毁页面组件（切换到其他页面时调用）"""
        self.frame.destroy()


# ====================================================================
# 3. 注册页面
# ====================================================================

class RegisterPage:
    """用户注册界面，包含账号、密码、确认密码及显示密码复选框"""
    def __init__(self, root, switch_login):
        self.root = root
        self.switch_login = switch_login
        self.root.title("注册")
        self.root.geometry("600x500")
        self.root.resizable(False, False)

        self.frame = ttk.Frame(root, padding=80)
        self.frame.pack(fill=tk.BOTH, expand=True)

        ttk.Label(self.frame, text="用户注册", font=("SimHei",22)).pack(pady=(20,40))

        # 账号
        ttk.Label(self.frame, text="账号", font=("SimHei",12)).pack(anchor="w")
        self.account_var = tk.StringVar()
        ttk.Entry(self.frame, textvariable=self.account_var, font=("SimHei",12)).pack(fill=tk.X, pady=(0,18))

        # 密码
        ttk.Label(self.frame, text="密码", font=("SimHei",12)).pack(anchor="w")
        self.pwd_var = tk.StringVar()
        self.pwd_entry = ttk.Entry(self.frame, textvariable=self.pwd_var, show="*", font=("SimHei",12))
        self.pwd_entry.pack(fill=tk.X, pady=(0,5))

        # 确认密码
        ttk.Label(self.frame, text="确认密码", font=("SimHei",12)).pack(anchor="w")
        self.pwd2_var = tk.StringVar()
        self.pwd2_entry = ttk.Entry(self.frame, textvariable=self.pwd2_var, show="*", font=("SimHei",12))
        self.pwd2_entry.pack(fill=tk.X, pady=(0,5))

        # 显示密码（同时控制两个输入框）
        self.show_pwd_var = tk.BooleanVar(value=False)
        chk = ttk.Checkbutton(self.frame, text="显示密码", variable=self.show_pwd_var,
                              command=self.toggle_pwd_show)
        chk.pack(anchor="w", pady=(0,30))

        btn_frame = ttk.Frame(self.frame)
        btn_frame.pack(fill=tk.X)
        ttk.Button(btn_frame, text="注册", command=self.reg).pack(side=tk.RIGHT, padx=(10,0))
        ttk.Button(btn_frame, text="去登录", command=self.switch_login).pack(side=tk.RIGHT)

    def toggle_pwd_show(self):
        """同时切换密码和确认密码的显示/隐藏"""
        if self.show_pwd_var.get():
            self.pwd_entry.config(show="")
            self.pwd2_entry.config(show="")
        else:
            self.pwd_entry.config(show="*")
            self.pwd2_entry.config(show="*")

    def reg(self):
        """
        注册逻辑：
        1. 校验非空
        2. 检查两次密码是否一致
        3. 检查是否已存在注册用户（单用户模式）
        4. 保存用户信息，提示成功并切换到登录
        """
        acc = self.account_var.get().strip()
        p1 = self.pwd_var.get().strip()
        p2 = self.pwd2_var.get().strip()

        if not acc or not p1:
            messagebox.showwarning("提示","账号密码不能为空")
            return
        if p1 != p2:
            messagebox.showerror("错误","两次密码不一致")
            return

        current_user = load_user()
        if current_user["account"] != "":
            messagebox.showwarning("提示","已经存在注册账号，本程序仅支持单个用户")
            return

        save_user(acc, p1)
        messagebox.showinfo("成功","注册完成，请去登录")
        self.switch_login()   # 返回登录页面

    def destroy(self):
        self.frame.destroy()


# ====================================================================
# 4. 主页面（账单管理）
# ====================================================================

class BillMainPage:
    """账单管理主界面，包含筛选、添加、删除及表格展示"""
    def __init__(self, root):
        self.root = root
        self.root.title("账单管理主页")
        self.root.geometry("600x550")
        self.root.resizable(False, False)

        # 主框架
        self.frame = ttk.Frame(root)
        self.frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # ---------- 筛选栏 ----------
        filter_frame = ttk.Frame(self.frame)
        filter_frame.pack(fill=tk.X, pady=(0,10))
        ttk.Label(filter_frame, text="筛选种类：").pack(side=tk.LEFT)
        self.filter_var = tk.StringVar(value="全部")
        cb = ttk.Combobox(filter_frame, textvariable=self.filter_var, values=["全部","支出","收入"], state="readonly")
        cb.pack(side=tk.LEFT, padx=5)
        ttk.Button(filter_frame, text="应用筛选", command=self.refresh_table).pack(side=tk.LEFT, padx=5)

        # ---------- 新增账单区域 ----------
        add_frame = ttk.LabelFrame(self.frame, text="新增账单")
        add_frame.pack(fill=tk.X, pady=(0,10))

        # 类型
        ttk.Label(add_frame, text="类型").grid(row=0,column=0,padx=4,pady=6)
        self.bill_type_var = tk.StringVar(value="支出")
        ttk.Combobox(add_frame, textvariable=self.bill_type_var, values=["支出","收入"], state="readonly").grid(row=0,column=1,padx=4,pady=6)

        # 名称
        ttk.Label(add_frame, text="名称").grid(row=0,column=2,padx=4,pady=6)
        self.name_var = tk.StringVar()
        ttk.Entry(add_frame, textvariable=self.name_var, width=12).grid(row=0,column=3,padx=4,pady=6)

        # 价格
        ttk.Label(add_frame, text="价格").grid(row=0,column=4,padx=4,pady=6)
        self.price_var = tk.StringVar()
        ttk.Entry(add_frame, textvariable=self.price_var, width=10).grid(row=0,column=5,padx=4,pady=6)

        # 备注
        ttk.Label(add_frame, text="备注tip").grid(row=1,column=0,padx=4,pady=6)
        self.tip_var = tk.StringVar()
        ttk.Entry(add_frame, textvariable=self.tip_var, width=30).grid(row=1,column=1,columnspan=4,padx=4,pady=6)

        ttk.Button(add_frame, text="添加账单", command=self.add_bill).grid(row=1,column=5,padx=8,pady=6)

        # ---------- 操作工具栏 ----------
        toolbar = ttk.Frame(self.frame)
        toolbar.pack(fill=tk.X, pady=(0,5))
        ttk.Button(toolbar, text="删除选中账单", command=self.delete_selected).pack(side=tk.LEFT)
        ttk.Button(toolbar, text="刷新列表", command=self.refresh_table).pack(side=tk.LEFT, padx=5)

        # ---------- 表格（Treeview） ----------
        columns = ("type","name","price","tip")
        self.tree = ttk.Treeview(self.frame, columns=columns, show="headings", height=11)
        self.tree.heading("type", text="类型")
        self.tree.heading("name", text="名称")
        self.tree.heading("price", text="价格")
        self.tree.heading("tip", text="备注tip")
        self.tree.column("type", width=80)
        self.tree.column("name", width=130)
        self.tree.column("price", width=110)
        self.tree.column("tip", width=220)

        # 滚动条
        scroll = ttk.Scrollbar(self.frame, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)

        # 当前显示的记录缓存（用于删除）
        self.current_bills = []
        self.refresh_table()

    def add_bill(self):
        """
        添加账单：从输入框取值，校验非空后写入文件，然后刷新表格。
        """
        name = self.name_var.get().strip()
        price = self.price_var.get().strip()
        tip = self.tip_var.get().strip()
        bill_type = self.bill_type_var.get()
        if not name or not price:
            messagebox.showwarning("提示","名称和价格不能为空")
            return
        new_item = {
            "type": bill_type,
            "name": name,
            "price": price,
            "tip": tip
        }
        append_bill_record(new_item)
        # 清空输入框
        self.name_var.set("")
        self.price_var.set("")
        self.tip_var.set("")
        self.refresh_table()

    def refresh_table(self):
        """
        根据当前筛选条件刷新表格显示。
        加载所有账单，按类型过滤，然后插入 Treeview。
        每行记录附带其 'id' 作为 tags，便于删除时定位。
        """
        # 清空表格
        for row in self.tree.get_children():
            self.tree.delete(row)

        all_records = load_all_bills()
        filter_kind = self.filter_var.get()
        filtered = []
        for bill in all_records:
            if filter_kind != "全部" and bill["type"] != filter_kind:
                continue
            filtered.append(bill)

        self.current_bills = filtered

        for bill in filtered:
            self.tree.insert("", tk.END,
                             values=(bill["type"], bill["name"], bill["price"], bill["tip"]),
                             tags=(str(bill["id"]),))

    def delete_selected(self):
        """
        删除选中的账单：
        1. 获取选中的行及关联的 ID。
        2. 从所有账单中过滤掉该 ID。
        3. 重写文件。
        4. 刷新表格。
        """
        selected = self.tree.selection()
        if not selected:
            messagebox.showwarning("提示", "请先选中一条要删除的账单")
            return

        item = selected[0]
        tags = self.tree.item(item, "tags")
        if not tags:
            messagebox.showerror("错误", "无法获取该条记录的ID，请刷新后重试")
            return
        record_id = int(tags[0])

        all_records = load_all_bills()
        new_records = []
        found = False
        for bill in all_records:
            if bill["id"] == record_id:
                found = True
                continue
            new_records.append(bill)
        if not found:
            messagebox.showerror("错误", "该记录可能已被删除")
            return

        rewrite_all_bills(new_records)
        self.refresh_table()
        messagebox.showinfo("成功", "已删除该账单")

    def destroy(self):
        """切换页面时销毁当前界面"""
        self.frame.destroy()


# ====================================================================
# 5. 应用主控类（页面切换）
# ====================================================================

class App:
    """管理页面切换的顶层控制类"""
    def __init__(self, root):
        self.root = root
        self.page = None          # 当前页面对象
        self.goto_login()

    def goto_login(self):
        """切换到登录页"""
        if self.page:
            self.page.destroy()
        self.page = LoginPage(self.root, self.goto_register, self.goto_main)

    def goto_register(self):
        """切换到注册页"""
        if self.page:
            self.page.destroy()
        self.page = RegisterPage(self.root, self.goto_login)

    def goto_main(self):
        """切换到账单管理主页面"""
        if self.page:
            self.page.destroy()
        self.page = BillMainPage(self.root)


# ====================================================================
# 6. 程序入口
# ====================================================================

if __name__ == "__main__":
    main_win = tk.Tk()
    App(main_win)
    main_win.mainloop()
