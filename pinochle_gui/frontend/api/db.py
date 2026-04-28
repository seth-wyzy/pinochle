import os
import psycopg2
import pickle
from game_logic import Game

DATABASE_URL = os.getenv("DATABASE_URL") or os.getenv("POSTGRES_URL")

def get_db_connection():
    if not DATABASE_URL:
        # Fallback for local development if DATABASE_URL is not set
        # Note: In production on Vercel, this MUST be set.
        return None
    return psycopg2.connect(DATABASE_URL)

def init_db():
    conn = get_db_connection()
    if not conn:
        print("Warning: DATABASE_URL not set. State will not be persisted.")
        return
    
    with conn.cursor() as cur:
        cur.execute("""
            CREATE TABLE IF NOT EXISTS game_state (
                id INTEGER PRIMARY KEY,
                data BYTEA
            )
        """)
        # Ensure initial row exists
        cur.execute("SELECT id FROM game_state WHERE id = 1")
        if not cur.fetchone():
            initial_game = Game()
            cur.execute("INSERT INTO game_state (id, data) VALUES (%s, %s)", 
                        (1, pickle.dumps(initial_game)))
    conn.commit()
    conn.close()

def load_game() -> Game:
    conn = get_db_connection()
    if not conn:
        # Fallback to in-memory if no DB
        if not hasattr(load_game, "_memory_game"):
            load_game._memory_game = Game()
        return load_game._memory_game
    
    try:
        with conn.cursor() as cur:
            cur.execute("SELECT data FROM game_state WHERE id = 1")
            row = cur.fetchone()
            if row:
                return pickle.loads(row[0])
            else:
                game = Game()
                save_game(game)
                return game
    finally:
        conn.close()

def save_game(game: Game):
    conn = get_db_connection()
    if not conn:
        # Fallback to in-memory if no DB
        load_game._memory_game = game
        return
    
    try:
        with conn.cursor() as cur:
            cur.execute("UPDATE game_state SET data = %s WHERE id = 1", (pickle.dumps(game),))
            if cur.rowcount == 0:
                cur.execute("INSERT INTO game_state (id, data) VALUES (%s, %s)", (1, pickle.dumps(game)))
        conn.commit()
    finally:
        conn.close()
