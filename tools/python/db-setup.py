import csv
import psycopg
from psycopg.rows import dict_row


__LINES_CSV = "Train Station Data - Route.csv"
__STATIONS_CSV = "Train Station Data - Station.csv"


def get_connection():
    __DB_CONFIG = {
        "host": "localhost",
        "port": "5432",
        "dbname": "jrhyaku",
        "user": "developer"
    }

    return psycopg.connect(**__DB_CONFIG, row_factory=dict_row)


def insert_lines(conn):
    with open(__LINES_CSV, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = [
            (int(row["line_cd"]), row["line_name"])
            for row in reader
        ]
 
    inserted = 0
    skipped = 0
    with conn.cursor() as cur:
        for line_cd, line_name in rows:
            cur.execute(
                """
                INSERT INTO lines (line_cd, line_name)
                VALUES (%s, %s)
                ON CONFLICT (line_cd) DO NOTHING
                RETURNING pk
                """,
                (line_cd, line_name),
            )
            if cur.fetchone() is None:
                print(f"  Skipped duplicate line: line_cd={line_cd} ({line_name})")
                skipped += 1
            else:
                inserted += 1
 
    conn.commit()
    print(f"Inserted {inserted} lines ({skipped} duplicates skipped).")
 
 
def insert_stations(conn):
    with open(__STATIONS_CSV, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = [
            (
                int(row["station_cd"]),
                row["station_name"],
                float(row["lon"]),
                float(row["lat"]),
                int(row["line_cd"]),
            )
            for row in reader
        ]
 
    inserted = 0
    skipped = 0
    with conn.cursor() as cur:
        for station_cd, station_name, lon, lat, line_cd in rows:
            cur.execute(
                """
                INSERT INTO stations (station_cd, station_name, lon, lat, line_cd)
                VALUES (%s, %s, %s, %s, %s)
                ON CONFLICT (station_cd) DO NOTHING
                RETURNING pk
                """,
                (station_cd, station_name, lon, lat, line_cd),
            )
            if cur.fetchone() is None:
                print(f"  Skipped duplicate station: station_cd={station_cd} ({station_name})")
                skipped += 1
            else:
                inserted += 1
 
    conn.commit()
    print(f"Inserted {inserted} stations ({skipped} duplicates skipped).")


def main():
    with get_connection() as conn:
        insert_lines(conn)
        insert_stations(conn)


if __name__ == '__main__':
    main()
