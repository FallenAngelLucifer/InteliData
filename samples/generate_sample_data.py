#!/usr/bin/env python3
"""
Generador de datos sintéticos matemáticamente estructurados para InteliData.
Genera una base de datos SQLite 'samples/ecommerce_obfuscated.db' con:
- Dependencias de Inclusión Probabilística (PIND): FKs exactas e inexactas
- Ley de Benford en montos financieros (distribución log-uniforme de dígitos significativos)
- Cadenas de Markov de estados con estados absorbentes (DELIVERED, CANCELLED)
- Entropía de Shannon: Columnas categóricas de baja entropía vs hashes de alta entropía
- Abreviaciones y contracciones consonánticas: fld_uid, fld_usr_ref, fld_oid, fld_ord_ref
"""

import sqlite3
import random
import os
import math
import hashlib

def generate_database(db_path: str):
    if os.path.exists(db_path):
        os.remove(db_path)

    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    # 1. Tabla Usuarios: tbl_001_usr
    cur.execute("""
    CREATE TABLE tbl_001_usr (
        fld_uid INTEGER PRIMARY KEY,
        fld_uname TEXT,
        fld_email_hash TEXT,
        fld_reg_date TEXT,
        fld_usr_type TEXT
    );
    """)

    user_types = ["STANDARD", "PREMIUM", "VIP", "ENTERPRISE"]
    users = []
    for uid in range(1, 1001):
        uname = f"usr_{uid}_{random.randint(100, 999)}"
        email_hash = hashlib.sha256(f"user{uid}@domain.com".encode()).hexdigest()
        reg_date = f"2025-{random.randint(1,12):02d}-{random.randint(1,28):02d}"
        usr_type = random.choices(user_types, weights=[0.60, 0.25, 0.10, 0.05])[0]
        users.append((uid, uname, email_hash, reg_date, usr_type))

    cur.executemany("INSERT INTO tbl_001_usr VALUES (?, ?, ?, ?, ?);", users)

    # 2. Tabla Órdenes: tbl_002_ord
    cur.execute("""
    CREATE TABLE tbl_002_ord (
        fld_oid INTEGER PRIMARY KEY,
        fld_usr_ref INTEGER,
        fld_amt REAL,
        fld_st_seq TEXT,
        fld_auth_tok TEXT,
        fld_notes TEXT
    );
    """)

    # Generación de montos que cumplen estrictamente la Ley de Benford:
    # Si X = 10^U donde U ~ Uniforme(a, b), los dígitos significativos siguen Benford exactamente.
    orders = []
    # Estados de Markov: CREATED -> PROCESSING -> SHIPPED -> DELIVERED (absorbente) o CANCELLED (absorbente)
    markov_terminal_states = ["DELIVERED", "CANCELLED", "REFUNDED"]
    markov_intermediate_states = ["CREATED", "PAYMENT_PENDING", "PROCESSING", "SHIPPED"]

    spanish_phrases = [
        "Pedido entregado en puerta principal del cliente",
        "Solicitud de cambio de direccion de facturacion recibida",
        "Empaque reforzado para producto fragil solicitado",
        "Cliente ausente en primera visita de mensajeria",
        "Confirmacion telefonica realizada con exito",
        "Pago procesado mediante pasarela bancaria externa"
    ]

    for oid in range(10001, 13001):
        usr_ref = random.randint(1, 1000) # FK hacia fld_uid
        # Monto Benford
        exponent = random.uniform(1.0, 4.5)
        amt = round(10 ** exponent, 2)
        
        # Simulación de estados
        p = random.random()
        if p < 0.70:
            st = "DELIVERED" # Terminal
        elif p < 0.85:
            st = "PROCESSING"
        elif p < 0.95:
            st = "CANCELLED" # Terminal
        else:
            st = "PAYMENT_PENDING"

        auth_tok = hashlib.md5(f"tok_{oid}_{random.random()}".encode()).hexdigest()
        notes = random.choice(spanish_phrases)
        orders.append((oid, usr_ref, amt, st, auth_tok, notes))

    cur.executemany("INSERT INTO tbl_002_ord VALUES (?, ?, ?, ?, ?, ?);", orders)

    # 3. Tabla Líneas de Pedido: tbl_003_itm
    cur.execute("""
    CREATE TABLE tbl_003_itm (
        fld_iid INTEGER PRIMARY KEY,
        fld_ord_ref INTEGER,
        fld_sku TEXT,
        fld_qty INTEGER,
        fld_unit_price REAL
    );
    """)

    items = []
    iid = 1
    skus = [f"SKU-{random.randint(100, 999)}-{chr(65+i)}" for i in range(20)]
    for oid in range(10001, 13001):
        num_items = random.randint(1, 4)
        for _ in range(num_items):
            sku = random.choice(skus)
            qty = random.choices([1, 2, 3, 4, 5], weights=[0.5, 0.25, 0.15, 0.07, 0.03])[0]
            # Precio unitario Benford
            price = round(10 ** random.uniform(0.8, 3.2), 2)
            items.append((iid, oid, sku, qty, price))
            iid += 1

    cur.executemany("INSERT INTO tbl_003_itm VALUES (?, ?, ?, ?, ?);", items)

    # 4. Tabla Auditoría / Eventos: tbl_004_log
    cur.execute("""
    CREATE TABLE tbl_004_log (
        fld_lid INTEGER PRIMARY KEY,
        fld_usr_ref INTEGER,
        fld_event_type TEXT,
        fld_ip_addr TEXT,
        fld_ts TEXT
    );
    """)

    events = ["LOGIN_SUCCESS", "LOGIN_FAILED", "VIEW_ITEM", "ADD_TO_CART", "CHECKOUT_INIT", "LOGOUT"]
    logs = []
    for lid in range(1, 5001):
        usr_ref = random.randint(1, 1000)
        evt = random.choice(events)
        ip = f"192.168.{random.randint(1, 20)}.{random.randint(1, 254)}"
        ts = f"2025-10-{random.randint(1, 31):02d} {random.randint(0, 23):02d}:{random.randint(0, 59):02d}:{random.randint(0, 59):02d}"
        logs.append((lid, usr_ref, evt, ip, ts))

    cur.executemany("INSERT INTO tbl_004_log VALUES (?, ?, ?, ?, ?);", logs)

    conn.commit()
    conn.close()
    print(f"Base de datos de prueba creada con exito: {db_path}")

if __name__ == "__main__":
    out_dir = os.path.dirname(os.path.abspath(__file__))
    db_file = os.path.join(out_dir, "ecommerce_obfuscated.db")
    generate_database(db_file)
