import os
import psycopg2
import pickle
import traceback
from game_logic import Game

DATABASE_URL = os.getenv("DATABASE_URL") or os.getenv("POSTGRES_URL")

def get_db_connection():
    if not DATABASE_URL:
        print("DATABASE_URL not found in environment.")
        return None
    try:
        # Neon often needs sslmode=require which should be in the URL,
        # but we can also force it here if needed.
        conn = psycopg2.connect(DATABASE_URL)
        return conn
    except Exception as e:
        print(f"Error connecting to database: {e}")
        traceback.print_exc()
        return None

def init_db():
    print("Initializing database...")
    conn = get_db_connection()
    if not conn:
        print("Warning: Could not connect to DB. State will not be persisted.")
        return
    
    try:
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
                print("Creating initial game state...")
                initial_game = Game()
                cur.execute("INSERT INTO game_state (id, data) VALUES (%s, %s)", 
                            (1, pickle.dumps(initial_game)))
        conn.commit()
        print("Database initialized successfully.")
    except Exception as e:
        print(f"Error during init_db: {e}")
        traceback.print_exc()
    finally:
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
                print("No game state found in DB, creating new.")
                game = Game()
                # We can't easily call save_game here without recursing or opening another conn, 
                # so we just return the new game. The first save will create it.
                return game
    except Exception as e:
        print(f"Error loading game: {e}")
        traceback.print_exc()
        if not hasattr(load_game, "_memory_game"):
            load_game._memory_game = Game()
        return load_game._memory_game
    finally:
        conn.close()

def save_game(game: Game):
    conn = get_db_connection()
    if not conn:
        load_game._memory_game = game
        return
    
    try:
        with conn.cursor() as cur:
            cur.execute("UPDATE game_state SET data = %s WHERE id = 1", (pickle.dumps(game),))
            if cur.rowcount == 0:
                cur.execute("INSERT INTO game_state (id, data) VALUES (%s, %s)", (1, pickle.dumps(game)))
        conn.commit()
    except Exception as e:
        print(f"Error saving game: {e}")
        traceback.print_exc()
    finally:
        conn.close()
