#!/usr/bin/env python3
"""
generate_data.py — Generates TPC-H-like data for NanoDB
Produces: data/customer.tbl, data/orders.tbl, data/lineitem.tbl
Scale: 20,000 customers | 30,000 orders | 50,000 lineitems

Usage:  python3 scripts/generate_data.py
"""

import random
import os
import time

random.seed(42)

# ── Config ─────────────────────────────────────────────────────────────────
NUM_CUSTOMERS = 20_000
NUM_ORDERS    = 30_000
NUM_LINEITEMS = 50_000
DATA_DIR      = "data"

os.makedirs(DATA_DIR, exist_ok=True)

# ── Lookup tables ──────────────────────────────────────────────────────────
SEGMENTS   = ["AUTOMOBILE", "BUILDING", "FURNITURE", "HOUSEHOLD", "MACHINERY"]
STATUSES   = ["O", "F", "P"]
PRIORITIES = ["1-URGENT", "2-HIGH", "3-MEDIUM", "4-NOT SPECIFIED", "5-LOW"]
RETURN_FLAGS = ["N", "R", "A"]
LINE_STATUS  = ["O", "F"]

def rand_name():
    first = ["Alpha","Beta","Gamma","Delta","Epsilon","Zeta","Eta","Theta","Iota","Kappa",
             "Lambda","Mu","Nu","Xi","Omicron","Pi","Rho","Sigma","Tau","Upsilon"]
    last  = ["Corp","Ltd","Inc","Co","LLC","Group","Partners","Ventures","Industries","Holdings"]
    return f"Customer {random.choice(first)} {random.choice(last)}"

def rand_address():
    streets = ["Main St","Oak Ave","Pine Rd","Elm Blvd","Cedar Ln","Maple Dr","Birch Way","Ash Ct"]
    return f"{random.randint(1,9999)} {random.choice(streets)}"

def rand_phone():
    return f"{random.randint(100,999)}-{random.randint(100,999)}-{random.randint(1000,9999)}"

def rand_date(year_min=1992, year_max=1998):
    y = random.randint(year_min, year_max)
    m = random.randint(1, 12)
    d = random.randint(1, 28)
    return f"{y:04d}-{m:02d}-{d:02d}"

def rand_comment(maxlen=60):
    words = ["quick","foxes","sleep","regular","final","pending","express","special",
             "silent","careful","pending","blithely","furiously","ironically","slyly"]
    return " ".join(random.choices(words, k=random.randint(3,8)))[:maxlen]

def rand_clerk():
    return f"Clerk#{random.randint(1,1000):04d}"

# ── Generate customers ─────────────────────────────────────────────────────
print(f"Generating {NUM_CUSTOMERS} customers...")
t0 = time.time()

with open(f"{DATA_DIR}/customer.tbl", "w") as f:
    for i in range(1, NUM_CUSTOMERS + 1):
        custkey    = i
        name       = rand_name()
        address    = rand_address()
        nationkey  = random.randint(0, 24)
        phone      = rand_phone()
        acctbal    = round(random.uniform(-999.99, 9999.99), 2)
        mktsegment = random.choice(SEGMENTS)
        comment    = rand_comment()
        f.write(f"{custkey}|{name}|{address}|{nationkey}|{phone}|{acctbal:.2f}|{mktsegment}|{comment}\n")

print(f"  Done in {time.time()-t0:.1f}s → data/customer.tbl")

# ── Generate orders ────────────────────────────────────────────────────────
print(f"Generating {NUM_ORDERS} orders...")
t0 = time.time()

customer_keys = list(range(1, NUM_CUSTOMERS + 1))
order_keys    = list(range(1, NUM_ORDERS + 1))

with open(f"{DATA_DIR}/orders.tbl", "w") as f:
    for i in range(1, NUM_ORDERS + 1):
        orderkey     = i
        custkey      = random.choice(customer_keys)
        orderstatus  = random.choice(STATUSES)
        totalprice   = round(random.uniform(1000.0, 500000.0), 2)
        orderdate    = rand_date()
        orderpriority = random.choice(PRIORITIES)
        clerk        = rand_clerk()
        shippriority = 0
        comment      = rand_comment()
        f.write(f"{orderkey}|{custkey}|{orderstatus}|{totalprice:.2f}|{orderdate}|{orderpriority}|{clerk}|{shippriority}|{comment}\n")

print(f"  Done in {time.time()-t0:.1f}s → data/orders.tbl")

# ── Generate lineitems ─────────────────────────────────────────────────────
print(f"Generating {NUM_LINEITEMS} lineitems...")
t0 = time.time()

with open(f"{DATA_DIR}/lineitem.tbl", "w") as f:
    # Spread lineitems across orders
    per_order = NUM_LINEITEMS // NUM_ORDERS  # ~1-2 per order
    extra     = NUM_LINEITEMS - per_order * NUM_ORDERS
    linenum_global = 0

    for orderkey in order_keys:
        count = per_order + (1 if linenum_global < extra else 0)
        linenum_global += 1
        for ln in range(1, count + 1):
            partkey      = random.randint(1, 200_000)
            suppkey      = random.randint(1, 10_000)
            linenumber   = ln
            quantity     = round(random.uniform(1.0, 50.0), 2)
            extprice     = round(quantity * random.uniform(100.0, 10_000.0), 2)
            discount     = round(random.uniform(0.0, 0.10), 2)
            tax          = round(random.uniform(0.02, 0.08), 2)
            returnflag   = random.choice(RETURN_FLAGS)
            linestatus   = random.choice(LINE_STATUS)
            shipdate     = rand_date(1992, 1998)
            f.write(f"{orderkey}|{partkey}|{suppkey}|{linenumber}|{quantity:.2f}|{extprice:.2f}|{discount:.2f}|{tax:.2f}|{returnflag}|{linestatus}|{shipdate}\n")

print(f"  Done in {time.time()-t0:.1f}s → data/lineitem.tbl")

# ── Summary ────────────────────────────────────────────────────────────────
cust_size  = os.path.getsize(f"{DATA_DIR}/customer.tbl")
ord_size   = os.path.getsize(f"{DATA_DIR}/orders.tbl")
li_size    = os.path.getsize(f"{DATA_DIR}/lineitem.tbl")
total_mb   = (cust_size + ord_size + li_size) / (1024*1024)

print(f"\n✓ Data generation complete:")
print(f"  customer.tbl  : {NUM_CUSTOMERS:>7,} rows  ({cust_size/1024:.1f} KB)")
print(f"  orders.tbl    : {NUM_ORDERS:>7,} rows  ({ord_size/1024:.1f} KB)")
print(f"  lineitem.tbl  : {NUM_LINEITEMS:>7,} rows  ({li_size/1024:.1f} KB)")
print(f"  Total         : {NUM_CUSTOMERS+NUM_ORDERS+NUM_LINEITEMS:>7,} rows  ({total_mb:.1f} MB)")
print(f"\nNow run:  make && ./nanodb")
